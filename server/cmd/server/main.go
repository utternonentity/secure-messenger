package main


import (
"context"
"crypto/tls"
"log"
"net"


"google.golang.org/grpc"
"google.golang.org/grpc/credentials"


smv1 "github.com/example/secure-messenger/server/internal/gen/sm/v1"
"github.com/example/secure-messenger/server/internal/auth"
"github.com/example/secure-messenger/server/internal/directory"
"github.com/example/secure-messenger/server/internal/messaging"
"github.com/example/secure-messenger/server/internal/mtls"
)


func main() {
// Загрузка mTLS (сертификат сервера + доверенные CA для клиентов)
cfg, err := mtls.LoadServerTLSConfig("/etc/sm/certs/server.pem", "/etc/sm/certs/server.key", "/etc/sm/certs/client_ca.pem")
if err != nil { log.Fatalf("TLS load: %v", err) }


lis, err := net.Listen("tcp", ":8443")
if err != nil { log.Fatalf("listen: %v", err) }


srv := grpc.NewServer(grpc.Creds(credentials.NewTLS(cfg)))
ctx := context.Background()


smv1.RegisterAuthServer(srv, auth.NewService())
smv1.RegisterDirectoryServer(srv, directory.NewService())
smv1.RegisterMessagingServer(srv, messaging.NewService(ctx))


log.Printf("secure-messenger server listening on %s", lis.Addr())
if err := srv.Serve(lis); err != nil { log.Fatalf("serve: %v", err) }
}