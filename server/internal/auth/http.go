package auth

import (
	"context"
	"encoding/base64"
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
	Nickname    string `json:"nickname"`
	Certificate string `json:"certificate"`
}

type registerResponse struct {
	UserID   string `json:"user_id"`
	Nickname string `json:"nickname"`
}

type listUsersResponse struct {
	Users []userProfileResponse `json:"users"`
}

type userProfileResponse struct {
	UserID      string   `json:"user_id"`
	Nickname    string   `json:"nickname"`
	Roles       []string `json:"roles"`
	Certificate string   `json:"certificate"`
}

// NewHTTPHandler exposes a minimal endpoint for registering users by nickname.
func NewHTTPHandler(manager *identity.Manager) (http.Handler, error) {
	if manager == nil {
		return nil, errors.New("auth: http handler requires an identity manager")
	}
	server := &httpServer{identities: manager}
	mux := http.NewServeMux()
	mux.HandleFunc("/api/auth/register", server.handleRegister)
	mux.HandleFunc("/api/auth/users", server.handleListUsers)
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
	certB64 := strings.TrimSpace(payload.Certificate)
	if nickname == "" {
		http.Error(w, "nickname is required", http.StatusBadRequest)
		return
	}
	if certB64 == "" {
		http.Error(w, "certificate is required", http.StatusBadRequest)
		return
	}
	certDER, err := base64.StdEncoding.DecodeString(certB64)
	if err != nil {
		http.Error(w, "certificate must be base64 encoded", http.StatusBadRequest)
		return
	}

	profile, err := s.identities.RegisterUser(r.Context(), nickname, certDER)
	if err != nil {
		switch {
		case errors.Is(err, context.Canceled):
			http.Error(w, http.StatusText(http.StatusRequestTimeout), http.StatusRequestTimeout)
		case errors.Is(err, context.DeadlineExceeded):
			http.Error(w, http.StatusText(http.StatusGatewayTimeout), http.StatusGatewayTimeout)
		case errors.Is(err, identity.ErrInvalidNickname):
			http.Error(w, err.Error(), http.StatusBadRequest)
		case errors.Is(err, identity.ErrNicknameTaken):
			http.Error(w, err.Error(), http.StatusConflict)
		case errors.Is(err, identity.ErrInvalidCertificate):
			http.Error(w, err.Error(), http.StatusBadRequest)
		case errors.Is(err, identity.ErrCertificateAlreadyAssigned):
			http.Error(w, err.Error(), http.StatusConflict)
		case errors.Is(err, identity.ErrCertificateMismatch):
			http.Error(w, err.Error(), http.StatusConflict)
		default:
			http.Error(w, http.StatusText(http.StatusInternalServerError), http.StatusInternalServerError)
		}
		return
	}

	resp := registerResponse{
		UserID:   profile.UserID,
		Nickname: profile.Nickname,
	}
	writeJSON(w, http.StatusCreated, resp)
}

func (s *httpServer) handleListUsers(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}

	profiles, err := s.identities.ListProfiles(r.Context())
	if err != nil {
		switch {
		case errors.Is(err, context.Canceled):
			http.Error(w, http.StatusText(http.StatusRequestTimeout), http.StatusRequestTimeout)
		case errors.Is(err, context.DeadlineExceeded):
			http.Error(w, http.StatusText(http.StatusGatewayTimeout), http.StatusGatewayTimeout)
		default:
			http.Error(w, http.StatusText(http.StatusInternalServerError), http.StatusInternalServerError)
		}
		return
	}

	resp := listUsersResponse{Users: make([]userProfileResponse, 0, len(profiles))}
	for _, profile := range profiles {
		resp.Users = append(resp.Users, convertIdentityProfile(profile))
	}
	writeJSON(w, http.StatusOK, resp)
}

func convertIdentityProfile(profile identity.Profile) userProfileResponse {
	cert := ""
	if len(profile.CertDER) > 0 {
		cert = base64.StdEncoding.EncodeToString(profile.CertDER)
	}
	return userProfileResponse{
		UserID:      profile.UserID,
		Nickname:    profile.Nickname,
		Roles:       append([]string(nil), profile.Roles...),
		Certificate: cert,
	}
}

func writeJSON(w http.ResponseWriter, status int, payload any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	encoder := json.NewEncoder(w)
	encoder.SetIndent("", "  ")
	_ = encoder.Encode(payload)
}
