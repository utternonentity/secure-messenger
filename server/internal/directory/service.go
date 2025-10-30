package directory

import (
	"context"
	"errors"
	"fmt"
	"os"
	"strings"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
	"github.com/utternonentity/secure-messenger/server/internal/identity"
)

type Service struct {
	smv1.UnimplementedDirectoryServer
	identities *identity.Manager
}

func NewService(manager *identity.Manager) (*Service, error) {
	if manager == nil {
		return nil, fmt.Errorf("identity manager must not be nil")
	}
	return &Service{identities: manager}, nil
}

func (s *Service) ListUsers(ctx context.Context, _ *smv1.Empty) (*smv1.ListUsersResponse, error) {
	profiles, err := s.identities.ListProfiles(ctx)
	if err != nil {
		return nil, status.Errorf(codes.Internal, "list users: %v", err)
	}
	users := make([]*smv1.UserProfile, 0, len(profiles))
	for _, profile := range profiles {
		users = append(users, convertProfile(profile))
	}
	return &smv1.ListUsersResponse{Users: users}, nil
}

func (s *Service) GetUser(ctx context.Context, id *smv1.UserId) (*smv1.UserProfile, error) {
	profile, err := s.identities.GetProfile(ctx, id.GetId())
	if err != nil {
		return nil, mapDirectoryError(err)
	}
	return convertProfile(profile), nil
}

func convertProfile(profile identity.Profile) *smv1.UserProfile {
	return &smv1.UserProfile{
		UserId:      profile.UserID,
		DisplayName: profile.Nickname,
		Devices:     nil,
	}
}

func mapDirectoryError(err error) error {
	switch {
	case errors.Is(err, identity.ErrInvalidCertificate):
		return status.Error(codes.InvalidArgument, err.Error())
	case errors.Is(err, identity.ErrCertificateMismatch):
		return status.Error(codes.FailedPrecondition, err.Error())
	default:
		if errors.Is(err, os.ErrNotExist) || strings.Contains(err.Error(), "not found") {
			return status.Error(codes.NotFound, err.Error())
		}
		return status.Errorf(codes.Internal, "directory operation failed: %v", err)
	}
}
