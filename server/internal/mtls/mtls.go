package mtls


import (
"crypto/tls"
"crypto/x509"
"os"
)


func LoadServerTLSConfig(certFile, keyFile, clientCAFile string) (*tls.Config, error) {
cert, err := tls.LoadX509KeyPair(certFile, keyFile)
if err != nil { return nil, err }
caBytes, err := os.ReadFile(clientCAFile)
if err != nil { return nil, err }
clientCAPool := x509.NewCertPool()
clientCAPool.AppendCertsFromPEM(caBytes)
return &tls.Config{
Certificates: []tls.Certificate{cert},
ClientCAs: clientCAPool,
ClientAuth: tls.RequireAndVerifyClientCert,
MinVersion: tls.VersionTLS13,
}, nil
}