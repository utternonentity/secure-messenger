#include "AppController.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <utility>

namespace {
QString encodedCertificate(const QString &deviceId, const QString &label)
{
    const QByteArray raw = QStringLiteral("%1:%2").arg(deviceId, label).toUtf8();
    return raw.toBase64();
}
}

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(3000);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        fetchHistoryFromServer(m_lastServerMsgId);
    });

    m_apiBaseUrl = qEnvironmentVariable("SM_HTTP_API").trimmed();
    if (m_apiBaseUrl.isEmpty()) {
        m_apiBaseUrl = QStringLiteral("http://127.0.0.1:8080");
    }

    loadRegistration();
    if (m_isRegistered) {
        initializeAfterRegistration();
    }

    emit registrationChanged();
}

QVariantMap AppController::authInfo() const
{
    return buildAuthInfo();
}

QVariantList AppController::userList() const
{
    return buildUserList();
}

QVariantList AppController::conversation() const
{
    return buildConversation();
}

QStringList AppController::serverLog() const
{
    return m_serverLog;
}

QString AppController::currentConversation() const
{
    return m_currentConversation;
}

void AppController::setCurrentConversation(const QString &conversationId)
{
    const QString trimmed = conversationId.trimmed();
    if (trimmed.isEmpty() || trimmed == m_currentConversation) {
        return;
    }
    m_currentConversation = trimmed;
    if (!m_conversations.contains(m_currentConversation)) {
        m_conversations.insert(m_currentConversation, {});
    }
    appendLog(QStringLiteral("Messaging.Pull -> подписка обновлена, канал %1").arg(m_currentConversation));
    emit currentConversationChanged();
    emit conversationChanged();
}

bool AppController::isRegistered() const
{
    return m_isRegistered;
}

QString AppController::nickname() const
{
    return m_registeredNickname;
}

void AppController::send(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (!m_conversations.contains(m_currentConversation)) {
        m_conversations.insert(m_currentConversation, {});
    }

    appendLog(QStringLiteral("Messaging.Send -> отправка в канал %1")
                  .arg(m_currentConversation));
    postMessageToServer(m_currentConversation, trimmed);
}

void AppController::startConversationWith(const QString &userId)
{
    const QString trimmed = userId.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (trimmed == m_authenticatedUser.userId) {
        appendLog(QStringLiteral("Messaging.Direct -> попытка открыть чат с самим собой отклонена"));
        return;
    }

    const QString directChannel = QStringLiteral("dm-%1-%2").arg(m_authenticatedUser.userId, trimmed);
    const bool alreadyExists = m_conversations.contains(directChannel);

    setCurrentConversation(directChannel);
    appendLog(QStringLiteral("Messaging.Direct -> активирован канал %1").arg(directChannel));

    if (!alreadyExists) {
        const User *user = findUser(trimmed);
        const QString partnerName = user ? user->nickname : trimmed;
        addMessage(directChannel,
                   QStringLiteral("Сервер"),
                   QStringLiteral("Создан защищённый канал с %1").arg(partnerName),
                   false);
        if (user) {
            addMessage(directChannel,
                       partnerName,
                       QStringLiteral("Привет! Готов(а) к общению."),
                       false);
        }
    }
}

void AppController::rotateDevice(const QString &userId, const QString &deviceId)
{
    Device *device = findDevice(userId, deviceId);
    if (!device) {
        return;
    }

    const QString label = QStringLiteral("rotated-%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("hhmmss")));
    device->certificate = encodedCertificate(deviceId, label);
    device->revoked = false;
    appendLog(QStringLiteral("Directory.RotateDevice -> %1/%2 обновлён сертификат")
                  .arg(userId, deviceId));
    emit userListChanged();
}

void AppController::revokeDevice(const QString &userId, const QString &deviceId)
{
    Device *device = findDevice(userId, deviceId);
    if (!device) {
        return;
    }

    if (device->revoked) {
        appendLog(QStringLiteral("Directory.RevokeDevice -> %1/%2 уже отозван")
                      .arg(userId, deviceId));
        return;
    }

    device->revoked = true;
    appendLog(QStringLiteral("Directory.RevokeDevice -> %1/%2 помечен revoked")
                  .arg(userId, deviceId));
    emit userListChanged();
}

void AppController::refreshUsers()
{
    const QString identityPath = QDir(resolveDataDirectory()).filePath(QStringLiteral("identity_store.json"));
    const bool reloaded = loadUserDirectory(identityPath);
    ensureDirectoryContainsAuthUser();
    if (reloaded) {
        appendLog(QStringLiteral("Directory.ListUsers -> обновлено, %1 профиля")
                      .arg(m_directory.size()));
    } else {
        appendLog(QStringLiteral("Directory.ListUsers -> обновление не удалось, используется кэш (%1 профиля)")
                      .arg(m_directory.size()));
    }
    emit authInfoChanged();
    emit userListChanged();
}

void AppController::simulatePull()
{
    appendLog(QStringLiteral("Messaging.Pull -> ручной запрос обновлений"));
    fetchHistoryFromServer(m_lastServerMsgId);
}

void AppController::completeRegistration(const QString &nickname)
{
    const QString trimmed = nickname.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    const QString previousNickname = m_registeredNickname;
    const bool wasRegistered = m_isRegistered;

    persistRegistration(trimmed);
    m_registeredNickname = trimmed;
    m_isRegistered = true;

    initializeAfterRegistration();

    if (!wasRegistered || previousNickname != trimmed) {
        appendLog(QStringLiteral("Registration -> активирован локальный профиль %1").arg(trimmed));
    }

    emit registrationChanged();
}

QVariantMap AppController::buildAuthInfo() const
{
    QVariantMap map;
    map.insert(QStringLiteral("userId"), m_authenticatedUser.userId);
    map.insert(QStringLiteral("nickname"), m_authenticatedUser.nickname);
    if (!m_authenticatedUser.devices.isEmpty()) {
        const Device &device = m_authenticatedUser.devices.first();
        map.insert(QStringLiteral("deviceId"), device.deviceId);
        map.insert(QStringLiteral("certificate"), device.certificate);
    }
    map.insert(QStringLiteral("roles"), m_authenticatedRoles);
    return map;
}

QVariantList AppController::buildUserList() const
{
    QVariantList list;
    for (const User &user : m_directory) {
        QVariantMap entry;
        entry.insert(QStringLiteral("userId"), user.userId);
        entry.insert(QStringLiteral("nickname"), user.nickname);
        QVariantList devices;
        for (const Device &device : user.devices) {
            QVariantMap deviceMap;
            deviceMap.insert(QStringLiteral("deviceId"), device.deviceId);
            deviceMap.insert(QStringLiteral("certificate"), device.certificate);
            deviceMap.insert(QStringLiteral("revoked"), device.revoked);
            devices.append(deviceMap);
        }
        entry.insert(QStringLiteral("devices"), devices);
        list.append(entry);
    }
    return list;
}

QVariantList AppController::buildConversation() const
{
    QVariantList list;
    const auto it = m_conversations.constFind(m_currentConversation);
    if (it == m_conversations.constEnd()) {
        return list;
    }

    const QList<Message> &messages = it.value();
    for (const Message &message : messages) {
        QVariantMap entry;
        entry.insert(QStringLiteral("serverMsgId"), message.serverMsgId);
        entry.insert(QStringLiteral("author"), message.author);
        entry.insert(QStringLiteral("text"), message.text);
        entry.insert(QStringLiteral("timestamp"), message.timestamp);
        entry.insert(QStringLiteral("outgoing"), message.outgoing);
        list.append(entry);
    }
    return list;
}

void AppController::initializeAfterRegistration()
{
    if (m_initialized) {
        applyRegisteredNickname();
        ensureDirectoryContainsAuthUser();
        emit authInfoChanged();
        emit userListChanged();
        return;
    }

    m_initialized = true;

    loadServerData();
    applyRegisteredNickname();
    ensureDirectoryContainsAuthUser();

    int totalMessages = 0;
    for (const QList<Message> &messages : std::as_const(m_conversations)) {
        totalMessages += messages.size();
    }

    appendLog(QStringLiteral("Auth.WhoAmI -> %1 (%2)")
                  .arg(m_authenticatedUser.userId, m_authenticatedUser.nickname));
    appendLog(QStringLiteral("Directory.ListUsers -> %1 профиля")
                  .arg(m_directory.size()));
    appendLog(QStringLiteral("Messaging.LoadHistory -> локальный кэш %1 сообщений в %2 каналах")
                  .arg(totalMessages)
                  .arg(m_conversations.size()));
    appendLog(QStringLiteral("Messaging.Pull -> подписка на %1")
                  .arg(m_currentConversation));
    appendLog(QStringLiteral("Messaging.HTTP -> базовый URL %1")
                  .arg(m_apiBaseUrl));

    emit authInfoChanged();
    emit userListChanged();
    emit conversationChanged();
    emit currentConversationChanged();

    fetchHistoryFromServer();
    m_pollTimer->start();
}

void AppController::applyRegisteredNickname()
{
    const QString trimmed = m_registeredNickname.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    m_authenticatedUser.nickname = trimmed;
    for (User &user : m_directory) {
        if (user.userId == m_authenticatedUser.userId) {
            user.nickname = trimmed;
            break;
        }
    }
}

void AppController::loadRegistration()
{
    QSettings settings;
    const QString storedNickname = settings.value(QStringLiteral("registration/nickname")).toString().trimmed();
    if (storedNickname.isEmpty()) {
        m_isRegistered = false;
        m_registeredNickname.clear();
    } else {
        m_isRegistered = true;
        m_registeredNickname = storedNickname;
    }
}

void AppController::persistRegistration(const QString &nickname)
{
    QSettings settings;
    settings.setValue(QStringLiteral("registration/nickname"), nickname);
    settings.sync();
}

void AppController::loadServerData()
{
    m_directory.clear();
    m_conversations.clear();
    m_authenticatedRoles.clear();
    m_authenticatedUser = User{};
    m_lastServerMsgId.clear();
    m_knownServerMsgIds.clear();
    m_nextMessageId = 1;

    const QString dataDir = resolveDataDirectory();
    const QString identityPath = QDir(dataDir).filePath(QStringLiteral("identity_store.json"));
    const QString messagesPath = QDir(dataDir).filePath(QStringLiteral("messages.db"));

    const bool usersLoaded = loadUserDirectory(identityPath);
    ensureDirectoryContainsAuthUser();
    if (m_authenticatedRoles.isEmpty()) {
        m_authenticatedRoles << QStringLiteral("user");
    }

    const bool historyLoaded = loadMessageHistory(messagesPath);
    if (m_conversations.isEmpty()) {
        m_conversations.insert(QStringLiteral("corp-secure-room"), {});
    }

    if (m_currentConversation.trimmed().isEmpty()) {
        if (m_conversations.contains(QStringLiteral("corp-secure-room"))) {
            m_currentConversation = QStringLiteral("corp-secure-room");
        } else if (!m_conversations.isEmpty()) {
            m_currentConversation = m_conversations.constBegin().key();
        } else {
            m_currentConversation = QStringLiteral("corp-secure-room");
        }
    }

    if (!usersLoaded) {
        appendLog(QStringLiteral("Directory.Load -> не удалось прочитать %1, использованы встроенные данные")
                      .arg(identityPath));
    }
    if (!historyLoaded) {
        appendLog(QStringLiteral("Messaging.LoadHistory -> не удалось прочитать %1, история пуста")
                      .arg(messagesPath));
    }
}

bool AppController::loadUserDirectory(const QString &path)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        m_directory.clear();
        m_authenticatedRoles = QStringList{QStringLiteral("user")};

        m_authenticatedUser = User{};
        m_authenticatedUser.userId = QStringLiteral("user-0001");
        m_authenticatedUser.nickname = QStringLiteral("ironwarden");
        m_authenticatedUser.devices.append({QStringLiteral("device-ivan-laptop"),
                                            encodedCertificate(QStringLiteral("device-ivan-laptop"), QStringLiteral("primary")),
                                            false});

        User maria;
        maria.userId = QStringLiteral("user-0002");
        maria.nickname = QStringLiteral("nova");
        maria.devices.append({QStringLiteral("device-maria-laptop"),
                              encodedCertificate(QStringLiteral("device-maria-laptop"), QStringLiteral("laptop")),
                              false});
        maria.devices.append({QStringLiteral("device-maria-mobile"),
                              encodedCertificate(QStringLiteral("device-maria-mobile"), QStringLiteral("mobile")),
                              false});

        User oleg;
        oleg.userId = QStringLiteral("user-0003");
        oleg.nickname = QStringLiteral("bytefox");
        oleg.devices.append({QStringLiteral("device-oleg-desktop"),
                             encodedCertificate(QStringLiteral("device-oleg-desktop"), QStringLiteral("desktop")),
                             false});

        m_directory.append(m_authenticatedUser);
        m_directory.append(maria);
        m_directory.append(oleg);
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonArray users = doc.object().value(QStringLiteral("users")).toArray();
    if (users.isEmpty()) {
        return false;
    }

    m_directory.clear();
    QString preferredUserId = qEnvironmentVariable("SM_AUTH_USER_ID").trimmed();
    if (preferredUserId.isEmpty()) {
        preferredUserId = users.first().toObject().value(QStringLiteral("user_id")).toString();
    }

    for (const QJsonValue &userValue : users) {
        if (!userValue.isObject()) {
            continue;
        }
        const QJsonObject obj = userValue.toObject();
        User user;
        user.userId = obj.value(QStringLiteral("user_id")).toString();
        user.nickname = obj.value(QStringLiteral("nickname")).toString(user.userId);

        const QJsonObject devicesObj = obj.value(QStringLiteral("devices")).toObject();
        for (auto it = devicesObj.constBegin(); it != devicesObj.constEnd(); ++it) {
            const QJsonObject deviceObj = it.value().toObject();
            Device device;
            device.deviceId = deviceObj.value(QStringLiteral("device_id")).toString(it.key());
            device.certificate = deviceObj.value(QStringLiteral("cert_der")).toString();
            device.revoked = deviceObj.value(QStringLiteral("revoked")).toBool(false);
            user.devices.append(device);
        }
        m_directory.append(user);

        if (user.userId == preferredUserId) {
            m_authenticatedUser = user;
            m_authenticatedRoles.clear();
            const QJsonArray roles = obj.value(QStringLiteral("roles")).toArray();
            for (const QJsonValue &roleValue : roles) {
                const QString role = roleValue.toString().trimmed();
                if (!role.isEmpty()) {
                    m_authenticatedRoles.append(role);
                }
            }
        }
    }

    if (m_authenticatedUser.userId.isEmpty()) {
        m_authenticatedUser = m_directory.first();
        const QJsonArray roles = users.first().toObject().value(QStringLiteral("roles")).toArray();
        for (const QJsonValue &roleValue : roles) {
            const QString role = roleValue.toString().trimmed();
            if (!role.isEmpty()) {
                m_authenticatedRoles.append(role);
            }
        }
    }

    return true;
}

bool AppController::loadMessageHistory(const QString &path)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        m_nextMessageId = 1;
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonArray messages = doc.object().value(QStringLiteral("messages")).toArray();

    m_conversations.clear();
    m_nextMessageId = 1;
    for (const QJsonValue &value : messages) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        const qint64 id = static_cast<qint64>(obj.value(QStringLiteral("id")).toDouble());
        const QString conversationId = obj.value(QStringLiteral("conversation_id")).toString().trimmed();
        if (conversationId.isEmpty()) {
            continue;
        }
        const QString senderId = obj.value(QStringLiteral("sender_user_id")).toString();
        const QString ciphertext = obj.value(QStringLiteral("ciphertext_b64")).toString();
        const QByteArray decoded = QByteArray::fromBase64(ciphertext.toUtf8());
        const QString text = QString::fromUtf8(decoded.isEmpty() ? ciphertext.toUtf8() : decoded);
        const qint64 sentUnix = static_cast<qint64>(obj.value(QStringLiteral("sent_unix_sec")).toDouble());

        Message message;
        const QString serverMsgId = QStringLiteral("msg-%1").arg(id);
        message.serverMsgId = serverMsgId;
        message.author = nicknameForUserId(senderId);
        message.text = text;
        message.outgoing = senderId == m_authenticatedUser.userId;
        if (sentUnix > 0) {
            message.timestamp = QDateTime::fromSecsSinceEpoch(sentUnix).toString(QStringLiteral("HH:mm:ss"));
        } else {
            message.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
        }

        QList<Message> &conversation = m_conversations[conversationId];
        conversation.append(message);
        m_knownServerMsgIds.insert(serverMsgId);
        updateLastServerMsgId(serverMsgId);
    }

    return true;
}

QString AppController::resolveDataDirectory() const
{
    const QString envPath = qEnvironmentVariable("SM_DATA_DIR").trimmed();
    if (!envPath.isEmpty()) {
        return QDir(envPath).absolutePath();
    }

    const QStringList candidates = {QStringLiteral("../data"), QStringLiteral("../../data"), QStringLiteral("data")};
    QDir base(QCoreApplication::applicationDirPath());
    for (const QString &candidate : candidates) {
        QDir probe(base);
        if (probe.cd(candidate)) {
            return probe.absolutePath();
        }
    }

    QDir current(QDir::currentPath());
    if (current.cd(QStringLiteral("data"))) {
        return current.absolutePath();
    }
    return QDir::currentPath();
}

QString AppController::nicknameForUserId(const QString &userId) const
{
    if (userId == m_authenticatedUser.userId) {
        return m_authenticatedUser.nickname;
    }
    for (const User &user : m_directory) {
        if (user.userId == userId) {
            return user.nickname;
        }
    }
    return userId;
}

void AppController::fetchHistoryFromServer(const QString &sinceServerMsgId)
{
    if (!m_networkManager) {
        appendLog(QStringLiteral("Messaging.HTTP -> сетевой менеджер не инициализирован"));
        return;
    }

    QUrlQuery query;
    const QString marker = sinceServerMsgId.trimmed();
    if (!marker.isEmpty()) {
        query.addQueryItem(QStringLiteral("since_id"), marker);
    }

    const QUrl url = buildApiUrl(QStringLiteral("/api/messages"), query);
    if (!url.isValid()) {
        appendLog(QStringLiteral("Messaging.HTTP -> некорректный адрес API (%1)").arg(m_apiBaseUrl));
        return;
    }

    QNetworkRequest request(url);
    auto *reply = m_networkManager->get(request);
    const bool initialLoad = marker.isEmpty();
    connect(reply, &QNetworkReply::finished, this, [this, reply, initialLoad]() {
        const QNetworkReply::NetworkError error = reply->error();
        const QString errorText = reply->errorString();
        const QByteArray payload = reply->readAll();
        reply->deleteLater();

        if (error != QNetworkReply::NoError) {
            appendLog(QStringLiteral("Messaging.HTTP -> ошибка получения истории: %1")
                          .arg(errorText));
            return;
        }

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            appendLog(QStringLiteral("Messaging.HTTP -> некорректный JSON: %1")
                          .arg(parseError.errorString()));
            return;
        }

        const int added = handleMessagesResponse(doc);
        if (initialLoad) {
            appendLog(QStringLiteral("Messaging.Sync -> сервер вернул %1 сообщений")
                          .arg(added));
        } else if (added > 0) {
            appendLog(QStringLiteral("Messaging.Pull -> получено %1 новых сообщений")
                          .arg(added));
        }
    });
}

int AppController::handleMessagesResponse(const QJsonDocument &doc)
{
    if (!doc.isObject()) {
        return 0;
    }

    const QJsonObject root = doc.object();
    const QJsonArray messages = root.value(QStringLiteral("messages")).toArray();
    int added = 0;
    for (const QJsonValue &value : messages) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        const QString serverMsgId = obj.value(QStringLiteral("server_msg_id")).toString().trimmed();
        if (serverMsgId.isEmpty() || m_knownServerMsgIds.contains(serverMsgId)) {
            continue;
        }
        const QString conversationId = obj.value(QStringLiteral("conversation_id")).toString().trimmed();
        if (conversationId.isEmpty()) {
            continue;
        }
        const QString senderUserId = obj.value(QStringLiteral("sender_user_id")).toString();
        const QString text = obj.value(QStringLiteral("text")).toString();
        const qint64 sentUnixSec = static_cast<qint64>(obj.value(QStringLiteral("sent_unix_sec")).toDouble());

        const QString author = nicknameForUserId(senderUserId);
        const bool outgoing = senderUserId == m_authenticatedUser.userId;
        addServerMessage(conversationId, serverMsgId, author, text, outgoing, sentUnixSec);
        ++added;
    }

    const QString lastId = root.value(QStringLiteral("last_server_msg_id")).toString().trimmed();
    if (!lastId.isEmpty()) {
        updateLastServerMsgId(lastId);
    }

    return added;
}

void AppController::postMessageToServer(const QString &conversationId, const QString &text)
{
    if (!m_networkManager) {
        appendLog(QStringLiteral("Messaging.Send -> сетевой менеджер не инициализирован"));
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("conversation_id"), conversationId);
    payload.insert(QStringLiteral("sender_user_id"), m_authenticatedUser.userId);
    if (!m_authenticatedUser.devices.isEmpty()) {
        payload.insert(QStringLiteral("sender_device_id"), m_authenticatedUser.devices.first().deviceId);
    }
    payload.insert(QStringLiteral("text"), text);

    const QUrl url = buildApiUrl(QStringLiteral("/api/messages"));
    if (!url.isValid()) {
        appendLog(QStringLiteral("Messaging.Send -> некорректный адрес API (%1)").arg(m_apiBaseUrl));
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    auto *reply = m_networkManager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, conversationId, text]() {
        const QNetworkReply::NetworkError error = reply->error();
        const QString errorText = reply->errorString();
        const QByteArray payload = reply->readAll();
        reply->deleteLater();

        if (error != QNetworkReply::NoError) {
            appendLog(QStringLiteral("Messaging.Send -> ошибка публикации: %1")
                          .arg(errorText));
            return;
        }

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            appendLog(QStringLiteral("Messaging.Send -> некорректный ответ сервера"));
            return;
        }

        const QJsonObject obj = doc.object();
        const QString serverMsgId = obj.value(QStringLiteral("server_msg_id")).toString().trimmed();
        QString convId = obj.value(QStringLiteral("conversation_id")).toString().trimmed();
        if (convId.isEmpty()) {
            convId = conversationId;
        }
        const QString senderUserId = obj.value(QStringLiteral("sender_user_id")).toString(m_authenticatedUser.userId);
        const QString deliveredText = obj.value(QStringLiteral("text")).toString(text);
        const qint64 sentUnixSec = static_cast<qint64>(obj.value(QStringLiteral("sent_unix_sec")).toDouble());

        if (!serverMsgId.isEmpty() && !m_knownServerMsgIds.contains(serverMsgId)) {
        const QString author = nicknameForUserId(senderUserId);
            addServerMessage(convId,
                             serverMsgId,
                             author,
                             deliveredText,
                             senderUserId == m_authenticatedUser.userId,
                             sentUnixSec);
            appendLog(QStringLiteral("Messaging.Send -> доставлено %1 (conv=%2)")
                          .arg(serverMsgId, convId));
        }
    });
}

void AppController::updateLastServerMsgId(const QString &serverMsgId)
{
    const qint64 numeric = parseServerMsgNumeric(serverMsgId);
    const qint64 current = parseServerMsgNumeric(m_lastServerMsgId);
    if (numeric > current) {
        m_lastServerMsgId = serverMsgId;
    }
}

qint64 AppController::parseServerMsgNumeric(const QString &serverMsgId) const
{
    if (!serverMsgId.startsWith(QStringLiteral("msg-"))) {
        return 0;
    }
    bool ok = false;
    const qint64 value = serverMsgId.mid(4).toLongLong(&ok);
    if (!ok) {
        return 0;
    }
    return value;
}

QUrl AppController::buildApiUrl(const QString &path, const QUrlQuery &query) const
{
    QUrl base(m_apiBaseUrl);
    if (!base.isValid()) {
        return {};
    }
    QUrl endpoint = base.resolved(QUrl(path));
    if (!query.isEmpty()) {
        endpoint.setQuery(query);
    }
    return endpoint;
}

void AppController::addServerMessage(const QString &conversationId,
                                      const QString &serverMsgId,
                                      const QString &author,
                                      const QString &text,
                                      bool outgoing,
                                      qint64 sentUnixSec)
{
    if (conversationId.trimmed().isEmpty() || serverMsgId.trimmed().isEmpty()) {
        return;
    }

    Message message;
    message.serverMsgId = serverMsgId;
    message.author = author;
    message.text = text;
    message.outgoing = outgoing;
    if (sentUnixSec > 0) {
        message.timestamp = QDateTime::fromSecsSinceEpoch(sentUnixSec).toString(QStringLiteral("HH:mm:ss"));
    } else {
        message.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    }

    QList<Message> &messages = m_conversations[conversationId];
    messages.append(message);
    m_knownServerMsgIds.insert(serverMsgId);
    updateLastServerMsgId(serverMsgId);

    if (conversationId == m_currentConversation) {
        emit conversationChanged();
    }
}

QString AppController::addMessage(const QString &conversationId, const QString &author, const QString &text, bool outgoing)
{
    Message message;
    message.serverMsgId = QStringLiteral("local-%1").arg(m_nextMessageId++);
    message.author = author;
    message.text = text;
    message.outgoing = outgoing;
    message.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    QList<Message> &messages = m_conversations[conversationId];
    messages.append(message);
    if (conversationId == m_currentConversation) {
        emit conversationChanged();
    }
    return message.serverMsgId;
}

void AppController::appendLog(const QString &entry)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_serverLog.append(QStringLiteral("[%1] %2").arg(ts, entry));
    emit serverLogChanged();
}

void AppController::ensureDirectoryContainsAuthUser()
{
    if (findUser(m_authenticatedUser.userId) == nullptr) {
        m_directory.prepend(m_authenticatedUser);
    }
}

AppController::User *AppController::findUser(const QString &userId)
{
    for (User &user : m_directory) {
        if (user.userId == userId) {
            return &user;
        }
    }
    return nullptr;
}

AppController::Device *AppController::findDevice(const QString &userId, const QString &deviceId)
{
    User *user = findUser(userId);
    if (!user) {
        return nullptr;
    }
    for (Device &device : user->devices) {
        if (device.deviceId == deviceId) {
            return &device;
        }
    }
    return nullptr;
}
