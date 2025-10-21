package identity

import (
	"context"
	"crypto/x509"

	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/peer"
)

type grpcPeer struct {
	info credentials.TLSInfo
}

func (p grpcPeer) TLSCertificates() []*x509.Certificate {
	return p.info.State.PeerCertificates
}

func peerFromContext(ctx context.Context) (interface{ TLSCertificates() []*x509.Certificate }, bool) {
	pr, ok := peer.FromContext(ctx)
	if !ok {
		return nil, false
	}
	tlsInfo, ok := pr.AuthInfo.(credentials.TLSInfo)
	if !ok {
		return nil, false
	}
	return grpcPeer{info: tlsInfo}, true
}
