package identity

import (
	"bytes"
	"context"
	"crypto/rand"
	"crypto/sha256"
	"crypto/x509"
	"encoding/base64"
	"encoding/gob"
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
	ErrInvalidCredentials         = errors.New("identity: invalid nickname or password")
	ErrWeakPassword               = errors.New("identity: password is too weak")
)

const minPasswordLength = 8

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
	if err := decodeStore(data, &wrapper); err != nil {
		return fmt.Errorf("unmarshal identity store: %w", err)
	}
	needsPersist := false
	for _, user := range wrapper.Users {
		password := strings.TrimSpace(user.Password)
		if password != "" && !isHashedPassword(password) {
			hashed, err := hashPassword(password)
			if err != nil {
				return fmt.Errorf("identity: migrate password for %s: %w", user.UserID, err)
			}
			user.Password = hashed
			needsPersist = true
		}
		m.users[user.UserID] = user
	}
	if needsPersist {
		m.mu.Lock()
		if err := m.persistLocked(); err != nil {
			m.mu.Unlock()
			return err
		}
		m.mu.Unlock()
	}
	return nil
}

func decodeStore(data []byte, dest *storeFile) error {
	if len(data) == 0 {
		return nil
	}
	if err := gob.NewDecoder(bytes.NewReader(data)).Decode(dest); err == nil {
		return nil
	}
	if err := json.Unmarshal(data, dest); err == nil {
		return nil
	} else {
		return err
	}
}

func (m *Manager) persistLocked() error {
	wrapper := storeFile{Users: make([]storedUser, 0, len(m.users))}
	for _, user := range m.users {
		wrapper.Users = append(wrapper.Users, user)
	}
	buf := &bytes.Buffer{}
	if err := gob.NewEncoder(buf).Encode(&wrapper); err != nil {
		return fmt.Errorf("marshal identity store: %w", err)
	}
	tmpPath := m.path + ".tmp"
	if err := os.MkdirAll(filepath.Dir(m.path), 0o755); err != nil && !errors.Is(err, os.ErrExist) {
		return fmt.Errorf("prepare identity directory: %w", err)
	}
	if err := os.WriteFile(tmpPath, buf.Bytes(), 0o600); err != nil {
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

func (m *Manager) RegisterUser(ctx context.Context, nickname, password string, certDER []byte) (Profile, error) {
	if ctx != nil {
		if err := ctx.Err(); err != nil {
			return Profile{}, err
		}
	}
	nickname = strings.TrimSpace(nickname)
	if nickname == "" {
		return Profile{}, ErrInvalidNickname
	}
	password = strings.TrimSpace(password)
	if len(password) < minPasswordLength {
		return Profile{}, ErrWeakPassword
	}
	if len(certDER) == 0 {
		return Profile{}, fmt.Errorf("identity: certificate must not be empty")
	}
	cert, err := x509.ParseCertificate(certDER)
	if err != nil {
		return Profile{}, fmt.Errorf("identity: parse certificate: %w", err)
	}

	hashedPassword, err := hashPassword(password)
	if err != nil {
		return Profile{}, fmt.Errorf("identity: hash password: %w", err)
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
		Password: hashedPassword,
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

// Authenticate validates nickname/password credentials and matches the certificate, returning the identity.
func (m *Manager) Authenticate(ctx context.Context, nickname, password string, certDER []byte) (Identity, error) {
	if ctx != nil {
		if err := ctx.Err(); err != nil {
			return Identity{}, err
		}
	}
	nickname = strings.TrimSpace(nickname)
	password = strings.TrimSpace(password)
	if nickname == "" || password == "" {
		return Identity{}, ErrInvalidCredentials
	}

	var cert *x509.Certificate
	var err error
	if len(certDER) > 0 {
		cert, err = x509.ParseCertificate(certDER)
		if err != nil {
			return Identity{}, fmt.Errorf("identity: parse certificate: %w", err)
		}
	}

	m.mu.RLock()
	defer m.mu.RUnlock()

	for _, user := range m.users {
		if !strings.EqualFold(user.Nickname, nickname) {
			continue
		}
		if err := comparePassword(user.Password, password); err != nil {
			return Identity{}, ErrInvalidCredentials
		}
		if cert != nil {
			if len(user.CertDER) == 0 || !bytes.Equal(user.CertDER, cert.Raw) {
				return Identity{}, ErrCertificateMismatch
			}
		}
		return user.toIdentity(), nil
	}

	return Identity{}, ErrInvalidCredentials
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

func (u storedUser) toIdentity() Identity {
	profile := u.toProfile()
	return profile.ToIdentity()
}

// ToIdentity converts the profile into an Identity instance.
func (p Profile) ToIdentity() Identity {
	return Identity{
		UserID:   p.UserID,
		Nickname: p.Nickname,
		Roles:    append([]string(nil), p.Roles...),
		CertDER:  append([]byte(nil), p.CertDER...),
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

func hashPassword(password string) (string, error) {
	salt := make([]byte, 16)
	if _, err := rand.Read(salt); err != nil {
		return "", err
	}
	sum := sha256.Sum256(append(salt, []byte(password)...))
	encodedSalt := base64.StdEncoding.EncodeToString(salt)
	encodedHash := base64.StdEncoding.EncodeToString(sum[:])
	return encodedSalt + ":" + encodedHash, nil
}

func comparePassword(hashed, password string) error {
	parts := strings.Split(strings.TrimSpace(hashed), ":")
	if len(parts) != 2 {
		return ErrInvalidCredentials
	}
	salt, err := base64.StdEncoding.DecodeString(parts[0])
	if err != nil {
		return ErrInvalidCredentials
	}
	expected, err := base64.StdEncoding.DecodeString(parts[1])
	if err != nil {
		return ErrInvalidCredentials
	}
	sum := sha256.Sum256(append(salt, []byte(password)...))
	if !bytes.Equal(sum[:], expected) {
		return ErrInvalidCredentials
	}
	return nil
}

func isHashedPassword(password string) bool {
	parts := strings.Split(strings.TrimSpace(password), ":")
	if len(parts) != 2 {
		return false
	}
	if _, err := base64.StdEncoding.DecodeString(parts[0]); err != nil {
		return false
	}
	if _, err := base64.StdEncoding.DecodeString(parts[1]); err != nil {
		return false
	}
	return true
}
