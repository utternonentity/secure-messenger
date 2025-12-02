package messaging

import (
	"context"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"google.golang.org/protobuf/proto"

	smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
)

// StoredEnvelope represents a persisted envelope along with its monotonic identifier.
type StoredEnvelope struct {
	ID       int64
	Envelope *smv1.EncryptedEnvelope
}

type envelopeRepository interface {
	Save(ctx context.Context, env *smv1.EncryptedEnvelope) (int64, error)
	ForEachSince(ctx context.Context, afterID int64, fn func(StoredEnvelope) error) error
}

type fileStore struct {
	mu             sync.RWMutex
	path           string
	cipher         EnvelopeCipher
	records        []fileRecord
	nextID         int64
	keyFingerprint string
}

type fileRecord struct {
	id       int64
	envelope *smv1.EncryptedEnvelope
}

type jsonStore struct {
	Messages       []jsonMessage `json:"messages"`
	KeyFingerprint string        `json:"key_fingerprint,omitempty"`
}

type jsonMessage struct {
	ID             int64  `json:"id"`
	ConversationID string `json:"conversation_id"`
	SenderUserID   string `json:"sender_user_id"`
	SentUnixSec    int64  `json:"sent_unix_sec"`
	CiphertextB64  string `json:"ciphertext_b64"`
	Plaintext      string `json:"text,omitempty"`
	ServerMsgID    string `json:"server_msg_id,omitempty"`
}

// NewStore creates a persistent store backed by a JSON file.
func NewStore(path string, cipher EnvelopeCipher) (*fileStore, error) {
	if strings.TrimSpace(path) == "" {
		return nil, fmt.Errorf("store path must not be empty")
	}
	if cipher == nil {
		return nil, fmt.Errorf("store cipher must not be nil")
	}
	store := &fileStore{path: path, cipher: cipher}
	dirty, err := store.load()
	if err != nil {
		return nil, err
	}
	if store.keyFingerprint != "" && store.keyFingerprint != cipher.Fingerprint() {
		return nil, fmt.Errorf("message store encrypted with a different key; update SM_MESSAGE_KEY")
	}
	if store.keyFingerprint == "" {
		store.keyFingerprint = cipher.Fingerprint()
		dirty = true
	}
	if dirty {
		if err := store.persist(); err != nil {
			return nil, err
		}
	}
	return store, nil
}

// Close is kept for compatibility with previous implementations.
func (s *fileStore) Close() error { return nil }

func (s *fileStore) load() (bool, error) {
	dirty := false
	data, err := os.ReadFile(s.path)
	if errors.Is(err, os.ErrNotExist) {
		s.records = nil
		s.nextID = 0
		return false, nil
	}
	if err != nil {
		return false, fmt.Errorf("read message store: %w", err)
	}
	if len(data) == 0 {
		s.records = nil
		s.nextID = 0
		return false, nil
	}
	var wrapper jsonStore
	if err := json.Unmarshal(data, &wrapper); err != nil {
		return false, fmt.Errorf("unmarshal message store: %w", err)
	}
	s.keyFingerprint = strings.TrimSpace(wrapper.KeyFingerprint)
	records := make([]fileRecord, 0, len(wrapper.Messages))
	var maxID int64
	for _, msg := range wrapper.Messages {
		env := &smv1.EncryptedEnvelope{Meta: &smv1.EnvelopeMeta{}}
		if msg.ConversationID != "" {
			env.Meta.ConversationId = msg.ConversationID
		}
		if msg.SenderUserID != "" {
			env.Meta.SenderUserId = msg.SenderUserID
		}
		if msg.SentUnixSec != 0 {
			env.Meta.SentUnixSec = msg.SentUnixSec
		}

		id := msg.ID
		if id == 0 && msg.ServerMsgID != "" {
			parsed, err := parseServerMsgID(msg.ServerMsgID)
			if err == nil {
				id = parsed
			}
		}
		if id == 0 {
			id = maxID + 1
			dirty = true
		}

		if msg.CiphertextB64 != "" {
			payload, err := base64.StdEncoding.DecodeString(msg.CiphertextB64)
			if err != nil {
				return false, fmt.Errorf("decode message %d ciphertext: %w", msg.ID, err)
			}
			env.Ciphertext = payload
		}

		if len(env.Ciphertext) == 0 && strings.TrimSpace(msg.Plaintext) != "" {
			if s.cipher == nil {
				return false, fmt.Errorf("store cipher is required to encrypt plaintext messages")
			}
			ciphertext, err := s.cipher.Encrypt([]byte(msg.Plaintext))
			if err != nil {
				return false, fmt.Errorf("encrypt legacy message %d: %w", id, err)
			}
			env.Ciphertext = ciphertext
			dirty = true
		}

		records = append(records, fileRecord{id: id, envelope: env})
		if id > maxID {
			maxID = id
		}
	}
	s.records = records
	s.nextID = maxID
	return dirty, nil
}

func (s *fileStore) persist() error {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return s.persistLocked()
}

func (s *fileStore) persistLocked() error {
	wrapper := jsonStore{Messages: make([]jsonMessage, 0, len(s.records))}
	for _, rec := range s.records {
		env := rec.envelope
		meta := env.GetMeta()
		msg := jsonMessage{ID: rec.id}
		if meta != nil {
			msg.ConversationID = meta.GetConversationId()
			msg.SenderUserID = meta.GetSenderUserId()
			msg.SentUnixSec = meta.GetSentUnixSec()
		}
		if len(env.GetCiphertext()) > 0 {
			msg.CiphertextB64 = base64.StdEncoding.EncodeToString(env.GetCiphertext())
		}
		wrapper.Messages = append(wrapper.Messages, msg)
	}
	if s.keyFingerprint != "" {
		wrapper.KeyFingerprint = s.keyFingerprint
	}

	data, err := json.MarshalIndent(&wrapper, "", "  ")
	if err != nil {
		return fmt.Errorf("marshal message store: %w", err)
	}
	dir := filepath.Dir(s.path)
	if dir != "." && dir != "" {
		if err := os.MkdirAll(dir, 0o755); err != nil && !errors.Is(err, os.ErrExist) {
			return fmt.Errorf("prepare store directory: %w", err)
		}
	}
	tmpPath := s.path + ".tmp"
	if err := os.WriteFile(tmpPath, data, 0o600); err != nil {
		return fmt.Errorf("write message store: %w", err)
	}
	if err := os.Rename(tmpPath, s.path); err != nil {
		return fmt.Errorf("commit message store: %w", err)
	}
	return nil
}

func (s *fileStore) Save(ctx context.Context, env *smv1.EncryptedEnvelope) (int64, error) {
	if env == nil {
		return 0, fmt.Errorf("envelope must not be nil")
	}
	if err := ctx.Err(); err != nil {
		return 0, err
	}

	s.mu.Lock()
	defer s.mu.Unlock()

	id := s.nextID + 1
	cloned := proto.Clone(env).(*smv1.EncryptedEnvelope)
	s.records = append(s.records, fileRecord{id: id, envelope: cloned})
	s.nextID = id
	if err := s.persistLocked(); err != nil {
		s.records = s.records[:len(s.records)-1]
		s.nextID--
		return 0, err
	}
	return id, nil
}

func (s *fileStore) ForEachSince(ctx context.Context, afterID int64, fn func(StoredEnvelope) error) error {
	if err := ctx.Err(); err != nil {
		return err
	}

	s.mu.RLock()
	snapshot := make([]fileRecord, 0, len(s.records))
	for _, rec := range s.records {
		if rec.id > afterID {
			snapshot = append(snapshot, fileRecord{id: rec.id, envelope: proto.Clone(rec.envelope).(*smv1.EncryptedEnvelope)})
		}
	}
	s.mu.RUnlock()

	for _, rec := range snapshot {
		if err := ctx.Err(); err != nil {
			return err
		}
		if err := fn(StoredEnvelope{ID: rec.id, Envelope: rec.envelope}); err != nil {
			return err
		}
	}
	return nil
}
