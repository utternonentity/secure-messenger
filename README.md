# Secure Messenger — MVP Skeleton

## Предпосылки
- офлайн‑сборка возможна при наличии локальных зеркал/артефактов.
- сервер: Go 1.22+, `protoc` + плагины go/grpc.
- клиент: Qt 6.5+ (Core, Qml, Quick), CMake 3.22+.

## Генерация gRPC артефактов
Запустите генерацию protobuf перед сборкой серверной части и клиентов:

```bash
# Linux
bash build-scripts/gen_proto.sh

# Windows
powershell -ExecutionPolicy Bypass -File build-scripts/gen_proto.ps1
```

## Подготовка тестовых сертификатов для mTLS
Сервер и клиенты обмениваются сообщениями только через mTLS. Для локальной демонстрации можно выпустить собственные сертификаты при помощи `openssl`.

1. Создайте рабочую папку и корневой центр сертификации (CA):
   ```bash
   mkdir -p certs
   openssl genrsa -out certs/rootCA.key 4096
   openssl req -x509 -new -key certs/rootCA.key -sha256 -days 365 -out certs/rootCA.pem \
     -subj "/CN=SM Demo Root"
   cp certs/rootCA.pem certs/client_ca.pem   # доверенный CA для клиентов
   ```
2. Выпустите сертификат сервера (`server.pem`) с SAN `localhost` и `127.0.0.1`:
   ```bash
   cat > certs/server.cnf <<'EOF'
   [req]
   default_bits = 4096
   prompt = no
   default_md = sha256
   req_extensions = req_ext
   distinguished_name = dn

   [dn]
   CN = secure-messenger.local

   [req_ext]
   subjectAltName = @alt_names

   [alt_names]
   DNS.1 = secure-messenger.local
   DNS.2 = localhost
   IP.1 = 127.0.0.1
   EOF

   openssl genrsa -out certs/server.key 4096
   openssl req -new -key certs/server.key -out certs/server.csr -config certs/server.cnf
   openssl x509 -req -in certs/server.csr -CA certs/rootCA.pem -CAkey certs/rootCA.key -CAcreateserial \
     -out certs/server.pem -days 365 -sha256 -extensions req_ext -extfile certs/server.cnf
   ```
3. Выпустите клиентский сертификат пользователя. Он понадобится при регистрации и для последующих подключений по mTLS:
   ```bash
   cat > certs/user-alice.cnf <<'EOF'
   [req]
   default_bits = 4096
   prompt = no
   default_md = sha256
   req_extensions = req_ext
   distinguished_name = dn

   [dn]
   CN = Alice Doe

   [req_ext]
   subjectAltName = email:alice@example.org
   EOF

   openssl genrsa -out certs/user-alice.key 4096
   openssl req -new -key certs/user-alice.key -out certs/user-alice.csr -config certs/user-alice.cnf
   openssl x509 -req -in certs/user-alice.csr -CA certs/rootCA.pem -CAkey certs/rootCA.key -CAcreateserial \
     -out certs/user-alice.pem -days 365 -sha256 -extensions req_ext -extfile certs/user-alice.cnf
   ```

Эти шаги можно повторить для любого числа пользователей, меняя CN и дополнительные поля. Файлы `*.pem` копируются на соответствующие
 узлы, а `client_ca.pem` устанавливается как доверенный корень на сервере. При регистрации новый пользователь передает DER-кодированный
 сертификат в base64 (см. раздел про HTTP API ниже).

## Сборка и запуск сервера
```bash
cd server
go build ./cmd/server

# Запуск (пути и адреса можно переопределять флагами/переменными окружения)
SM_TLS_CERT=../certs/server.pem \
SM_TLS_KEY=../certs/server.key \
SM_TLS_CLIENT_CA=../certs/client_ca.pem \
SM_LISTEN_ADDR=:8443 \
./server --http-listen :8080
```

### Переменные окружения сервера
- `SM_LISTEN_ADDR` — адрес и порт gRPC сервера (по умолчанию `:8443`).
- `SM_TLS_CERT` — путь к PEM-сертификату сервера (по умолчанию `/etc/sm/certs/server.pem`).
- `SM_TLS_KEY` — путь к приватному ключу сервера (по умолчанию `/etc/sm/certs/server.key`).
- `SM_TLS_CLIENT_CA` — набор доверенных клиентских CA (по умолчанию `/etc/sm/certs/client_ca.pem`).
- HTTP API для истории сообщений и публикации новых записей слушает адрес, переданный флагом `--http-listen` (по умолчанию `:8080`).
- Пути к файлам хранилищ можно задать флагами `--store` и `--identity-store`, если требуется нестандартный путь.

Идентификационные и журнальные БД (`data/identity_store.json`, `data/messages.db`) создаются автоматически при первом запуске сервера. Их можно переопределить флагами `--identity-store` и `--store`.

> ⚠️ Начиная с этой версии сервер всегда ищет базы данных относительно корня репозитория. Если у вас остались старые копии в `server/data/` или `server/sm_messages.db`, они больше не используются. Удалите их во избежание путаницы:
> - `server/data/identity_store.json` — устаревшая копия справочника пользователей.
> - `server/data/messages.db` — устаревшая копия истории сообщений.
> - `server/sm_messages.db` — временный файл, не задействованный в приложении.

## Подключение клиентов и обмен сообщениями
1. На каждом устройстве установите Qt 6.5+ и соберите демонстрационный клиент:
   ```bash
   cmake -S client-qt -B build/client-qt -GNinja
   cmake --build build/client-qt
   ```
2. Убедитесь, что сервер запущен и HTTP API доступен (по умолчанию `https://<host>:8080` при использовании mTLS, либо `http://` при тестовом запуске без прокси).
3. Настройте клиент на использование нужного HTTP-адреса. По умолчанию используется `http://127.0.0.1:8080`. Чтобы переключиться на удалённый сервер, перед запуском задайте
   ```bash
   export SM_HTTP_API="http://10.0.0.5:8080"
   export SM_AUTH_USER_ID="user-0002"   # (опционально) выбрать пользователя каталога
   ./build/client-qt/sm_client
   ```
   При старте приложение загружает историю сообщений с сервера, отображает её в UI и каждые несколько секунд запрашивает новые записи. Любое отправленное сообщение сразу уходит на сервер и становится доступным для других клиентов.
4. Повторите шаги на другом устройстве (или запустите ещё один экземпляр на той же машине), указав другую переменную `SM_AUTH_USER_ID`. Оба клиента будут видеть общий список сообщений из файла `data/messages.db`, который хранится на сервере и автоматически пополняется.

### HTTP API сообщений
HTTP интерфейс позволяет интегрировать любые дополнительные клиенты. Основные запросы:
- `GET /api/messages?since_id=msg-5` — возвращает JSON со списком сообщений (опционально только с указанного идентификатора).
- `POST /api/messages` — принимает JSON вида:
  ```json
  {
    "conversation_id": "corp-secure-room",
    "sender_user_id": "user-0001",
    "text": "Привет!"
  }
  ```
  В ответ сервер возвращает идентификатор сохранённого сообщения и отметку времени.

### HTTP API регистрации
Для добавления нового пользователя отправьте POST-запрос на `/api/auth/register` с JSON:
```json
{
  "nickname": "alice",
  "certificate": "<DER сертификата в base64>"
}
```
Получить base64-строку можно так:
```bash
openssl x509 -in certs/user-alice.pem -outform DER | base64 -w0
```
В ответе вернётся `user_id`, присвоенный сервером. Этот же сертификат используется при mTLS-подключении.


### Примеры для gRPC (опционально)
Для глубокой интеграции остаётся доступен исходный gRPC интерфейс. Ниже приведены команды `grpcurl` для тестирования `Pull`/`Send` в обход HTTP API.
- Подписка на канал:
  ```bash
  grpcurl -cacert certs/rootCA.pem -cert certs/device-laptop.pem -key certs/device-laptop.key \
    -d '{"sinceServerMsgId":"","conversationIds":["corp-secure-room"]}' \
    localhost:8443 sm.v1.Messaging/Pull
  ```
- Отправка сообщения:
  ```bash
  grpcurl -cacert certs/rootCA.pem -cert certs/device-phone.pem -key certs/device-phone.key \
    -d '{
          "meta": {
            "conversationId": "corp-secure-room",
            "senderUserId": "user-ivan",
            "senderDeviceId": "device-phone",
            "sentUnixSec": 1700000000
          },
          "ciphertext": "dGVtb19lbmNyeXB0ZWRfZGF0YQ=="
        }' \
    localhost:8443 sm.v1.Messaging/Send
  ```

## Что дальше
- Реализуйте gRPC-вызовы в Qt-клиенте, используя сгенерированные protobuf-стабы.
- Добавьте хранение секретов и реальные криптопримитивы поверх `ciphertext`.
- Расширьте README собственными сценариями деплоя (Docker/Kubernetes, systemd и т.д.), если они появятся.
