package mtls

import (
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"os"
)

func LoadServerTLSConfig(certFile, keyFile, clientCAFile string) (*tls.Config, error) {
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
	return &tls.Config{
		Certificates: []tls.Certificate{cert},
		ClientCAs:    clientCAPool,
		ClientAuth:   tls.RequireAndVerifyClientCert,
		MinVersion:   tls.VersionTLS13,
	}, nil
}
