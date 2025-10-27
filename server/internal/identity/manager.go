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
	"time"
)

var (
	ErrInvalidCertificate  = errors.New("identity: certificate missing identity attributes")
	ErrDeviceRevoked       = errors.New("identity: device certificate revoked")
	ErrCertificateMismatch = errors.New("identity: certificate mismatch for device")
	ErrDisplayNameTaken    = errors.New("identity: display name already taken")
	ErrInvalidDisplayName  = errors.New("identity: display name must not be empty")
)

type Identity struct {
	UserID      string
	DeviceID    string
	DisplayName string
	Roles       []string
	CertDER     []byte
}

type Device struct {
	DeviceID string
	CertDER  []byte
	Revoked  bool
	Updated  time.Time
}

type Profile struct {
	UserID      string
	DisplayName string
	Roles       []string
	Devices     []Device
}

type storedUser struct {
	UserID      string                  `json:"user_id"`
	DisplayName string                  `json:"display_name"`
	Roles       []string                `json:"roles"`
	Devices     map[string]storedDevice `json:"devices"`
}

type storedDevice struct {
	DeviceID    string `json:"device_id"`
	CertDER     []byte `json:"cert_der"`
	Revoked     bool   `json:"revoked"`
	UpdatedUnix int64  `json:"updated_unix"`
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
		if user.Devices == nil {
			user.Devices = make(map[string]storedDevice)
		}
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
	identity, err := extractIdentity(cert)
	if err != nil {
		return Identity{}, err
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	user := m.users[identity.UserID]
	if user.UserID == "" {
		user = storedUser{
			UserID:      identity.UserID,
			DisplayName: identity.DisplayName,
			Roles:       []string{"user"},
			Devices:     make(map[string]storedDevice),
		}
	}
	if len(user.Roles) == 0 {
		user.Roles = []string{"user"}
	}
	if strings.TrimSpace(user.DisplayName) == "" {
		user.DisplayName = identity.DisplayName
	}
	dev := user.Devices[identity.DeviceID]
	if dev.DeviceID == "" {
		dev.DeviceID = identity.DeviceID
	}
	if dev.Revoked {
		return Identity{}, ErrDeviceRevoked
	}
	if len(dev.CertDER) > 0 && !bytes.Equal(dev.CertDER, identity.CertDER) {
		return Identity{}, ErrCertificateMismatch
	}
	dev.CertDER = append([]byte(nil), identity.CertDER...)
	dev.UpdatedUnix = time.Now().Unix()
	user.Devices[identity.DeviceID] = dev
	m.users[identity.UserID] = user
	if err := m.persistLocked(); err != nil {
		return Identity{}, err
	}

	identity.DisplayName = user.DisplayName
	identity.Roles = append([]string(nil), user.Roles...)
	return identity, nil
}

func (m *Manager) IdentityFromContext(ctx context.Context) (Identity, error) {
	cert, err := certificateFromContext(ctx)
	if err != nil {
		return Identity{}, err
	}
	base, err := extractIdentity(cert)
	if err != nil {
		return Identity{}, err
	}

	m.mu.RLock()
	defer m.mu.RUnlock()

	user, ok := m.users[base.UserID]
	if !ok {
		return Identity{}, fmt.Errorf("identity: user %s not found", base.UserID)
	}
	dev, ok := user.Devices[base.DeviceID]
	if !ok {
		return Identity{}, fmt.Errorf("identity: device %s not registered", base.DeviceID)
	}
	if dev.Revoked {
		return Identity{}, ErrDeviceRevoked
	}
	if !bytes.Equal(dev.CertDER, base.CertDER) {
		return Identity{}, ErrCertificateMismatch
	}
	base.DisplayName = user.DisplayName
	base.Roles = append([]string(nil), user.Roles...)
	return base, nil
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

func (m *Manager) RegisterUser(ctx context.Context, displayName string) (Profile, error) {
	if ctx != nil {
		if err := ctx.Err(); err != nil {
			return Profile{}, err
		}
	}
	displayName = strings.TrimSpace(displayName)
	if displayName == "" {
		return Profile{}, ErrInvalidDisplayName
	}

	m.mu.Lock()
	defer m.mu.Unlock()

	for _, user := range m.users {
		if strings.EqualFold(user.DisplayName, displayName) {
			return Profile{}, ErrDisplayNameTaken
		}
	}

	userID := m.nextUserIDLocked()
	stored := storedUser{
		UserID:      userID,
		DisplayName: displayName,
		Roles:       []string{"user"},
		Devices:     make(map[string]storedDevice),
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

func (m *Manager) RotateDeviceCertificate(ctx context.Context, userID, deviceID string, certDER []byte) (Device, error) {
	if len(certDER) == 0 {
		return Device{}, fmt.Errorf("identity: new certificate must not be empty")
	}
	m.mu.Lock()
	defer m.mu.Unlock()
	user, ok := m.users[userID]
	if !ok {
		return Device{}, fmt.Errorf("identity: user %s not found", userID)
	}
	dev, ok := user.Devices[deviceID]
	if !ok {
		return Device{}, fmt.Errorf("identity: device %s not registered", deviceID)
	}
	if dev.Revoked {
		return Device{}, ErrDeviceRevoked
	}
	dev.CertDER = append([]byte(nil), certDER...)
	dev.UpdatedUnix = time.Now().Unix()
	user.Devices[deviceID] = dev
	m.users[userID] = user
	if err := m.persistLocked(); err != nil {
		return Device{}, err
	}
	return storedDeviceToDevice(dev), nil
}

func (m *Manager) RevokeDevice(ctx context.Context, userID, deviceID string) (Device, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	user, ok := m.users[userID]
	if !ok {
		return Device{}, fmt.Errorf("identity: user %s not found", userID)
	}
	dev, ok := user.Devices[deviceID]
	if !ok {
		return Device{}, fmt.Errorf("identity: device %s not registered", deviceID)
	}
	dev.Revoked = true
	dev.UpdatedUnix = time.Now().Unix()
	user.Devices[deviceID] = dev
	m.users[userID] = user
	if err := m.persistLocked(); err != nil {
		return Device{}, err
	}
	return storedDeviceToDevice(dev), nil
}

func storedDeviceToDevice(dev storedDevice) Device {
	return Device{
		DeviceID: dev.DeviceID,
		CertDER:  append([]byte(nil), dev.CertDER...),
		Revoked:  dev.Revoked,
		Updated:  time.Unix(dev.UpdatedUnix, 0).UTC(),
	}
}

func (u storedUser) toProfile() Profile {
	devices := make([]Device, 0, len(u.Devices))
	for _, dev := range u.Devices {
		devices = append(devices, storedDeviceToDevice(dev))
	}
	return Profile{
		UserID:      u.UserID,
		DisplayName: u.DisplayName,
		Roles:       append([]string(nil), u.Roles...),
		Devices:     devices,
	}
}

func extractIdentity(cert *x509.Certificate) (Identity, error) {
	displayName := strings.TrimSpace(cert.Subject.CommonName)
	var userID, deviceID string
	for _, uri := range cert.URIs {
		if uri == nil || !strings.EqualFold(uri.Scheme, "sm") {
			continue
		}
		switch strings.ToLower(uri.Host) {
		case "user":
			userID = strings.Trim(strings.TrimPrefix(uri.Path, "/"), " ")
		case "device":
			deviceID = strings.Trim(strings.TrimPrefix(uri.Path, "/"), " ")
		}
	}
	if userID == "" || deviceID == "" {
		return Identity{}, ErrInvalidCertificate
	}
	if displayName == "" {
		displayName = userID
	}
	return Identity{
		UserID:      userID,
		DeviceID:    deviceID,
		DisplayName: displayName,
		CertDER:     append([]byte(nil), cert.Raw...),
	}, nil
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
