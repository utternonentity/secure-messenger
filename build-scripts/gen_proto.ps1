$ROOT = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$PROTO = Join-Path $ROOT 'proto'
$GOOUT = Join-Path $ROOT 'server/internal/gen'
New-Item -ItemType Directory -Force -Path $GOOUT | Out-Null
protoc -I $PROTO `
--go_out=$GOOUT --go_opt=paths=source_relative `
--go-grpc_out=$GOOUT --go-grpc_opt=paths=source_relative `
(Join-Path $PROTO '*.proto')