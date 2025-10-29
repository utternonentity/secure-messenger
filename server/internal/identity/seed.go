package identity

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"
)

// EnsureSeedData creates a demo identity store if the file does not exist yet.
func EnsureSeedData(path string) error {
	if strings.TrimSpace(path) == "" {
		return fmt.Errorf("identity: seed path must not be empty")
	}
	if _, err := os.Stat(path); err == nil {
		return nil
	} else if !errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("identity: check seed store: %w", err)
	}

	now := time.Now().UTC().Unix()
	users := []storedUser{
		{
			UserID:   "user-0001",
			Nickname: "ironwarden",
			Roles:    []string{"admin", "user"},
			Password: "swordfish",
			Devices: map[string]storedDevice{
				"device-ivan-laptop": {
					DeviceID:    "device-ivan-laptop",
					CertDER:     []byte("Device 01 primary"),
					Revoked:     false,
					UpdatedUnix: now,
				},
				"device-ivan-phone": {
					DeviceID:    "device-ivan-phone",
					CertDER:     []byte("Ivan phone"),
					Revoked:     false,
					UpdatedUnix: now,
				},
			},
		},
		{
			UserID:   "user-0002",
			Nickname: "nova",
			Roles:    []string{"user"},
			Password: "starlight",
			Devices: map[string]storedDevice{
				"device-maria-laptop": {
					DeviceID:    "device-maria-laptop",
					CertDER:     []byte("Maria laptop"),
					Revoked:     false,
					UpdatedUnix: now,
				},
				"device-maria-mobile": {
					DeviceID:    "device-maria-mobile",
					CertDER:     []byte("Maria mobile"),
					Revoked:     false,
					UpdatedUnix: now,
				},
			},
		},
	}

	wrapper := storeFile{Users: users}
	data, err := json.MarshalIndent(&wrapper, "", "  ")
	if err != nil {
		return fmt.Errorf("identity: marshal seed store: %w", err)
	}

	dir := filepath.Dir(path)
	if dir != "." && dir != "" {
		if err := os.MkdirAll(dir, 0o755); err != nil && !errors.Is(err, os.ErrExist) {
			return fmt.Errorf("identity: create seed directory: %w", err)
		}
	}
	if err := os.WriteFile(path, data, 0o600); err != nil {
		return fmt.Errorf("identity: write seed store: %w", err)
	}
	return nil
}
