package messaging

import (
	"bytes"
	"encoding/gob"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// EnsureSeedData creates a demo message history if the store file does not exist.
func EnsureSeedData(path string, cipher EnvelopeCipher) error {
	if strings.TrimSpace(path) == "" {
		return fmt.Errorf("messaging: seed path must not be empty")
	}
	if cipher == nil {
		return fmt.Errorf("messaging: seed cipher must not be nil")
	}
	if _, err := os.Stat(path); err == nil {
		return nil
	} else if !errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("messaging: check seed store: %w", err)
	}

	snapshot := storeSnapshot{Messages: nil}
	if cipher != nil {
		snapshot.KeyFingerprint = cipher.Fingerprint()
	}

	buf := &bytes.Buffer{}
	if err := gob.NewEncoder(buf).Encode(&snapshot); err != nil {
		return fmt.Errorf("messaging: marshal seed messages: %w", err)
	}
	dir := filepath.Dir(path)
	if dir != "." && dir != "" {
		if err := os.MkdirAll(dir, 0o755); err != nil && !errors.Is(err, os.ErrExist) {
			return fmt.Errorf("messaging: create seed directory: %w", err)
		}
	}
	if err := os.WriteFile(path, buf.Bytes(), 0o600); err != nil {
		return fmt.Errorf("messaging: write seed messages: %w", err)
	}
	return nil
}
