package messaging

import (
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

// EnsureSeedData creates a demo message history if the store file does not exist.
func EnsureSeedData(path string) error {
	if strings.TrimSpace(path) == "" {
		return fmt.Errorf("messaging: seed path must not be empty")
	}
	if _, err := os.Stat(path); err == nil {
		return nil
	} else if !errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("messaging: check seed store: %w", err)
	}

	sample := jsonStore{Messages: []jsonMessage{
		{
			ID:             1,
			ConversationID: "corp-secure-room",
			SenderUserID:   "user-0002",
			SenderDeviceID: "device-maria-laptop",
			SentUnixSec:    1709484000,
			CiphertextB64:  encodeCiphertext("Привет! Сервер подтвердил наш общий ключ."),
		},
		{
			ID:             2,
			ConversationID: "corp-secure-room",
			SenderUserID:   "user-0001",
			SenderDeviceID: "device-ivan-laptop",
			SentUnixSec:    1709484060,
			CiphertextB64:  encodeCiphertext("Отличные новости, спасибо!"),
		},
		{
			ID:             3,
			ConversationID: "corp-secure-room",
			SenderUserID:   "user-0002",
			SenderDeviceID: "device-maria-mobile",
			SentUnixSec:    1709484300,
			CiphertextB64:  encodeCiphertext("Напомню, созвон через 15 минут."),
		},
		{
			ID:             4,
			ConversationID: "corp-secure-room",
			SenderUserID:   "user-0001",
			SenderDeviceID: "device-ivan-phone",
			SentUnixSec:    1709484360,
			CiphertextB64:  encodeCiphertext("Принято, буду на связи."),
		},
		{
			ID:             5,
			ConversationID: "dm-user-0001-user-0002",
			SenderUserID:   "user-0001",
			SenderDeviceID: "device-ivan-laptop",
			SentUnixSec:    1709485200,
			CiphertextB64:  encodeCiphertext("Нужно проверить новые ключи доступа."),
		},
		{
			ID:             6,
			ConversationID: "dm-user-0001-user-0002",
			SenderUserID:   "user-0002",
			SenderDeviceID: "device-maria-laptop",
			SentUnixSec:    1709485260,
			CiphertextB64:  encodeCiphertext("Готово, всё активировано."),
		},
	}}

	data, err := json.MarshalIndent(&sample, "", "  ")
	if err != nil {
		return fmt.Errorf("messaging: marshal seed messages: %w", err)
	}
	dir := filepath.Dir(path)
	if dir != "." && dir != "" {
		if err := os.MkdirAll(dir, 0o755); err != nil && !errors.Is(err, os.ErrExist) {
			return fmt.Errorf("messaging: create seed directory: %w", err)
		}
	}
	if err := os.WriteFile(path, data, 0o600); err != nil {
		return fmt.Errorf("messaging: write seed messages: %w", err)
	}
	return nil
}

func encodeCiphertext(text string) string {
	return base64.StdEncoding.EncodeToString([]byte(text))
}
