package mtls

import (
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"math/big"
	"net/url"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestLoadServerTLSConfigSuccess(t *testing.T) {
	certPEM, keyPEM, caPEM := generateServerAndCA(t)
	dir := t.TempDir()
	certFile := writeTempFile(t, dir, "server-cert-*.pem", certPEM)
	keyFile := writeTempFile(t, dir, "server-key-*.pem", keyPEM)
	caFile := writeTempFile(t, dir, "ca-cert-*.pem", caPEM)

	cfg, err := LoadServerTLSConfig(certFile, keyFile, caFile, nil)
	if err != nil {
		t.Fatalf("LoadServerTLSConfig returned error: %v", err)
	}

	if len(cfg.Certificates) != 1 {
		t.Fatalf("expected 1 certificate, got %d", len(cfg.Certificates))
	}
	if cfg.ClientAuth != tls.RequireAndVerifyClientCert {
		t.Fatalf("expected ClientAuth RequireAndVerifyClientCert, got %v", cfg.ClientAuth)
	}
	if cfg.MinVersion != tls.VersionTLS13 {
		t.Fatalf("expected MinVersion TLS1.3, got %d", cfg.MinVersion)
	}
	if cfg.ClientCAs == nil || len(cfg.ClientCAs.Subjects()) == 0 {
		t.Fatal("expected ClientCAs to contain CA certificate")
	}
}

func TestLoadServerTLSConfigMissingKeyPair(t *testing.T) {
	_, _, caPEM := generateServerAndCA(t)
	dir := t.TempDir()
	caFile := writeTempFile(t, dir, "ca-cert-*.pem", caPEM)

	_, err := LoadServerTLSConfig(filepath.Join(dir, "missing-cert.pem"), filepath.Join(dir, "missing-key.pem"), caFile, nil)
	if err == nil {
		t.Fatal("expected error when certificate or key file is missing")
	}
}

func TestLoadServerTLSConfigInvalidClientCA(t *testing.T) {
	certPEM, keyPEM, _ := generateServerAndCA(t)
	dir := t.TempDir()
	certFile := writeTempFile(t, dir, "server-cert-*.pem", certPEM)
	keyFile := writeTempFile(t, dir, "server-key-*.pem", keyPEM)
	caFile := writeTempFile(t, dir, "ca-cert-*.pem", []byte("not a valid certificate"))

	_, err := LoadServerTLSConfig(certFile, keyFile, caFile, nil)
	if err == nil {
		t.Fatal("expected error when client CA file does not contain valid certificates")
	}
}

func TestVerifyPeerCertificateChecksSAN(t *testing.T) {
	certPEM, keyPEM, caPEM := generateServerAndCA(t)
	dir := t.TempDir()
	certFile := writeTempFile(t, dir, "server-cert-*.pem", certPEM)
	keyFile := writeTempFile(t, dir, "server-key-*.pem", keyPEM)
	caFile := writeTempFile(t, dir, "ca-cert-*.pem", caPEM)

	cfg, err := LoadServerTLSConfig(certFile, keyFile, caFile, nil)
	if err != nil {
		t.Fatalf("LoadServerTLSConfig: %v", err)
	}

	clientCert := createClientCert(t, "client", nil)
	if err := cfg.VerifyPeerCertificate([][]byte{clientCert.Raw}, nil); err == nil {
		t.Fatal("expected SAN validation error")
	}

	validCert := createClientCert(t, "client", []*url.URL{mustParseURL(t, "sm://user/user123"), mustParseURL(t, "sm://device/device1")})
	if err := cfg.VerifyPeerCertificate([][]byte{validCert.Raw}, nil); err != nil {
		t.Fatalf("unexpected validation error: %v", err)
	}
}

func createClientCert(t *testing.T, cn string, uris []*url.URL) *x509.Certificate {
	t.Helper()

	priv, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("generate key: %v", err)
	}

	tmpl := &x509.Certificate{
		SerialNumber:          big.NewInt(time.Now().UnixNano()),
		Subject:               pkix.Name{CommonName: cn},
		NotBefore:             time.Now().Add(-time.Hour),
		NotAfter:              time.Now().Add(24 * time.Hour),
		KeyUsage:              x509.KeyUsageDigitalSignature,
		URIs:                  uris,
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

func generateServerAndCA(t *testing.T) (certPEM, keyPEM, caCertPEM []byte) {
	t.Helper()

	caKey, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("failed to generate CA key: %v", err)
	}

	now := time.Now()
	caTemplate := &x509.Certificate{
		SerialNumber:          big.NewInt(1),
		Subject:               pkix.Name{CommonName: "Test Root CA"},
		NotBefore:             now.Add(-time.Hour),
		NotAfter:              now.Add(24 * time.Hour),
		KeyUsage:              x509.KeyUsageCertSign | x509.KeyUsageCRLSign,
		BasicConstraintsValid: true,
		IsCA:                  true,
		MaxPathLen:            1,
	}

	caDER, err := x509.CreateCertificate(rand.Reader, caTemplate, caTemplate, &caKey.PublicKey, caKey)
	if err != nil {
		t.Fatalf("failed to create CA certificate: %v", err)
	}
	caCertPEM = pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: caDER})

	serverKey, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		t.Fatalf("failed to generate server key: %v", err)
	}

	serverTemplate := &x509.Certificate{
		SerialNumber: big.NewInt(2),
		Subject:      pkix.Name{CommonName: "localhost"},
		NotBefore:    now.Add(-time.Hour),
		NotAfter:     now.Add(24 * time.Hour),
		KeyUsage:     x509.KeyUsageDigitalSignature | x509.KeyUsageKeyEncipherment,
		ExtKeyUsage:  []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
	}

	serverDER, err := x509.CreateCertificate(rand.Reader, serverTemplate, caTemplate, &serverKey.PublicKey, caKey)
	if err != nil {
		t.Fatalf("failed to create server certificate: %v", err)
	}

	certPEM = pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: serverDER})
	keyPEM = pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(serverKey)})

	return certPEM, keyPEM, caCertPEM
}

func writeTempFile(t *testing.T, dir, pattern string, data []byte) string {
	t.Helper()

	f, err := os.CreateTemp(dir, pattern)
	if err != nil {
		t.Fatalf("failed to create temp file: %v", err)
	}
	defer f.Close()

	if _, err := f.Write(data); err != nil {
		t.Fatalf("failed to write temp file: %v", err)
	}
	return f.Name()
}
