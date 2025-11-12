package auth

import (
	"crypto/rand"
	"encoding/base64"
	"errors"
	"io"
	"strings"
	"sync"
	"time"

	"github.com/utternonentity/secure-messenger/server/internal/identity"
)

var (
	// ErrInvalidToken indicates that the provided token is unknown or expired.
	ErrInvalidToken = errors.New("auth: token is invalid or expired")
	// ErrInvalidAuthorizationHeader indicates a malformed Authorization header.
	ErrInvalidAuthorizationHeader = errors.New("auth: invalid authorization header")
)

const bearerPrefix = "Bearer "

// TokenValidator defines the behaviour required to validate issued tokens.
type TokenValidator interface {
	ValidateToken(token string) (identity.Identity, time.Time, error)
}

type tokenEntry struct {
	identity identity.Identity
	expires  time.Time
}

// TokenManager issues and validates bearer tokens for HTTP APIs.
type TokenManager struct {
	mu     sync.RWMutex
	tokens map[string]tokenEntry
	ttl    time.Duration
}

// NewTokenManager creates a TokenManager with the provided token lifetime.
func NewTokenManager(ttl time.Duration) *TokenManager {
	if ttl <= 0 {
		ttl = 30 * time.Minute
	}
	return &TokenManager{
		tokens: make(map[string]tokenEntry),
		ttl:    ttl,
	}
}

// IssueToken registers a new session token for the given identity.
func (m *TokenManager) IssueToken(ident identity.Identity) (string, time.Time, error) {
	token, err := randomToken(32)
	if err != nil {
		return "", time.Time{}, err
	}
	expires := time.Now().Add(m.ttl)

	m.mu.Lock()
	m.tokens[token] = tokenEntry{identity: ident, expires: expires}
	m.mu.Unlock()

	return token, expires, nil
}

// ValidateToken returns the identity attached to the token if it is still valid.
func (m *TokenManager) ValidateToken(token string) (identity.Identity, time.Time, error) {
	token = strings.TrimSpace(token)
	if token == "" {
		return identity.Identity{}, time.Time{}, ErrInvalidToken
	}

	m.mu.RLock()
	entry, ok := m.tokens[token]
	m.mu.RUnlock()
	if !ok {
		return identity.Identity{}, time.Time{}, ErrInvalidToken
	}

	if time.Now().After(entry.expires) {
		m.mu.Lock()
		delete(m.tokens, token)
		m.mu.Unlock()
		return identity.Identity{}, time.Time{}, ErrInvalidToken
	}

	return entry.identity, entry.expires, nil
}

// Revoke removes a token from the manager.
func (m *TokenManager) Revoke(token string) {
	m.mu.Lock()
	delete(m.tokens, strings.TrimSpace(token))
	m.mu.Unlock()
}

func randomToken(size int) (string, error) {
	buf := make([]byte, size)
	if _, err := io.ReadFull(rand.Reader, buf); err != nil {
		return "", err
	}
	return base64.RawURLEncoding.EncodeToString(buf), nil
}

// ParseBearerToken extracts the bearer token from an Authorization header.
func ParseBearerToken(header string) (string, error) {
	header = strings.TrimSpace(header)
	if header == "" {
		return "", ErrInvalidAuthorizationHeader
	}
	if !strings.HasPrefix(strings.ToLower(header), strings.ToLower(bearerPrefix)) {
		return "", ErrInvalidAuthorizationHeader
	}
	token := strings.TrimSpace(header[len(bearerPrefix):])
	if token == "" {
		return "", ErrInvalidAuthorizationHeader
	}
	return token, nil
}
