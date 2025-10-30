package identity

import (
	"bytes"
	"context"
	"crypto/x509"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
)

var (
	ErrInvalidCertificate         = errors.New("identity: certificate is invalid")
	ErrCertificateMismatch        = errors.New("identity: certificate not registered")
	ErrCertificateAlreadyAssigned = errors.New("identity: certificate already registered")
	ErrNicknameTaken              = errors.New("identity: nickname already taken")
	ErrInvalidNickname            = errors.New("identity: nickname must not be empty")
)

type Identity struct {
	UserID   string
	Nickname string
	Roles    []string
	CertDER  []byte
}

type Profile struct {
	UserID   string
	Nickname string
	Roles    []string
	CertDER  []byte
}

type storedUser struct {
	UserID   string   `json:"user_id"`
	Nickname string   `json:"nickname"`
	Roles    []string `json:"roles"`
	Password string   `json:"password,omitempty"`
	CertDER  []byte   `json:"cert_der"`
}

type storeFile struct {
	Users []storedUser `json:"users"`
}

type Manager struct {
	mu    sync.RWMutex
	path  string
	users map[string]storedUser
}

func NewManager(path string) (*Manager, error) {
	if strings.TrimSpace(path) == "" {
		return nil, fmt.Errorf("identity store path must not be empty")
	}
	mgr := &Manager{path: path, users: make(map[string]storedUser)}
	if err := mgr.load(); err != nil {
		return nil, err
	}
	return mgr, nil
}

func (m *Manager) Close() error { return nil }

func (m *Manager) load() error {
	data, err := os.ReadFile(m.path)
	if errors.Is(err, os.ErrNotExist) {
		return nil
	}
	if err != nil {
		return fmt.Errorf("read identity store: %w", err)
	}
	wrapper := storeFile{}
	if err := json.Unmarshal(data, &wrapper); err != nil {
		return fmt.Errorf("unmarshal identity store: %w", err)
	}
	for _, user := range wrapper.Users {
		m.users[user.UserID] = user
	}
	return nil
}

func (m *Manager) persistLocked() error {
	wrapper := storeFile{Users: make([]storedUser, 0, len(m.users))}
	for _, user := range m.users {
		wrapper.Users = append(wrapper.Users, user)
	}
	data, err := json.MarshalIndent(&wrapper, "", "  ")
	if err != nil {
		return fmt.Errorf("marshal identity store: %w", err)
	}
	tmpPath := m.path + ".tmp"
	if err := os.MkdirAll(filepath.Dir(m.path), 0o755); err != nil && !errors.Is(err, os.ErrExist) {
		return fmt.Errorf("prepare identity directory: %w", err)
	}
	if err := os.WriteFile(tmpPath, data, 0o600); err != nil {
		return fmt.Errorf("write identity store: %w", err)
	}
	if err := os.Rename(tmpPath, m.path); err != nil {
		return fmt.Errorf("commit identity store: %w", err)
	}
	return nil
}

func (m *Manager) ValidateCertificate(cert *x509.Certificate) (Identity, error) {
	if cert == nil {
		return Identity{}, ErrInvalidCertificate
	}

	m.mu.RLock()
	defer m.mu.RUnlock()

	for _, user := range m.users {
		if len(user.CertDER) == 0 {
			continue
		}
		if bytes.Equal(user.CertDER, cert.Raw) {
			roles := append([]string(nil), user.Roles...)
			if len(roles) == 0 {
				roles = []string{"user"}
			}
			return Identity{
				UserID:   user.UserID,
				Nickname: user.Nickname,
				Roles:    roles,
				CertDER:  append([]byte(nil), cert.Raw...),
			}, nil
		}
	}

	return Identity{}, ErrCertificateMismatch
}

func (m *Manager) IdentityFromContext(ctx context.Context) (Identity, error) {
	cert, err := certificateFromContext(ctx)
	if err != nil {
		return Identity{}, err
	}
	return m.ValidateCertificate(cert)
}

func (m *Manager) GetProfile(ctx context.Context, userID string) (Profile, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	user, ok := m.users[userID]
	if !ok {
		return Profile{}, fmt.Errorf("identity: user %s not found", userID)
	}
	return user.toProfile(), nil
}

func (m *Manager) ListProfiles(ctx context.Context) ([]Profile, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	profiles := make([]Profile, 0, len(m.users))
	for _, user := range m.users {
		profiles = append(profiles, user.toProfile())
	}
	return profiles, nil
}

func (m *Manager) RegisterUser(ctx context.Context, nickname string, certDER []byte) (Profile, error) {
	if ctx != nil {
		if err := ctx.Err(); err != nil {
			return Profile{}, err
		}
	}
	nickname = strings.TrimSpace(nickname)
	if nickname == "" {
		return Profile{}, ErrInvalidNickname
	}
	if len(certDER) == 0 {
		return Profile{}, fmt.Errorf("identity: certificate must not be empty")
	}
	cert, err := x509.ParseCertificate(certDER)
	if err != nil {
		return Profile{}, fmt.Errorf("identity: parse certificate: %w", err)
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	for _, user := range m.users {
		if strings.EqualFold(user.Nickname, nickname) {
			return Profile{}, ErrNicknameTaken
		}
		if len(user.CertDER) > 0 && bytes.Equal(user.CertDER, cert.Raw) {
			return Profile{}, ErrCertificateAlreadyAssigned
		}
	}

	userID := m.nextUserIDLocked()
	stored := storedUser{
		UserID:   userID,
		Nickname: nickname,
		Roles:    []string{"user"},
		CertDER:  append([]byte(nil), cert.Raw...),
	}
	m.users[userID] = stored
	if err := m.persistLocked(); err != nil {
		delete(m.users, userID)
		return Profile{}, err
	}
	return stored.toProfile(), nil
}

func (m *Manager) nextUserIDLocked() string {
	const prefix = "user-"
	maxNumeric := 0
	for id := range m.users {
		if !strings.HasPrefix(id, prefix) {
			continue
		}
		n, err := strconv.Atoi(strings.TrimPrefix(id, prefix))
		if err != nil {
			continue
		}
		if n > maxNumeric {
			maxNumeric = n
		}
	}
	for {
		maxNumeric++
		candidate := fmt.Sprintf("%s%04d", prefix, maxNumeric)
		if _, exists := m.users[candidate]; !exists {
			return candidate
		}
	}
}

func (u storedUser) toProfile() Profile {
	roles := append([]string(nil), u.Roles...)
	if len(roles) == 0 {
		roles = []string{"user"}
	}
	return Profile{
		UserID:   u.UserID,
		Nickname: u.Nickname,
		Roles:    roles,
		CertDER:  append([]byte(nil), u.CertDER...),
	}
}

func certificateFromContext(ctx context.Context) (*x509.Certificate, error) {
	if ctx == nil {
		return nil, errors.New("identity: context is nil")
	}
	peerInfo, ok := peerFromContext(ctx)
	if !ok {
		return nil, errors.New("identity: missing peer information")
	}
	certs := peerInfo.TLSCertificates()
	if len(certs) == 0 {
		return nil, errors.New("identity: no client certificate present")
	}
	return certs[0], nil
}
