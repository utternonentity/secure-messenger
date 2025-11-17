package messaging

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strings"
	"time"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"github.com/utternonentity/secure-messenger/server/internal/auth"
	smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
	"github.com/utternonentity/secure-messenger/server/internal/identity"
)

type httpServer struct {
	svc    *Service
	tokens auth.TokenValidator
	cipher EnvelopeCipher
}

type httpMessage struct {
	ServerMsgID     string `json:"server_msg_id"`
	ConversationID  string `json:"conversation_id"`
	SenderUserID    string `json:"sender_user_id"`
	SentUnixSeconds int64  `json:"sent_unix_sec"`
	Text            string `json:"text"`
}

type listResponse struct {
	Messages        []httpMessage `json:"messages"`
	LastServerMsgID string        `json:"last_server_msg_id,omitempty"`
}

type sendRequest struct {
	ConversationID string `json:"conversation_id"`
	SenderUserID   string `json:"sender_user_id"`
	Text           string `json:"text"`
}

type sendResponse struct {
	ServerMsgID    string `json:"server_msg_id"`
	ConversationID string `json:"conversation_id"`
	SenderUserID   string `json:"sender_user_id"`
	SentUnixSec    int64  `json:"sent_unix_sec"`
	Text           string `json:"text"`
}

// NewHTTPHandler exposes a minimal JSON API for history sync and message publishing.
func NewHTTPHandler(svc *Service, tokens auth.TokenValidator, cipher EnvelopeCipher) (http.Handler, error) {
	if svc == nil {
		return nil, errors.New("messaging: http handler requires a service instance")
	}
	if tokens == nil {
		return nil, errors.New("messaging: http handler requires a token validator")
	}
	if cipher == nil {
		return nil, errors.New("messaging: http handler requires an envelope cipher")
	}
	server := &httpServer{svc: svc, tokens: tokens, cipher: cipher}
	mux := http.NewServeMux()
	mux.HandleFunc("/api/messages", server.handleMessages)
	mux.HandleFunc("/healthz", server.handleHealth)
	return mux, nil
}

func (s *httpServer) handleHealth(w http.ResponseWriter, _ *http.Request) {
	w.WriteHeader(http.StatusOK)
	_, _ = w.Write([]byte("ok"))
}

func (s *httpServer) handleMessages(w http.ResponseWriter, r *http.Request) {
	ident, status, err := s.authenticate(r)
	if err != nil {
		http.Error(w, http.StatusText(status), status)
		return
	}

	switch r.Method {
	case http.MethodGet:
		s.handleList(w, r, ident)
	case http.MethodPost:
		s.handleSend(w, r, ident)
	default:
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
	}
}

func (s *httpServer) handleList(w http.ResponseWriter, r *http.Request, _ identity.Identity) {
	sinceParam := strings.TrimSpace(r.URL.Query().Get("since_id"))
	convParam := strings.TrimSpace(r.URL.Query().Get("conversation_id"))

	sinceID, err := parseServerMsgID(sinceParam)
	if err != nil {
		http.Error(w, "invalid since_id", http.StatusBadRequest)
		return
	}

	var messages []httpMessage
	var lastID int64
	collect := func(rec StoredEnvelope) error {
		if convParam != "" && conversationIDOf(rec.Envelope) != convParam {
			return nil
		}
		plaintext, err := s.cipher.Decrypt(rec.Envelope.GetCiphertext())
		if err != nil {
			return fmt.Errorf("decrypt message %d: %w", rec.ID, err)
		}
		msg := httpMessage{
			ServerMsgID:     formatServerMsgID(rec.ID),
			ConversationID:  conversationIDOf(rec.Envelope),
			SenderUserID:    senderUserIDOf(rec.Envelope),
			SentUnixSeconds: sentUnixOf(rec.Envelope),
			Text:            string(plaintext),
		}
		messages = append(messages, msg)
		if rec.ID > lastID {
			lastID = rec.ID
		}
		return nil
	}

	if err := s.svc.store.ForEachSince(r.Context(), sinceID, collect); err != nil {
		statusCode := httpStatusFromError(err)
		http.Error(w, http.StatusText(statusCode), statusCode)
		return
	}

	resp := listResponse{Messages: messages}
	if lastID > 0 {
		resp.LastServerMsgID = formatServerMsgID(lastID)
	}
	writeJSON(w, resp)
}

func (s *httpServer) handleSend(w http.ResponseWriter, r *http.Request, ident identity.Identity) {
	var payload sendRequest
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid json payload", http.StatusBadRequest)
		return
	}
	payload.ConversationID = strings.TrimSpace(payload.ConversationID)
	payload.SenderUserID = strings.TrimSpace(payload.SenderUserID)
	payload.Text = strings.TrimSpace(payload.Text)

	if payload.ConversationID == "" || payload.Text == "" {
		http.Error(w, "conversation_id and text are required", http.StatusBadRequest)
		return
	}

	if payload.SenderUserID == "" {
		payload.SenderUserID = ident.UserID
	}
	if !strings.EqualFold(payload.SenderUserID, ident.UserID) {
		http.Error(w, "sender_user_id does not match authenticated user", http.StatusForbidden)
		return
	}

	ciphertext, err := s.cipher.Encrypt([]byte(payload.Text))
	if err != nil {
		http.Error(w, "failed to encrypt message", http.StatusInternalServerError)
		return
	}
	env := &smv1.EncryptedEnvelope{
		Meta: &smv1.EnvelopeMeta{
			ConversationId: payload.ConversationID,
			SenderUserId:   payload.SenderUserID,
			SentUnixSec:    time.Now().Unix(),
		},
		Ciphertext: ciphertext,
	}

	resp, err := s.svc.Send(r.Context(), env)
	if err != nil {
		statusCode := httpStatusFromError(err)
		http.Error(w, http.StatusText(statusCode), statusCode)
		return
	}

	writeJSON(w, sendResponse{
		ServerMsgID:    resp.GetServerMsgId(),
		ConversationID: payload.ConversationID,
		SenderUserID:   payload.SenderUserID,
		SentUnixSec:    env.GetMeta().GetSentUnixSec(),
		Text:           payload.Text,
	})
}

func (s *httpServer) authenticate(r *http.Request) (identity.Identity, int, error) {
	header := r.Header.Get("Authorization")
	token, err := auth.ParseBearerToken(header)
	if err != nil {
		return identity.Identity{}, http.StatusUnauthorized, err
	}
	ident, _, err := s.tokens.ValidateToken(token)
	if err != nil {
		if errors.Is(err, auth.ErrInvalidToken) {
			return identity.Identity{}, http.StatusUnauthorized, err
		}
		return identity.Identity{}, http.StatusInternalServerError, err
	}
	if strings.TrimSpace(ident.UserID) == "" {
		return identity.Identity{}, http.StatusForbidden, errors.New("messaging: token missing user id")
	}
	return ident, http.StatusOK, nil
}

func writeJSON(w http.ResponseWriter, payload any) {
	w.Header().Set("Content-Type", "application/json")
	encoder := json.NewEncoder(w)
	encoder.SetIndent("", "  ")
	_ = encoder.Encode(payload)
}

func httpStatusFromError(err error) int {
	if err == nil {
		return http.StatusOK
	}
	if errors.Is(err, context.Canceled) {
		return http.StatusRequestTimeout
	}
	if errors.Is(err, context.DeadlineExceeded) {
		return http.StatusGatewayTimeout
	}
	if st, ok := status.FromError(err); ok {
		switch st.Code() {
		case codes.InvalidArgument:
			return http.StatusBadRequest
		case codes.NotFound:
			return http.StatusNotFound
		case codes.PermissionDenied, codes.Unauthenticated:
			return http.StatusForbidden
		case codes.Unavailable:
			return http.StatusServiceUnavailable
		default:
			return http.StatusInternalServerError
		}
	}
	return http.StatusInternalServerError
}

func senderUserIDOf(env *smv1.EncryptedEnvelope) string {
	if env == nil {
		return ""
	}
	meta := env.GetMeta()
	if meta == nil {
		return ""
	}
	return meta.GetSenderUserId()
}

func sentUnixOf(env *smv1.EncryptedEnvelope) int64 {
	if env == nil {
		return 0
	}
	meta := env.GetMeta()
	if meta == nil {
		return 0
	}
	return meta.GetSentUnixSec()
}
