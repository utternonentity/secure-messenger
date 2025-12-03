# Secure Messenger

Secure Messenger — это минимально жизнеспособный пример корпоративного мессенджера с mTLS-авторизацией устройств. В репозитории есть:

- сервер на Go (`server/`), предоставляющий gRPC и HTTP API;
- Qt/QML-клиент (`client-qt/`);
- protobuf-схемы (`proto/`) и генераторы (`build-scripts/`).

Документ описывает полный цикл запуска сервера на собственном узле, выпуск сертификатов, передачу их пользователям, регистрацию новых аккаунтов и работу с клиентом.

## 1. Требования

### Общие
- Возможность сборки без доступа в интернет (заготовьте артефакты заранее при необходимости).
- `git`, `cmake`, `ninja` (или `make`).

### Сервер
- Go 1.22 или новее.
- `protoc` 3.21+ и плагины `protoc-gen-go`, `protoc-gen-go-grpc`.
- OpenSSL 1.1+ для генерации сертификатов.

### Клиент
- Qt 6.5+ (модули Core, Qml, Quick).
- Компилятор C++17 (gcc/clang/msvc).

## 2. Структура репозитория
- `build-scripts/` — скрипты генерации protobuf артефактов.
- `client-qt/` — исходники демонстрационного клиента.
- `data/` — файлы БД сервера (создаются/обновляются при запуске).
- `proto/` — gRPC/HTTP схемы.
- `server/` — код серверного приложения.

## 3. Генерация gRPC артефактов
Перед первой сборкой синхронизируйте protobuf-заготовки:

```bash
# Linux/macOS
bash build-scripts/gen_proto.sh

# Windows
powershell -ExecutionPolicy Bypass -File build-scripts/gen_proto.ps1
```

Команда пересоздаст файлы в `server/internal/gen` и клиентские stubs.

## 4. Сборка

### Сервер
```bash
cd server
go build ./cmd/server
```

Бинарный файл `server` появится в каталоге `server/`.

### Клиент Qt
```bash
cmake -S client-qt -B build/client-qt -GNinja
cmake --build build/client-qt
```

#### Установщик клиента
Для сборки и установки клиентского бинарника воспользуйтесь готовыми скриптами. По умолчанию артефакты кладутся в `dist/client`:

```bash
bash build-scripts/install_client.sh
```

На Windows используйте PowerShell-версию:

```powershell
powershell -ExecutionPolicy Bypass -File build-scripts/install_client.ps1
```

Скрипты принимают параметры для смены каталога сборки (`-b`/`-BuildDir`) и префикса установки (`-p`/`-Prefix`). Если установлен Ninja,
он выбирается автоматически; при необходимости передайте иной генератор (`-g`/`-Generator`).

Исполняемый файл располагается в `build/client-qt/sm_client` (путь может отличаться на Windows/macOS).

## 5. Настройка серверного узла

1. Создайте рабочий каталог и структуру хранения сертификатов и данных:
   ```bash
   sudo mkdir -p /opt/secure-messenger/{bin,certs,data,logs}
   sudo chown -R $USER: /opt/secure-messenger
   ```
2. Скопируйте собранный сервер:
   ```bash
   cp server/server /opt/secure-messenger/bin/
   ```
3. Определите переменные окружения (например, в `/etc/secure-messenger.env`):
   ```bash
   cat <<'ENV' | sudo tee /etc/secure-messenger.env
   SM_LISTEN_ADDR=:8443
   SM_TLS_CERT=/opt/secure-messenger/certs/server.pem
   SM_TLS_KEY=/opt/secure-messenger/certs/server.key
   SM_TLS_CLIENT_CA=/opt/secure-messenger/certs/client_ca.pem
   SM_MESSAGE_KEY=KpEyIdHR3J8zvm64LKGhXgeOy4cmh09YkHxAUlPAuro=
   SM_STORE=/opt/secure-messenger/data/messages.db
   SM_IDENTITY_STORE=/opt/secure-messenger/data/identity.db
   ENV
   ```
   `SM_MESSAGE_KEY` — base64-кодированный 32-байтовый ключ для AES-256-GCM, которым шифруются полезные нагрузки HTTP API. В
   примере указан тестовый ключ, в бою используйте собственный.
4. (Опционально) создайте `systemd` юнит `/etc/systemd/system/secure-messenger.service`:
   ```ini
   [Unit]
   Description=Secure Messenger Server
   After=network.target

   [Service]
   EnvironmentFile=/etc/secure-messenger.env
   ExecStart=/opt/secure-messenger/bin/server --http-listen :8080
   WorkingDirectory=/opt/secure-messenger
   Restart=on-failure
   StandardOutput=append:/opt/secure-messenger/logs/server.log
   StandardError=append:/opt/secure-messenger/logs/server.log

   [Install]
   WantedBy=multi-user.target
   ```
   Активируйте сервис:
   ```bash
   sudo systemctl daemon-reload
   sudo systemctl enable --now secure-messenger.service
   ```

Для ручного запуска используйте:
```bash
cd /opt/secure-messenger
SM_TLS_CERT=certs/server.pem \
SM_TLS_KEY=certs/server.key \
SM_TLS_CLIENT_CA=certs/client_ca.pem \
SM_LISTEN_ADDR=:8443 \
SM_MESSAGE_KEY=KpEyIdHR3J8zvm64LKGhXgeOy4cmh09YkHxAUlPAuro= \
SM_STORE=data/messages.db \
SM_IDENTITY_STORE=data/identity.db \
./bin/server --http-listen :8080
```

HTTP API по умолчанию слушает `:8080`, gRPC — `:8443`.

## 6. Выпуск сертификатов

Сервер и клиенты идентифицируются через mTLS. Один корневой центр сертификации (CA) выпускает сертификаты для сервера и устройств.

### 6.1 Создание корневого CA
```bash
mkdir -p /opt/secure-messenger/certs
cd /opt/secure-messenger/certs
openssl genrsa -out rootCA.key 4096
openssl req -x509 -new -key rootCA.key -sha256 -days 825 -out rootCA.pem \
  -subj "/CN=Secure Messenger Root"
cp rootCA.pem client_ca.pem
```

### 6.2 Выпуск серверного сертификата
```bash
cat > server.cnf <<'EOF'
[req]
default_bits = 4096
prompt = no
default_md = sha256
req_extensions = req_ext
distinguished_name = dn

[dn]
CN = secure-messenger.internal

[req_ext]
subjectAltName = @alt_names

[alt_names]
DNS.1 = secure-messenger.internal
DNS.2 = localhost
IP.1 = 127.0.0.1
EOF

openssl genrsa -out server.key 4096
openssl req -new -key server.key -out server.csr -config server.cnf
openssl x509 -req -in server.csr -CA rootCA.pem -CAkey rootCA.key -CAcreateserial \
  -out server.pem -days 825 -sha256 -extensions req_ext -extfile server.cnf
```

Поместите `server.pem` и `server.key` в `/opt/secure-messenger/certs/`. `client_ca.pem` остаётся рядом и используется сервером для проверки клиентских сертификатов.

### 6.3 Выпуск клиентских сертификатов
Для каждого устройства повторяйте шаблон, меняя `CN`, email и имя файла:

```bash
CLIENT_ID=alice-laptop
cat > client-${CLIENT_ID}.cnf <<'EOF'
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

openssl genrsa -out client-${CLIENT_ID}.key 4096
openssl req -new -key client-${CLIENT_ID}.key -out client-${CLIENT_ID}.csr -config client-${CLIENT_ID}.cnf
openssl x509 -req -in client-${CLIENT_ID}.csr -CA rootCA.pem -CAkey rootCA.key -CAcreateserial \
  -out client-${CLIENT_ID}.pem -days 365 -sha256 -extensions req_ext -extfile client-${CLIENT_ID}.cnf
```

### 6.4 Передача сертификатов пользователям
Пользовательскому устройству нужны:

- личный сертификат `client-<id>.pem` и приватный ключ `client-<id>.key`;
- корневой сертификат `rootCA.pem` для проверки сервера.

Упакуйте материалы в PKCS#12 контейнер или зашифрованный архив, а затем передайте по защищённым каналам:

```bash
openssl pkcs12 -export \
  -inkey client-${CLIENT_ID}.key \
  -in client-${CLIENT_ID}.pem \
  -certfile rootCA.pem \
  -out client-${CLIENT_ID}.p12
```

Пароль от контейнера передавайте отдельным каналом связи. Секретный ключ `rootCA.key` должен храниться только на сервере выпускного центра и не распространяться.

## 7. Регистрация пользователей

Сервер сопоставляет устройства с пользователями по клиентскому сертификату. Регистрация возможна:

### 7.1 Через HTTP API
1. На устройстве пользователя преобразуйте сертификат в DER и закодируйте в base64:
   ```bash
   openssl x509 -in client-alice.pem -outform DER | base64 -w0 > alice.der.b64
   ```
2. Отправьте запрос на `/api/auth/register` (используйте защищённый канал или локальный доступ):
   ```bash
   curl -X POST https://your-server:8080/api/auth/register \
     --cacert rootCA.pem \
     --cert client-alice.pem --key client-alice.key \
     -H 'Content-Type: application/json' \
     -d @<(cat <<'JSON'
   {
     "nickname": "alice",
     "certificate": "$(cat alice.der.b64)"
   }
   JSON
   )
   ```
   Ответ содержит `user_id`, который нужно сохранить на устройстве.

### 7.2 Через Qt-клиент
В форме регистрации укажите:
- путь к клиентскому сертификату (PEM или DER);
- приватный ключ, если он хранится отдельно;
- желаемый никнейм.

Клиент отправит сертификат на `/api/auth/register`, сохранит выданный `user_id` и будет использовать его при подключениях.

## 8. Настройка и запуск клиента

1. Соберите приложение (см. раздел 4).
2. Скопируйте выданные файлы на устройство пользователя и настройте права доступа.
3. Перед запуском задайте параметры окружения (пример для Linux/macOS):
   ```bash
   export SM_HTTP_API="https://your-server:8080"
   export SM_TLS_CERT="/path/to/client-alice.pem"
   export SM_TLS_KEY="/path/to/client-alice.key"
   export SM_TRUST_ANCHOR="/path/to/rootCA.pem"
   export SM_AUTH_USER_ID="user-0001"   # выданный идентификатор
   ./build/client-qt/sm_client
   ```

Интерфейс загрузит историю сообщений, покажет текущий диалог и будет регулярно опрашивать `/api/messages`. Отправленные сообщения уходят на сервер через `POST /api/messages`.

## 9. Эксплуатация

- Файлы `messages.db` и `identity.db` представляют собой бинарные базы (gob) и располагаются в директории, указанной переменными `SM_STORE` и `SM_IDENTITY_STORE`. Настройте резервное копирование.
- Для добавления нового устройства выпустите новый сертификат, повторите регистрацию и передайте его пользователю.
- Логи сервера перенаправляются в `logs/server.log`, если используется рекомендованный `systemd` юнит.

## 10. HTTP и gRPC интерфейсы

- `GET /api/messages?since_id=msg-5` — получить историю сообщений.
- `POST /api/messages` — отправить сообщение:
  ```json
  {
    "conversation_id": "corp-secure-room",
    "sender_user_id": "user-0001",
    "text": "Привет!"
  }
  ```
- `POST /api/auth/register` — зарегистрировать сертификат пользователя.

Для отладки доступен gRPC с методами `sm.v1.Messaging/Pull` и `sm.v1.Messaging/Send`. Подключайтесь через `grpcurl`, указывая `-cacert`, `-cert`, `-key` и адрес `localhost:8443` или удалённый хост.

## 11. Криптография, протоколы и хранение данных

- Транспорт: gRPC слушает по TLS 1.3 с обязательными клиентскими сертификатами (mTLS). Валидация проверяет наличие `CommonName` и парных SAN `sm://user` и `sm://device` в сертификате клиента, после чего сертификат сопоставляется с ранее зарегистрированным профилем. Для проверки клиентских цепочек используется `client_ca.pem`, а серверный ключ и сертификат берутся из `server.pem`/`server.key`. Валидацию выполняет `LoadServerTLSConfig`, подключённая в `cmd/server`. HTTP API может работать за тем же TLS-терминатором или реверс-прокси.
- Шифрование полезной нагрузки HTTP: тела сообщений шифруются симметричным AES-256-GCM. Ключ задаётся переменной `SM_MESSAGE_KEY` (или флагом `--message-key`) как base64-строка длиной 32 байта; nonce генерируется случайно и приставляется к ciphertext, а отпечаток ключа сохраняется вместе с данными хранилища для выявления несоответствий конфигурации.
- Токены сессии HTTP: после мTLS-аутентификации сервер выдаёт bearer-токены для повторного доступа. Они генерируются из 32 байтов криптографически стойких случайных данных, кодируются `base64url` и действуют по умолчанию 30 минут; хранение токенов осуществляется в памяти процесса.
- Хранилище сообщений (`SM_STORE`): представляет собой gob-снимок с метаданными сообщений и read-маркерами. В базе сохраняются только зашифрованные конверты (nonce+AES-GCM ciphertext); поля `conversation_id`, `sender_user_id` и `sent_unix_sec` хранятся отдельно для индексации. При несовпадении отпечатка ключа AES загрузка блокируется с подсказкой обновить `SM_MESSAGE_KEY`.
- Хранилище учётных записей (`SM_IDENTITY_STORE`): gob-файл со списком пользователей, их ролями и DER-копией клиентского сертификата. Пароли хранятся как соли и SHA-256-хэши (`<salt_b64>:<hash_b64>`); при загрузке устаревшие открытые пароли мигрируются в хэшированную форму.


