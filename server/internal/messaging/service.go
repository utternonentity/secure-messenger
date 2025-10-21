package messaging

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"strings"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
)

const serverMsgIDPrefix = "msg-"

// Service implements the Messaging gRPC service backed by a persistent envelope repository.
type Service struct {
	smv1.UnimplementedMessagingServer
	store envelopeRepository
}

// NewService constructs a Messaging service that stores envelopes in the provided repository.
func NewService(store envelopeRepository) (*Service, error) {
	if store == nil {
		return nil, fmt.Errorf("store must not be nil")
	}
	return &Service{store: store}, nil
}

// Send persists the encrypted envelope and returns the assigned server-side identifier.
func (s *Service) Send(ctx context.Context, env *smv1.EncryptedEnvelope) (*smv1.SendResponse, error) {
	if env == nil {
		return nil, status.Error(codes.InvalidArgument, "envelope must not be nil")
	}

	id, err := s.store.Save(ctx, env)
	if err != nil {
		if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
			return nil, status.FromContextError(err).Err()
		}
		return nil, status.Errorf(codes.Internal, "store envelope: %v", err)
	}

	return &smv1.SendResponse{ServerMsgId: formatServerMsgID(id)}, nil
}

// Pull streams envelopes with server identifiers greater than the provided marker.
func (s *Service) Pull(req *smv1.PullRequest, stream smv1.Messaging_PullServer) error {
	sinceID, err := parseServerMsgID(req.GetSinceServerMsgId())
	if err != nil {
		return status.Errorf(codes.InvalidArgument, "invalid since_server_msg_id: %v", err)
	}

	sendErr := s.store.ForEachSince(stream.Context(), sinceID, func(rec StoredEnvelope) error {
		return stream.Send(rec.Envelope)
	})
	if sendErr != nil {
		if errors.Is(sendErr, context.Canceled) || errors.Is(sendErr, context.DeadlineExceeded) {
			return status.FromContextError(sendErr).Err()
		}
		if st, ok := status.FromError(sendErr); ok {
			return st.Err()
		}
		return status.Errorf(codes.Internal, "deliver messages: %v", sendErr)
	}

	return nil
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
