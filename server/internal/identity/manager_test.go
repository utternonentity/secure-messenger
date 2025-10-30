package identity

import (
	"context"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"errors"
	"math/big"
	"testing"
	"time"

	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/peer"
)

func TestValidateCertificateMatchesRegisteredUser(t *testing.T) {
	mgr := newTestManager(t)

	cert := newTestCertificate(t, "Alice")
	profile, err := mgr.RegisterUser(context.Background(), "Alice", cert.Raw)
	if err != nil {
		t.Fatalf("RegisterUser: %v", err)
	}

	identity, err := mgr.ValidateCertificate(cert)
	if err != nil {
		t.Fatalf("ValidateCertificate: %v", err)
	}
	if identity.UserID != profile.UserID {
		t.Fatalf("unexpected identity: %+v", identity)
	}

	ctx := peer.NewContext(context.Background(), &peer.Peer{AuthInfo: credentials.TLSInfo{State: tls.ConnectionState{PeerCertificates: []*x509.Certificate{cert}}}})
	ctxIdentity, err := mgr.IdentityFromContext(ctx)
	if err != nil {
		t.Fatalf("IdentityFromContext: %v", err)
	}
	if ctxIdentity.UserID != profile.UserID {
		t.Fatalf("unexpected context identity: %+v", ctxIdentity)
	}

	stored, err := mgr.GetProfile(context.Background(), profile.UserID)
	if err != nil {
		t.Fatalf("GetProfile: %v", err)
	}
	if len(stored.CertDER) == 0 {
		t.Fatalf("expected certificate to be stored")
	}
}

func TestValidateCertificateRejectsUnknown(t *testing.T) {
	mgr := newTestManager(t)

	cert1 := newTestCertificate(t, "Alice")
	if _, err := mgr.RegisterUser(context.Background(), "Alice", cert1.Raw); err != nil {
		t.Fatalf("RegisterUser: %v", err)
	}

	cert2 := newTestCertificate(t, "Bob")
	if _, err := mgr.ValidateCertificate(cert2); !errors.Is(err, ErrCertificateMismatch) {
		t.Fatalf("expected ErrCertificateMismatch, got %v", err)
	}
}

func TestRegisterUser(t *testing.T) {
	mgr := newTestManager(t)

	certAlice := newTestCertificate(t, "Alice")
	profile, err := mgr.RegisterUser(context.Background(), "Alice", certAlice.Raw)
	if err != nil {
		t.Fatalf("RegisterUser: %v", err)
	}
	if profile.Nickname != "Alice" {
		t.Fatalf("unexpected nickname: %q", profile.Nickname)
	}
	if profile.UserID != "user-0001" {
		t.Fatalf("unexpected user id: %q", profile.UserID)
	}

	if _, err := mgr.GetProfile(context.Background(), profile.UserID); err != nil {
		t.Fatalf("GetProfile after register: %v", err)
	}

	if _, err = mgr.RegisterUser(context.Background(), "alice", certAlice.Raw); !errors.Is(err, ErrNicknameTaken) {
		t.Fatalf("expected ErrNicknameTaken, got %v", err)
	}
	if _, err = mgr.RegisterUser(context.Background(), "Alice-2", certAlice.Raw); !errors.Is(err, ErrCertificateAlreadyAssigned) {
		t.Fatalf("expected ErrCertificateAlreadyAssigned, got %v", err)
	}

	certBob := newTestCertificate(t, "Bob")
	second, err := mgr.RegisterUser(context.Background(), "Bob", certBob.Raw)
	if err != nil {
		t.Fatalf("RegisterUser second: %v", err)
	}
	if second.UserID != "user-0002" {
		t.Fatalf("unexpected second user id: %q", second.UserID)
	}

	if _, err = mgr.RegisterUser(context.Background(), " ", certBob.Raw); !errors.Is(err, ErrInvalidNickname) {
		t.Fatalf("expected ErrInvalidNickname, got %v", err)
	}
}

func newTestCertificate(t *testing.T, cn string) *x509.Certificate {
	t.Helper()

	priv, err := rsa.GenerateKey(rand.Reader, 1024)
	if err != nil {
		t.Fatalf("generate key: %v", err)
	}

	tmpl := &x509.Certificate{
		SerialNumber:          big.NewInt(time.Now().UnixNano()),
		Subject:               pkix.Name{CommonName: cn},
		NotBefore:             time.Now().Add(-time.Hour),
		NotAfter:              time.Now().Add(24 * time.Hour),
		KeyUsage:              x509.KeyUsageDigitalSignature,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageClientAuth},
		BasicConstraintsValid: true,
	}

	der, err := x509.CreateCertificate(rand.Reader, tmpl, tmpl, &priv.PublicKey, priv)
	if err != nil {
		t.Fatalf("create certificate: %v", err)
	}
	cert, err := x509.ParseCertificate(der)
	if err != nil {
		t.Fatalf("parse certificate: %v", err)
	}
	return cert
}

func newTestManager(t *testing.T) *Manager {
	t.Helper()
	mgr, err := NewManager(t.TempDir() + "/identity.db")
	if err != nil {
		t.Fatalf("NewManager: %v", err)
	}
	t.Cleanup(func() {
		_ = mgr.Close()
	})
	return mgr
}
