# Secure Messenger — MVP Skeleton

## Предпосылки
- offline‑сборка, без внешнего интернета возможна при наличии локальных зеркал/артефактов.
- сервер: Go 1.22+, protoc + плагины go/grpc.
- клиент: Qt 6.5+ (Core, Qml, Quick), CMake 3.22+

## Шаги
1. Сгенерировать gRPC код:
   - Linux: `bash build-scripts/gen_proto.sh`
   - Windows: `powershell -ExecutionPolicy Bypass -File build-scripts/gen_proto.ps1`
2. Собрать и запустить сервер:
   ```bash
   cd server
   go build ./cmd/server
   ./server
   ```

### Переменные окружения сервера
- `SM_LISTEN_ADDR` — адрес и порт, на котором будет слушать gRPC сервер (по умолчанию `:8443`).
- `SM_TLS_CERT` — путь к PEM-файлу сертификата сервера (по умолчанию `/etc/sm/certs/server.pem`).
- `SM_TLS_KEY` — путь к PEM-файлу приватного ключа сервера (по умолчанию `/etc/sm/certs/server.key`).
- `SM_TLS_CLIENT_CA` — путь к цепочке доверенных клиентских сертификатов (по умолчанию `/etc/sm/certs/client_ca.pem`).

Все значения можно оставить по умолчанию, если сертификаты расположены согласно структуре `build-scripts`.
