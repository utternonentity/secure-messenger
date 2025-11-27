package config

import (
	"fmt"
	"os"
	"strings"
)

const (
	defaultListenAddr   = ":8443"
	defaultServerCert   = "/home/yves/secure-messenger/certs/server.pem"
	defaultServerKey    = "/home/yves/secure-messenger/certs/server.key"
	defaultClientCAFile = "/home/yves/secure-messenger/client_ca.pem"
)

// TLS holds file paths for server-side mutual TLS configuration.
type TLS struct {
	CertFile     string
	KeyFile      string
	ClientCAFile string
}

// Server contains runtime configuration for the gRPC server.
type Server struct {
	ListenAddr string
	TLS        TLS
}

// Load reads configuration from environment variables, applying defaults when absent.
// Empty values are rejected so that accidental misconfiguration is caught early.
func Load() (Server, error) {
	cfg := Server{
		ListenAddr: firstNonEmpty(trimmedEnv("SM_LISTEN_ADDR"), defaultListenAddr),
		TLS: TLS{
			CertFile:     firstNonEmpty(trimmedEnv("SM_TLS_CERT"), defaultServerCert),
			KeyFile:      firstNonEmpty(trimmedEnv("SM_TLS_KEY"), defaultServerKey),
			ClientCAFile: firstNonEmpty(trimmedEnv("SM_TLS_CLIENT_CA"), defaultClientCAFile),
		},
	}

	if strings.TrimSpace(cfg.ListenAddr) == "" {
		return Server{}, fmt.Errorf("listen address must not be empty")
	}
	if strings.TrimSpace(cfg.TLS.CertFile) == "" {
		return Server{}, fmt.Errorf("server certificate path must not be empty")
	}
	if strings.TrimSpace(cfg.TLS.KeyFile) == "" {
		return Server{}, fmt.Errorf("server key path must not be empty")
	}
	if strings.TrimSpace(cfg.TLS.ClientCAFile) == "" {
		return Server{}, fmt.Errorf("client CA bundle path must not be empty")
	}

	return cfg, nil
}

func trimmedEnv(key string) string {
	return strings.TrimSpace(os.Getenv(key))
}

func firstNonEmpty(values ...string) string {
	for _, v := range values {
		if v != "" {
			return v
		}
	}
	return ""
}
