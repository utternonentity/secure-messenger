# Secure Messenger — MVP Skeleton


## Предпосылки
- offline‑сборка, без внешнего интернета возможна при наличии локальных зеркал/артефактов.
- сервер: Go 1.22+, protoc + плагины go/grpc.
- клиент: Qt 6.5+ (Core, Qml, Quick), CMake 3.22+


## Шаги
1. Сгенерировать gRPC код:
- Linux: `bash build-scripts/gen_proto.sh`
- Windows: `powershell -ExecutionPolicy Bypass -File build-scripts/gen_proto.ps1`
2. Собрать сервер:
```bash
cd server
go build ./cmd/server
./server