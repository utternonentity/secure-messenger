package identity

import (
	"bytes"
	"encoding/gob"
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

	wrapper := storeFile{Users: nil}
	buf := &bytes.Buffer{}
	if err := gob.NewEncoder(buf).Encode(&wrapper); err != nil {
		return fmt.Errorf("identity: marshal seed store: %w", err)
	}

	dir := filepath.Dir(path)
	if dir != "." && dir != "" {
		if err := os.MkdirAll(dir, 0o755); err != nil && !errors.Is(err, os.ErrExist) {
			return fmt.Errorf("identity: create seed directory: %w", err)
		}
	}
	if err := os.WriteFile(path, buf.Bytes(), 0o600); err != nil {
		return fmt.Errorf("identity: write seed store: %w", err)
	}
	return nil
}
