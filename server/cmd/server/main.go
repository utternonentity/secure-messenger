package main

import (
	"context"
	"crypto/tls"
	"crypto/x509"
	"errors"
	"flag"
	"log"
	"net"
	"net/http"
	"os"
	"strings"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"

	"github.com/roofn/secure-messenger/server/internal/auth"
	"github.com/roofn/secure-messenger/server/internal/directory"
	smv1 "github.com/roofn/secure-messenger/server/internal/gen/sm/v1"
	"github.com/roofn/secure-messenger/server/internal/identity"
	"github.com/roofn/secure-messenger/server/internal/logging"
	"github.com/roofn/secure-messenger/server/internal/messaging"
	"github.com/roofn/secure-messenger/server/internal/mtls"
	"github.com/roofn/secure-messenger/server/internal/storage"
)

type serverConfig struct {
	certPath       string
	keyPath        string
	clientCAPath   string
	listenAddr     string
	storePath      string
	identityPath   string
	httpListenAddr string
	messageKey     string
	logDir         string
}

type messageStore interface {
	Save(context.Context, *smv1.EncryptedEnvelope) (int64, error)
	ForEachSince(context.Context, int64, func(messaging.StoredEnvelope) error) error
	Close() error
}

func main() {
	cfg := parseConfig()
	logFile := setupLogging(cfg.logDir)
	defer closeLogFile(logFile)

	identityStore := resolveDataPath(cfg.identityPath, "identity store")
	messageStore := resolveDataPath(cfg.storePath, "message store")

	cipher := mustMessageCipher(cfg.messageKey)
	prepareSeedData(identityStore.Primary, messageStore.Primary, cipher)

	identityManager := mustIdentityManager(identityStore.Primary)
	defer closeIdentityManager(identityManager)

	tlsCfg := mustLoadTLSConfig(cfg, identityManager)
	lis := listenOrExit(cfg.listenAddr)
	srv := grpc.NewServer(grpc.Creds(credentials.NewTLS(tlsCfg)))

	msgStore := mustMessageStore(messageStore.Primary, cipher)
	defer closeMessageStore(msgStore)
	messagingService := mustMessagingService(msgStore)

	tokenManager := auth.NewTokenManager(30 * time.Minute)
	authService := mustAuthService(identityManager)
	directoryService := mustDirectoryService(identityManager)

	smv1.RegisterAuthServer(srv, authService)
	smv1.RegisterDirectoryServer(srv, directoryService)
	smv1.RegisterMessagingServer(srv, messagingService)

	startHTTPServer(cfg.httpListenAddr, mustHTTPMux(messagingService, identityManager, tokenManager, cipher))

	log.Printf("secure-messenger server listening on %s", lis.Addr())
	if err := srv.Serve(lis); err != nil {
		log.Fatalf("serve: %v", err)
	}
}

func envOrDefault(key, fallback string) string {
	if val := strings.TrimSpace(os.Getenv(key)); val != "" {
		return val
	}
	return fallback
}

func parseConfig() serverConfig {
	certPath := flag.String("cert", envOrDefault("SM_TLS_CERT", "/opt/secure-messenger/certs/server.pem"), "Path to the server TLS certificate")
	keyPath := flag.String("key", envOrDefault("SM_TLS_KEY", "/opt/secure-messenger/certs/server.key"), "Path to the server TLS private key")
	clientCAPath := flag.String("client-ca", envOrDefault("SM_TLS_CLIENT_CA", "/opt/secure-messenger/certs/client_ca.pem"), "Path to the client CA bundle")
	listenAddr := flag.String("listen", envOrDefault("SM_LISTEN_ADDR", ":8443"), "Address the server should listen on")
	storePath := flag.String("store", envOrDefault("SM_STORE", "data/messages.db"), "Path to the message store file")
	identityPath := flag.String("identity-store", envOrDefault("SM_IDENTITY_STORE", "data/identity.db"), "Path to the identity store file")
	httpListenAddr := flag.String("http-listen", ":8080", "Address the HTTP API should listen on")
	messageKey := flag.String("message-key", envOrDefault("SM_MESSAGE_KEY", messaging.DefaultMessageKeyBase64), "Base64-encoded AES-256 key for encrypting HTTP messages")
	logDir := flag.String("log-dir", "data/logs", "Directory where server logs should be stored")
	flag.Parse()

	return serverConfig{
		certPath:       *certPath,
		keyPath:        *keyPath,
		clientCAPath:   *clientCAPath,
		listenAddr:     *listenAddr,
		storePath:      *storePath,
		identityPath:   *identityPath,
		httpListenAddr: *httpListenAddr,
		messageKey:     *messageKey,
		logDir:         *logDir,
	}
}

func resolveDataPath(path, label string) storage.Resolution {
	dataPath, err := storage.ResolveDataPath(path)
	if err != nil {
		log.Fatalf("resolve %s: %v", label, err)
	}
	for _, legacy := range dataPath.Redundant {
		log.Printf("legacy %s detected at %s; delete it to avoid confusion", label, legacy)
	}
	return dataPath
}

func mustMessageCipher(messageKey string) *messaging.AESGCMCipher {
	cipher, err := messaging.NewAESGCMCipherFromBase64(messageKey)
	if err != nil {
		log.Fatalf("init message cipher: %v", err)
	}
	return cipher
}

func prepareSeedData(identityPath, messagePath string, cipher *messaging.AESGCMCipher) {
	if err := identity.EnsureSeedData(identityPath); err != nil {
		log.Fatalf("seed identity store: %v", err)
	}
	if err := messaging.EnsureSeedData(messagePath, cipher); err != nil {
		log.Fatalf("seed message store: %v", err)
	}
}

func mustIdentityManager(identityPath string) *identity.Manager {
	identityManager, err := identity.NewManager(identityPath)
	if err != nil {
		log.Fatalf("init identity store: %v", err)
	}
	return identityManager
}

func closeIdentityManager(manager *identity.Manager) {
	if err := manager.Close(); err != nil {
		log.Printf("close identity store: %v", err)
	}
}

func mustLoadTLSConfig(cfg serverConfig, identityManager *identity.Manager) *tls.Config {
	tlsCfg, err := mtls.LoadServerTLSConfig(cfg.certPath, cfg.keyPath, cfg.clientCAPath, func(cert *x509.Certificate) error {
		_, err := identityManager.ValidateCertificate(cert)
		return err
	})
	if err != nil {
		log.Fatalf("TLS load: %v", err)
	}
	return tlsCfg
}

func listenOrExit(listenAddr string) net.Listener {
	lis, err := net.Listen("tcp", listenAddr)
	if err != nil {
		log.Fatalf("listen: %v", err)
	}
	return lis
}

func mustMessageStore(messagePath string, cipher messaging.EnvelopeCipher) messageStore {
	msgStore, err := messaging.NewStore(messagePath, cipher)
	if err != nil {
		log.Fatalf("init message store: %v", err)
	}
	return msgStore
}

func closeMessageStore(msgStore messageStore) {
	if err := msgStore.Close(); err != nil {
		log.Printf("close message store: %v", err)
	}
}

func mustMessagingService(msgStore messageStore) *messaging.Service {
	messagingService, err := messaging.NewService(msgStore)
	if err != nil {
		log.Fatalf("init messaging service: %v", err)
	}
	return messagingService
}

func mustAuthService(identityManager *identity.Manager) *auth.Service {
	authService, err := auth.NewService(identityManager)
	if err != nil {
		log.Fatalf("init auth service: %v", err)
	}
	return authService
}

func mustDirectoryService(identityManager *identity.Manager) *directory.Service {
	directoryService, err := directory.NewService(identityManager)
	if err != nil {
		log.Fatalf("init directory service: %v", err)
	}
	return directoryService
}

func mustHTTPMux(messagingService *messaging.Service, identityManager *identity.Manager, tokenManager *auth.TokenManager, cipher *messaging.AESGCMCipher) *http.ServeMux {
	httpMessagesHandler, err := messaging.NewHTTPHandler(messagingService, tokenManager, cipher)
	if err != nil {
		log.Fatalf("init messaging http handler: %v", err)
	}
	httpAuthHandler, err := auth.NewHTTPHandler(identityManager, tokenManager)
	if err != nil {
		log.Fatalf("init auth http handler: %v", err)
	}
	httpMux := http.NewServeMux()
	httpMux.Handle("/api/auth/", httpAuthHandler)
	httpMux.Handle("/", httpMessagesHandler)
	return httpMux
}

func startHTTPServer(listenAddr string, mux *http.ServeMux) {
	go func() {
		httpSrv := &http.Server{Addr: listenAddr, Handler: mux}
		log.Printf("secure-messenger HTTP API listening on %s", httpSrv.Addr)
		if err := httpSrv.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			log.Fatalf("http serve: %v", err)
		}
	}()
}

func setupLogging(logDir string) *os.File {
	logFile, err := logging.Setup(logDir)
	if err != nil {
		log.Fatalf("init logging: %v", err)
	}
	return logFile
}

func closeLogFile(logFile *os.File) {
	if logFile == nil {
		return
	}
	if err := logFile.Close(); err != nil {
		log.Printf("close log file: %v", err)
	}
}
