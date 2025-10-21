package auth

import (
	"context"
	"errors"
	"fmt"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
	"github.com/utternonentity/secure-messenger/server/internal/identity"
)

type Service struct {
	smv1.UnimplementedAuthServer
	identities *identity.Manager
}

func NewService(manager *identity.Manager) (*Service, error) {
	if manager == nil {
		return nil, fmt.Errorf("identity manager must not be nil")
	}
	return &Service{identities: manager}, nil
}

func (s *Service) WhoAmI(ctx context.Context, _ *smv1.Empty) (*smv1.WhoAmIResponse, error) {
	ident, err := s.identities.IdentityFromContext(ctx)
	if err != nil {
		return nil, mapIdentityError(err)
	}

	return &smv1.WhoAmIResponse{
		UserId:        ident.UserID,
		DisplayName:   ident.DisplayName,
		Roles:         ident.Roles,
		DeviceId:      ident.DeviceID,
		DeviceCertDer: ident.CertDER,
	}, nil
}

func mapIdentityError(err error) error {
	switch {
	case errors.Is(err, identity.ErrInvalidCertificate):
		return status.Error(codes.Unauthenticated, err.Error())
	case errors.Is(err, identity.ErrDeviceRevoked):
		return status.Error(codes.PermissionDenied, err.Error())
	case errors.Is(err, identity.ErrCertificateMismatch):
		return status.Error(codes.PermissionDenied, err.Error())
	default:
		return status.Errorf(codes.Internal, "resolve identity: %v", err)
	}
}
