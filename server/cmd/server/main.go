package main

import (
	"crypto/x509"
	"errors"
	"flag"
	"log"
	"net"
	"net/http"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"

	"github.com/utternonentity/secure-messenger/server/internal/auth"
	"github.com/utternonentity/secure-messenger/server/internal/directory"
	smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
	"github.com/utternonentity/secure-messenger/server/internal/identity"
	"github.com/utternonentity/secure-messenger/server/internal/messaging"
	"github.com/utternonentity/secure-messenger/server/internal/mtls"
)

func main() {
	certPath := flag.String("cert", "/etc/sm/certs/server.pem", "Path to the server TLS certificate")
	keyPath := flag.String("key", "/etc/sm/certs/server.key", "Path to the server TLS private key")
	clientCAPath := flag.String("client-ca", "/etc/sm/certs/client_ca.pem", "Path to the client CA bundle")
	listenAddr := flag.String("listen", ":8443", "Address the server should listen on")
	storePath := flag.String("store", "data/messages.db", "Path to the message store file")
	identityPath := flag.String("identity-store", "data/identity_store.json", "Path to the identity store file")
	httpListenAddr := flag.String("http-listen", ":8080", "Address the HTTP API should listen on")
	flag.Parse()

	if err := identity.EnsureSeedData(*identityPath); err != nil {
		log.Fatalf("seed identity store: %v", err)
	}
	if err := messaging.EnsureSeedData(*storePath); err != nil {
		log.Fatalf("seed message store: %v", err)
	}

	identityManager, err := identity.NewManager(*identityPath)
	if err != nil {
		log.Fatalf("init identity store: %v", err)
	}
	defer func() {
		if err := identityManager.Close(); err != nil {
			log.Printf("close identity store: %v", err)
		}
	}()

	// Загрузка mTLS (сертификат сервера + доверенные CA для клиентов)
	cfg, err := mtls.LoadServerTLSConfig(*certPath, *keyPath, *clientCAPath, func(cert *x509.Certificate) error {
		_, err := identityManager.ValidateCertificate(cert)
		return err
	})
	if err != nil {
		log.Fatalf("TLS load: %v", err)
	}

	lis, err := net.Listen("tcp", *listenAddr)
	if err != nil {
		log.Fatalf("listen: %v", err)
	}

	srv := grpc.NewServer(grpc.Creds(credentials.NewTLS(cfg)))

	msgStore, err := messaging.NewStore(*storePath)
	if err != nil {
		log.Fatalf("init message store: %v", err)
	}
	defer func() {
		if err := msgStore.Close(); err != nil {
			log.Printf("close message store: %v", err)
		}
	}()

	messagingService, err := messaging.NewService(msgStore)
	if err != nil {
		log.Fatalf("init messaging service: %v", err)
	}

	httpHandler, err := messaging.NewHTTPHandler(messagingService)
	if err != nil {
		log.Fatalf("init messaging http handler: %v", err)
	}

	authService, err := auth.NewService(identityManager)
	if err != nil {
		log.Fatalf("init auth service: %v", err)
	}
	directoryService, err := directory.NewService(identityManager)
	if err != nil {
		log.Fatalf("init directory service: %v", err)
	}

	smv1.RegisterAuthServer(srv, authService)
	smv1.RegisterDirectoryServer(srv, directoryService)
	smv1.RegisterMessagingServer(srv, messagingService)

	go func() {
		httpSrv := &http.Server{Addr: *httpListenAddr, Handler: httpHandler}
		log.Printf("secure-messenger HTTP API listening on %s", httpSrv.Addr)
		if err := httpSrv.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			log.Fatalf("http serve: %v", err)
		}
	}()

	log.Printf("secure-messenger server listening on %s", lis.Addr())
	if err := srv.Serve(lis); err != nil {
		log.Fatalf("serve: %v", err)
	}
}
