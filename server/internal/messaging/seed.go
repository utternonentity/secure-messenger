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

	encode := func(id int64, conversation, sender, text string, sentUnix int64) (jsonMessage, error) {
		ciphertext, err := cipher.Encrypt([]byte(text))
		if err != nil {
			return jsonMessage{}, fmt.Errorf("messaging: encrypt seed message %d: %w", id, err)
		}
		return jsonMessage{
			ID:             id,
			ConversationID: conversation,
			SenderUserID:   sender,
			SentUnixSec:    sentUnix,
			CiphertextB64:  base64.StdEncoding.EncodeToString(ciphertext),
		}, nil
	}

	sample := jsonStore{Messages: make([]jsonMessage, 0, 6)}
	if cipher != nil {
		sample.KeyFingerprint = cipher.Fingerprint()
	}
	appendMsg := func(msg jsonMessage, err error) error {
		if err != nil {
			return err
		}
		sample.Messages = append(sample.Messages, msg)
		return nil
	}
	if err := appendMsg(encode(1, "corp-secure-room", "user-0002", "Привет! Сервер подтвердил наш общий ключ.", 1709484000)); err != nil {
		return err
	}
	if err := appendMsg(encode(2, "corp-secure-room", "user-0001", "Отличные новости, спасибо!", 1709484060)); err != nil {
		return err
	}
	if err := appendMsg(encode(3, "corp-secure-room", "user-0002", "Напомню, созвон через 15 минут.", 1709484300)); err != nil {
		return err
	}
	if err := appendMsg(encode(4, "corp-secure-room", "user-0001", "Принято, буду на связи.", 1709484360)); err != nil {
		return err
	}
	if err := appendMsg(encode(5, "dm-user-0001-user-0002", "user-0001", "Нужно проверить новые ключи доступа.", 1709485200)); err != nil {
		return err
	}
	if err := appendMsg(encode(6, "dm-user-0001-user-0002", "user-0002", "Готово, всё активировано.", 1709485260)); err != nil {
		return err
	}

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
