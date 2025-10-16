package directory


import (
"context"
smv1 "github.com/example/secure-messenger/server/internal/gen/sm/v1"
)


type Service struct { smv1.UnimplementedDirectoryServer }


func NewService() *Service { return &Service{} }


func (s *Service) ListUsers(ctx context.Context, _ *smv1.Empty) (*smv1.ListUsersResponse, error) {
return &smv1.ListUsersResponse{Users: []*smv1.UserProfile{ {UserId: "user123", DisplayName: "Иван Петров"} }}, nil
}


func (s *Service) GetUser(ctx context.Context, id *smv1.UserId) (*smv1.UserProfile, error) {
return &smv1.UserProfile{UserId: id.Id, DisplayName: "Иван Петров"}, nil
}