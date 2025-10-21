package auth

import (
	"context"
	smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
)

type Service struct{ smv1.UnimplementedAuthServer }

func NewService() *Service { return &Service{} }

func (s *Service) WhoAmI(ctx context.Context, _ *smv1.Empty) (*smv1.WhoAmIResponse, error) {
	// user/roles можно извлечь из клиентского сертификата (mTLS)
	return &smv1.WhoAmIResponse{
		UserId:      "user123",
		DisplayName: "Иван Петров",
		Roles:       []string{"user"},
	}, nil
}
