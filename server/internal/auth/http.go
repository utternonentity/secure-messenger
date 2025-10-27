package auth

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"strings"

	"github.com/utternonentity/secure-messenger/server/internal/identity"
)

type httpServer struct {
	identities *identity.Manager
}

type registerRequest struct {
	Nickname string `json:"nickname"`
}

type registerResponse struct {
	UserID      string `json:"user_id"`
	DisplayName string `json:"display_name"`
}

// NewHTTPHandler exposes a minimal endpoint for registering users by nickname.
func NewHTTPHandler(manager *identity.Manager) (http.Handler, error) {
	if manager == nil {
		return nil, errors.New("auth: http handler requires an identity manager")
	}
	server := &httpServer{identities: manager}
	mux := http.NewServeMux()
	mux.HandleFunc("/api/auth/register", server.handleRegister)
	return mux, nil
}

func (s *httpServer) handleRegister(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	defer r.Body.Close()

	var payload registerRequest
	if err := json.NewDecoder(r.Body).Decode(&payload); err != nil {
		http.Error(w, "invalid json payload", http.StatusBadRequest)
		return
	}
	nickname := strings.TrimSpace(payload.Nickname)
	if nickname == "" {
		http.Error(w, "nickname is required", http.StatusBadRequest)
		return
	}

	profile, err := s.identities.RegisterUser(r.Context(), nickname)
	if err != nil {
		switch {
		case errors.Is(err, context.Canceled):
			http.Error(w, http.StatusText(http.StatusRequestTimeout), http.StatusRequestTimeout)
		case errors.Is(err, context.DeadlineExceeded):
			http.Error(w, http.StatusText(http.StatusGatewayTimeout), http.StatusGatewayTimeout)
		case errors.Is(err, identity.ErrInvalidDisplayName):
			http.Error(w, err.Error(), http.StatusBadRequest)
		case errors.Is(err, identity.ErrDisplayNameTaken):
			http.Error(w, err.Error(), http.StatusConflict)
		default:
			http.Error(w, http.StatusText(http.StatusInternalServerError), http.StatusInternalServerError)
		}
		return
	}

	resp := registerResponse{
		UserID:      profile.UserID,
		DisplayName: profile.DisplayName,
	}
	writeJSON(w, http.StatusCreated, resp)
}

func writeJSON(w http.ResponseWriter, status int, payload any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	encoder := json.NewEncoder(w)
	encoder.SetIndent("", "  ")
	_ = encoder.Encode(payload)
}
