#!/usr/bin/env bash
set -euo pipefail

# Корень репозитория
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROTO_DIR="$ROOT_DIR/proto"
GO_OUT="$ROOT_DIR/server/internal/gen/sm/v1"   # ← без лишнего пробела!

# Убедимся, что плагины доступны (добавим $GOPATH/bin в PATH на всякий случай)
export GOPATH="${GOPATH:-$HOME/go}"
export PATH="$GOPATH/bin:/usr/local/go/bin:$PATH"

# Проверка наличия плагинов
command -v protoc-gen-go >/dev/null || { echo "❌ protoc-gen-go не найден. Выполни: go install google.golang.org/protobuf/cmd/protoc-gen-go@latest"; exit 1; }
command -v protoc-gen-go-grpc >/dev/null || { echo "❌ protoc-gen-go-grpc не найден. Выполни: go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest"; exit 1; }

mkdir -p "$GO_OUT"

# Генерация
protoc \
  -I"$PROTO_DIR" \
  --go_out="$GO_OUT" --go_opt=paths=source_relative \
  --go-grpc_out="$GO_OUT" --go-grpc_opt=paths=source_relative \
  "$PROTO_DIR"/*.proto

echo "✅ Generated into $GO_OUT"
