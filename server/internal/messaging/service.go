package messaging

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"strings"
	"sync"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"

	smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
)

const serverMsgIDPrefix = "msg-"

// Service implements the Messaging gRPC service backed by a persistent envelope repository.
type Service struct {
	smv1.UnimplementedMessagingServer
	store  envelopeRepository
	subsMu sync.RWMutex
	subs   map[*subscription]struct{}
}

// NewService constructs a Messaging service that stores envelopes in the provided repository.
func NewService(store envelopeRepository) (*Service, error) {
	if store == nil {
		return nil, fmt.Errorf("store must not be nil")
	}
	return &Service{store: store, subs: make(map[*subscription]struct{})}, nil
}

// Send persists the encrypted envelope and returns the assigned server-side identifier.
func (s *Service) Send(ctx context.Context, env *smv1.EncryptedEnvelope) (*smv1.SendResponse, error) {
	if env == nil {
		return nil, status.Error(codes.InvalidArgument, "envelope must not be nil")
	}
	meta := env.GetMeta()
	if meta == nil || strings.TrimSpace(meta.GetConversationId()) == "" {
		return nil, status.Error(codes.InvalidArgument, "conversation_id must be provided")
	}

	id, err := s.store.Save(ctx, env)
	if err != nil {
		if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
			return nil, status.FromContextError(err).Err()
		}
		return nil, status.Errorf(codes.Internal, "store envelope: %v", err)
	}

	s.broadcast(StoredEnvelope{ID: id, Envelope: proto.Clone(env).(*smv1.EncryptedEnvelope)})

	return &smv1.SendResponse{ServerMsgId: formatServerMsgID(id)}, nil
}

// Pull streams envelopes with server identifiers greater than the provided marker.
func (s *Service) Pull(req *smv1.PullRequest, stream smv1.Messaging_PullServer) error {
	sinceID, err := parseServerMsgID(req.GetSinceServerMsgId())
	if err != nil {
		return status.Errorf(codes.InvalidArgument, "invalid since_server_msg_id: %v", err)
	}
	allowed, err := conversationFilter(req.GetConversationIds())
	if err != nil {
		return status.Errorf(codes.InvalidArgument, "conversation_ids: %v", err)
	}

	sub := newSubscription(stream.Context(), allowed)
	s.addSubscription(sub)
	defer s.removeSubscription(sub)

	lastSent := sinceID
	deliver := func(rec StoredEnvelope) error {
		if !sub.matches(conversationIDOf(rec.Envelope)) {
			return nil
		}
		if err := stream.Send(rec.Envelope); err != nil {
			return err
		}
		if rec.ID > lastSent {
			lastSent = rec.ID
		}
		return nil
	}

	if err := s.store.ForEachSince(stream.Context(), sinceID, deliver); err != nil {
		if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
			return status.FromContextError(err).Err()
		}
		if st, ok := status.FromError(err); ok {
			return st.Err()
		}
		return status.Errorf(codes.Internal, "deliver messages: %v", err)
	}

	for {
		select {
		case <-stream.Context().Done():
			return status.FromContextError(stream.Context().Err()).Err()
		case rec, ok := <-sub.ch:
			if !ok {
				return nil
			}
			if rec.ID <= lastSent {
				continue
			}
			if !sub.matches(conversationIDOf(rec.Envelope)) {
				continue
			}
			if err := stream.Send(rec.Envelope); err != nil {
				if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
					return status.FromContextError(err).Err()
				}
				if st, ok := status.FromError(err); ok {
					return st.Err()
				}
				return status.Errorf(codes.Internal, "deliver message: %v", err)
			}
			if rec.ID > lastSent {
				lastSent = rec.ID
			}
		}
	}
}

type subscription struct {
	ctx    context.Context
	ch     chan StoredEnvelope
	filter map[string]struct{}
}

func newSubscription(ctx context.Context, filter map[string]struct{}) *subscription {
	return &subscription{
		ctx:    ctx,
		ch:     make(chan StoredEnvelope, 64),
		filter: filter,
	}
}

func (s *subscription) matches(conversationID string) bool {
	if len(s.filter) == 0 {
		return false
	}
	_, ok := s.filter[conversationID]
	return ok
}

func (svc *Service) addSubscription(sub *subscription) {
	svc.subsMu.Lock()
	svc.subs[sub] = struct{}{}
	svc.subsMu.Unlock()
}

func (svc *Service) removeSubscription(sub *subscription) {
	svc.subsMu.Lock()
	if _, ok := svc.subs[sub]; ok {
		delete(svc.subs, sub)
		close(sub.ch)
	}
	svc.subsMu.Unlock()
}

func (svc *Service) broadcast(rec StoredEnvelope) {
	svc.subsMu.RLock()
	subs := make([]*subscription, 0, len(svc.subs))
	for sub := range svc.subs {
		subs = append(subs, sub)
	}
	svc.subsMu.RUnlock()

	convID := conversationIDOf(rec.Envelope)
	for _, sub := range subs {
		if !sub.matches(convID) {
			continue
		}
		msg := StoredEnvelope{ID: rec.ID, Envelope: proto.Clone(rec.Envelope).(*smv1.EncryptedEnvelope)}
		select {
		case sub.ch <- msg:
		case <-sub.ctx.Done():
		default:
			go func(s *subscription, env StoredEnvelope) {
				select {
				case s.ch <- env:
				case <-s.ctx.Done():
				}
			}(sub, msg)
		}
	}
}

func conversationFilter(ids []string) (map[string]struct{}, error) {
	filter := make(map[string]struct{})
	for _, id := range ids {
		id = strings.TrimSpace(id)
		if id == "" {
			continue
		}
		filter[id] = struct{}{}
	}
	if len(filter) == 0 {
		return nil, fmt.Errorf("at least one conversation id is required")
	}
	return filter, nil
}

func conversationIDOf(env *smv1.EncryptedEnvelope) string {
	if env == nil {
		return ""
	}
	meta := env.GetMeta()
	if meta == nil {
		return ""
	}
	return meta.GetConversationId()
}

func formatServerMsgID(id int64) string {
	return fmt.Sprintf("%s%d", serverMsgIDPrefix, id)
}

func parseServerMsgID(value string) (int64, error) {
	value = strings.TrimSpace(value)
	if value == "" {
		return 0, nil
	}
	if !strings.HasPrefix(value, serverMsgIDPrefix) {
		return 0, fmt.Errorf("unexpected prefix")
	}

	numeric := value[len(serverMsgIDPrefix):]
	if numeric == "" {
		return 0, fmt.Errorf("missing identifier body")
	}
	id, err := strconv.ParseInt(numeric, 10, 64)
	if err != nil {
		return 0, fmt.Errorf("parse identifier: %w", err)
	}
	if id < 0 {
		return 0, fmt.Errorf("identifier must be non-negative")
	}
	return id, nil
}
