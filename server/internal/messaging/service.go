package messaging


import (
"context"
smv1 "github.com/utternonentity/secure-messenger/server/internal/gen/sm/v1"
)


type Service struct {
smv1.UnimplementedMessagingServer
store []*smv1.EncryptedEnvelope // простая память для MVP
}


func NewService(_ context.Context) *Service { return &Service{store: make([]*smv1.EncryptedEnvelope, 0)} }


func (s *Service) Send(ctx context.Context, env *smv1.EncryptedEnvelope) (*smv1.SendResponse, error) {
s.store = append(s.store, env)
return &smv1.SendResponse{ServerMsgId: "m-" + env.Meta.SenderUserId}, nil
}


func (s *Service) Pull(req *smv1.PullRequest, stream smv1.Messaging_PullServer) error {
// Игнорируем since_server_msg_id для MVP
for _, e := range s.store {
if err := stream.Send(e); err != nil { return err }
}
return nil
}