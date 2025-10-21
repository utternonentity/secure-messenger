package main

import (
	"flag"
	"log"
	"net"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"

	"github.com/utternonentity/secure-messenger/server/internal/auth"
	"github.com/utternonentity/secure-messenger/server/internal/directory"
	smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
	"github.com/utternonentity/secure-messenger/server/internal/messaging"
	"github.com/utternonentity/secure-messenger/server/internal/mtls"
)

func main() {
	certPath := flag.String("cert", "/etc/sm/certs/server.pem", "Path to the server TLS certificate")
	keyPath := flag.String("key", "/etc/sm/certs/server.key", "Path to the server TLS private key")
	clientCAPath := flag.String("client-ca", "/etc/sm/certs/client_ca.pem", "Path to the client CA bundle")
	listenAddr := flag.String("listen", ":8443", "Address the server should listen on")
	storePath := flag.String("store", "sm_messages.db", "Path to the message store file")
	flag.Parse()

	// Загрузка mTLS (сертификат сервера + доверенные CA для клиентов)
	cfg, err := mtls.LoadServerTLSConfig(*certPath, *keyPath, *clientCAPath)
	if err != nil {
		log.Fatalf("TLS load: %v", err)
	}

	lis, err := net.Listen("tcp", *listenAddr)
	if err != nil {
		log.Fatalf("listen: %v", err)
	}

	srv := grpc.NewServer(grpc.Creds(credentials.NewTLS(cfg)))

	msgStore, err := messaging.NewFileStore(*storePath)
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

	smv1.RegisterAuthServer(srv, auth.NewService())
	smv1.RegisterDirectoryServer(srv, directory.NewService())
	smv1.RegisterMessagingServer(srv, messagingService)

	log.Printf("secure-messenger server listening on %s", lis.Addr())
	if err := srv.Serve(lis); err != nil {
		log.Fatalf("serve: %v", err)
	}
}
