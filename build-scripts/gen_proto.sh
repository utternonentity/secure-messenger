#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
PROTO_DIR="$ROOT_DIR/proto"
GO_OUT="$ROOT_DIR/server/internal/gen"


mkdir -p "$GO_OUT"
protoc \
-I"$PROTO_DIR" \
--go_out="$GO_OUT" --go_opt=paths=source_relative \
--go-grpc_out="$GO_OUT" --go-grpc_opt=paths=source_relative \
"$PROTO_DIR"/*.proto