package messaging

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"crypto/sha256"
	"encoding/base64"
	"fmt"
	"io"
	"strings"
)

// DefaultMessageKeyBase64 provides a development-friendly 32-byte AES-256-GCM key
// encoded with base64. Override it in production via the SM_MESSAGE_KEY
// environment variable or the --message-key flag.
const DefaultMessageKeyBase64 = "KpEyIdHR3J8zvm64LKGhXgeOy4cmh09YkHxAUlPAuro="

// EnvelopeCipher encrypts and decrypts envelope payloads for the HTTP API.
type EnvelopeCipher interface {
	Encrypt(plaintext []byte) ([]byte, error)
	Decrypt(ciphertext []byte) ([]byte, error)
	Fingerprint() string
}

// AESGCMCipher implements EnvelopeCipher using AES-256-GCM.
type AESGCMCipher struct {
	aead   cipher.AEAD
	rawKey []byte
}

// NewAESGCMCipher constructs an AES-256-GCM cipher from the provided raw key.
func NewAESGCMCipher(key []byte) (*AESGCMCipher, error) {
	if len(key) != 32 {
		return nil, fmt.Errorf("message key must be 32 bytes for AES-256-GCM")
	}
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, fmt.Errorf("init cipher: %w", err)
	}
	aead, err := cipher.NewGCM(block)
	if err != nil {
		return nil, fmt.Errorf("init AEAD: %w", err)
	}
	return &AESGCMCipher{aead: aead, rawKey: append([]byte(nil), key...)}, nil
}

// NewAESGCMCipherFromBase64 constructs an AES-256-GCM cipher from a base64 key string.
func NewAESGCMCipherFromBase64(keyB64 string) (*AESGCMCipher, error) {
	key, err := decodeKey(keyB64)
	if err != nil {
		return nil, err
	}
	return NewAESGCMCipher(key)
}

// Encrypt encrypts the provided plaintext and prefixes the random nonce to the ciphertext.
func (c *AESGCMCipher) Encrypt(plaintext []byte) ([]byte, error) {
	nonce := make([]byte, c.aead.NonceSize())
	if _, err := io.ReadFull(rand.Reader, nonce); err != nil {
		return nil, fmt.Errorf("generate nonce: %w", err)
	}
	sealed := c.aead.Seal(nil, nonce, plaintext, nil)
	return append(nonce, sealed...), nil
}

// Decrypt validates and decrypts the ciphertext produced by Encrypt.
func (c *AESGCMCipher) Decrypt(ciphertext []byte) ([]byte, error) {
	nonceSize := c.aead.NonceSize()
	if len(ciphertext) < nonceSize {
		return nil, fmt.Errorf("ciphertext too short")
	}
	nonce := ciphertext[:nonceSize]
	payload := ciphertext[nonceSize:]
	plaintext, err := c.aead.Open(nil, nonce, payload, nil)
	if err != nil {
		return nil, fmt.Errorf("decrypt ciphertext: %w", err)
	}
	return plaintext, nil
}

// Fingerprint returns a stable base64-encoded fingerprint of the AES key.
// It can be persisted alongside message data to detect configuration drift.
func (c *AESGCMCipher) Fingerprint() string {
	sum := sha256.Sum256(c.rawKey)
	return base64.StdEncoding.EncodeToString(sum[:])
}

func decodeKey(keyB64 string) ([]byte, error) {
	trimmed := strings.TrimSpace(keyB64)
	if trimmed == "" {
		return nil, fmt.Errorf("message key must be provided")
	}
	key, err := base64.StdEncoding.DecodeString(trimmed)
	if err != nil {
		return nil, fmt.Errorf("decode base64 message key: %w", err)
	}
	return key, nil
}
