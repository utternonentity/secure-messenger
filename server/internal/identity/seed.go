package identity

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
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

	adminPassword, err := hashPassword("swordfish")
	if err != nil {
		return fmt.Errorf("identity: hash seed password: %w", err)
	}
	userPassword, err := hashPassword("starlight")
	if err != nil {
		return fmt.Errorf("identity: hash seed password: %w", err)
	}

	users := []storedUser{
		{
			UserID:   "user-0001",
			Nickname: "ironwarden",
			Roles:    []string{"admin", "user"},
			Password: adminPassword,
			CertDER:  []byte("Device 01 primary"),
		},
		{
			UserID:   "user-0002",
			Nickname: "nova",
			Roles:    []string{"user"},
			Password: userPassword,
			CertDER:  []byte("Maria laptop"),
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
