package messaging

import (
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
	"sync"

	"google.golang.org/protobuf/proto"

	smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
)

// StoredEnvelope represents a persisted envelope along with its monotonic server identifier.
type StoredEnvelope struct {
	ID       int64
	Envelope *smv1.EncryptedEnvelope
}

// envelopeRepository defines the behaviour required by the messaging service to persist envelopes.
type envelopeRepository interface {
	Save(ctx context.Context, env *smv1.EncryptedEnvelope) (int64, error)
	ForEachSince(ctx context.Context, afterID int64, fn func(StoredEnvelope) error) error
}

const (
	recordHeaderSize = 12 // 8 bytes for the identifier, 4 bytes for the payload length.
)

type fileStore struct {
	mu      sync.RWMutex
	file    *os.File
	records []fileRecord
	nextID  int64
}

type fileRecord struct {
	id      int64
	payload []byte
}

// NewFileStore initialises a persistent store backed by an append-only file. The file is created if absent.
func NewFileStore(path string) (*fileStore, error) {
	if strings.TrimSpace(path) == "" {
		return nil, fmt.Errorf("store path must not be empty")
	}

	f, err := os.OpenFile(path, os.O_CREATE|os.O_RDWR, 0o600)
	if err != nil {
		return nil, fmt.Errorf("open store file: %w", err)
	}

	records, err := loadRecords(f)
	if err != nil {
		f.Close()
		return nil, err
	}

	if _, err := f.Seek(0, io.SeekEnd); err != nil {
		f.Close()
		return nil, fmt.Errorf("seek end: %w", err)
	}

	var nextID int64
	if n := len(records); n > 0 {
		nextID = records[n-1].id
	}

	return &fileStore{file: f, records: records, nextID: nextID}, nil
}

// Close releases the underlying file descriptor.
func (s *fileStore) Close() error {
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.file == nil {
		return nil
	}
	err := s.file.Close()
	s.file = nil
	return err
}

func (s *fileStore) Save(ctx context.Context, env *smv1.EncryptedEnvelope) (int64, error) {
	if env == nil {
		return 0, fmt.Errorf("envelope must not be nil")
	}
	if err := ctx.Err(); err != nil {
		return 0, err
	}

	payload, err := proto.Marshal(env)
	if err != nil {
		return 0, fmt.Errorf("marshal envelope: %w", err)
	}

	s.mu.Lock()
	defer s.mu.Unlock()

	if s.file == nil {
		return 0, errors.New("store is closed")
	}

	id := s.nextID + 1
	rec := fileRecord{id: id, payload: payload}
	if err := writeRecord(s.file, rec); err != nil {
		return 0, err
	}
	if err := s.file.Sync(); err != nil {
		return 0, fmt.Errorf("sync store: %w", err)
	}

	s.records = append(s.records, rec)
	s.nextID = id
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
			snapshot = append(snapshot, rec)
		}
	}
	s.mu.RUnlock()

	for _, rec := range snapshot {
		if err := ctx.Err(); err != nil {
			return err
		}
		env := new(smv1.EncryptedEnvelope)
		if err := proto.Unmarshal(rec.payload, env); err != nil {
			return fmt.Errorf("unmarshal envelope %d: %w", rec.id, err)
		}
		if err := fn(StoredEnvelope{ID: rec.id, Envelope: env}); err != nil {
			return err
		}
	}
	return nil
}

func loadRecords(f *os.File) ([]fileRecord, error) {
	if _, err := f.Seek(0, io.SeekStart); err != nil {
		return nil, fmt.Errorf("seek start: %w", err)
	}

	header := make([]byte, recordHeaderSize)
	records := make([]fileRecord, 0)

	for {
		if _, err := io.ReadFull(f, header); err != nil {
			if errors.Is(err, io.EOF) {
				break
			}
			if errors.Is(err, io.ErrUnexpectedEOF) {
				return nil, fmt.Errorf("truncated record header")
			}
			return nil, fmt.Errorf("read record header: %w", err)
		}

		length := binary.LittleEndian.Uint32(header[8:])
		payload := make([]byte, length)
		if _, err := io.ReadFull(f, payload); err != nil {
			if errors.Is(err, io.EOF) || errors.Is(err, io.ErrUnexpectedEOF) {
				return nil, fmt.Errorf("truncated record payload")
			}
			return nil, fmt.Errorf("read record payload: %w", err)
		}

		id := int64(binary.LittleEndian.Uint64(header[:8]))
		records = append(records, fileRecord{id: id, payload: payload})
	}

	return records, nil
}

func writeRecord(f *os.File, rec fileRecord) error {
	var header [recordHeaderSize]byte
	binary.LittleEndian.PutUint64(header[:8], uint64(rec.id))
	if len(rec.payload) > int(^uint32(0)) {
		return fmt.Errorf("envelope too large: %d bytes", len(rec.payload))
	}
	binary.LittleEndian.PutUint32(header[8:], uint32(len(rec.payload)))

	if _, err := f.Write(header[:]); err != nil {
		return fmt.Errorf("write record header: %w", err)
	}
	if _, err := f.Write(rec.payload); err != nil {
		return fmt.Errorf("write record payload: %w", err)
	}
	return nil
}
