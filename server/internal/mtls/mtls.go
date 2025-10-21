package mtls

import (
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"os"
	"strings"
)

// ClientValidator is invoked for each authenticated client certificate.
type ClientValidator func(*x509.Certificate) error

func LoadServerTLSConfig(certFile, keyFile, clientCAFile string, validator ClientValidator) (*tls.Config, error) {
	cert, err := tls.LoadX509KeyPair(certFile, keyFile)
	if err != nil {
		return nil, err
	}
	caBytes, err := os.ReadFile(clientCAFile)
	if err != nil {
		return nil, err
	}
	clientCAPool := x509.NewCertPool()
	if ok := clientCAPool.AppendCertsFromPEM(caBytes); !ok {
		return nil, fmt.Errorf("failed to append client CA certs from %s", clientCAFile)
	}
	cfg := &tls.Config{
		Certificates: []tls.Certificate{cert},
		ClientCAs:    clientCAPool,
		ClientAuth:   tls.RequireAndVerifyClientCert,
		MinVersion:   tls.VersionTLS13,
	}

	cfg.VerifyPeerCertificate = func(rawCerts [][]byte, _ [][]*x509.Certificate) error {
		if len(rawCerts) == 0 {
			return fmt.Errorf("no client certificate provided")
		}
		clientCert, err := x509.ParseCertificate(rawCerts[0])
		if err != nil {
			return fmt.Errorf("parse client certificate: %w", err)
		}
		if strings.TrimSpace(clientCert.Subject.CommonName) == "" {
			return fmt.Errorf("client certificate missing CommonName")
		}
		var hasUser, hasDevice bool
		for _, uri := range clientCert.URIs {
			if uri == nil || !strings.EqualFold(uri.Scheme, "sm") {
				continue
			}
			switch strings.ToLower(uri.Host) {
			case "user":
				hasUser = true
			case "device":
				hasDevice = true
			}
		}
		if !hasUser || !hasDevice {
			return fmt.Errorf("client certificate missing sm://user and sm://device SAN entries")
		}
		if validator != nil {
			if err := validator(clientCert); err != nil {
				return err
			}
		}
		return nil
	}

	return cfg, nil
}
