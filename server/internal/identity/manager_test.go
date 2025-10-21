package identity

import (
	"context"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"math/big"
	"net/url"
	"testing"
	"time"

	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/peer"
)

func TestValidateCertificateRegistersUser(t *testing.T) {
	mgr := newTestManager(t)

	cert := newIdentityCert(t, "Alice", "user123", "deviceA")

	identity, err := mgr.ValidateCertificate(cert)
	if err != nil {
		t.Fatalf("ValidateCertificate: %v", err)
	}
	if identity.UserID != "user123" || identity.DeviceID != "deviceA" {
		t.Fatalf("unexpected identity: %+v", identity)
	}

	ctx := peer.NewContext(context.Background(), &peer.Peer{AuthInfo: credentials.TLSInfo{State: tls.ConnectionState{PeerCertificates: []*x509.Certificate{cert}}}})
	ctxIdentity, err := mgr.IdentityFromContext(ctx)
	if err != nil {
		t.Fatalf("IdentityFromContext: %v", err)
	}
	if ctxIdentity.UserID != "user123" || ctxIdentity.DeviceID != "deviceA" {
		t.Fatalf("unexpected context identity: %+v", ctxIdentity)
	}

	profile, err := mgr.GetProfile(context.Background(), "user123")
	if err != nil {
		t.Fatalf("GetProfile: %v", err)
	}
	if len(profile.Devices) != 1 {
		t.Fatalf("expected 1 device, got %d", len(profile.Devices))
	}
}

func TestValidateCertificateRejectsRevoked(t *testing.T) {
	mgr := newTestManager(t)
	cert := newIdentityCert(t, "Alice", "user123", "deviceA")

	if _, err := mgr.ValidateCertificate(cert); err != nil {
		t.Fatalf("ValidateCertificate: %v", err)
	}

	if _, err := mgr.RevokeDevice(context.Background(), "user123", "deviceA"); err != nil {
		t.Fatalf("RevokeDevice: %v", err)
	}

	if _, err := mgr.ValidateCertificate(cert); err != ErrDeviceRevoked {
		t.Fatalf("expected ErrDeviceRevoked, got %v", err)
	}
}

func TestRotateDeviceCertificate(t *testing.T) {
	mgr := newTestManager(t)
	cert1 := newIdentityCert(t, "Alice", "user123", "deviceA")
	if _, err := mgr.ValidateCertificate(cert1); err != nil {
		t.Fatalf("ValidateCertificate: %v", err)
	}

	cert2 := newIdentityCert(t, "Alice", "user123", "deviceA")
	if _, err := mgr.ValidateCertificate(cert2); err == nil {
		t.Fatalf("expected mismatch before rotation")
	}

	if _, err := mgr.RotateDeviceCertificate(context.Background(), "user123", "deviceA", cert2.Raw); err != nil {
		t.Fatalf("RotateDeviceCertificate: %v", err)
	}

	if _, err := mgr.ValidateCertificate(cert2); err != nil {
		t.Fatalf("ValidateCertificate(after rotation): %v", err)
	}
}

func newIdentityCert(t *testing.T, cn, userID, deviceID string) *x509.Certificate {
	t.Helper()

	priv, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("generate key: %v", err)
	}

	tmpl := &x509.Certificate{
		SerialNumber: big.NewInt(time.Now().UnixNano()),
		Subject: pkix.Name{
			CommonName: cn,
		},
		NotBefore: time.Now().Add(-time.Hour),
		NotAfter:  time.Now().Add(24 * time.Hour),
		KeyUsage:  x509.KeyUsageDigitalSignature,
		URIs: []*url.URL{
			mustParseURL(t, "sm://user/"+userID),
			mustParseURL(t, "sm://device/"+deviceID),
		},
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

func mustParseURL(t *testing.T, raw string) *url.URL {
	t.Helper()
	u, err := url.Parse(raw)
	if err != nil {
		t.Fatalf("parse url %s: %v", raw, err)
	}
	return u
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
