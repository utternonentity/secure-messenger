#include "AppController.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QEventLoop>
#include <QSsl>
#include <QSslCertificate>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QScopedValueRollback>
#include <QScopeGuard>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <utility>

namespace {
QString encodedCertificate(const QString &deviceId, const QString &label)
{
    const QByteArray raw = QStringLiteral("%1:%2").arg(deviceId, label).toUtf8();
    return raw.toBase64();
}

QString canonicalDirectConversationId(const QString &first, const QString &second)
{
    const QString left = first.trimmed();
    const QString right = second.trimmed();
    if (left.isEmpty() || right.isEmpty() || left == right) {
        return {};
    }

    QStringList ordered{left, right};
    std::sort(ordered.begin(), ordered.end(), [](const QString &lhs, const QString &rhs) {
        const int insensitive = QString::compare(lhs, rhs, Qt::CaseInsensitive);
        if (insensitive == 0) {
            return lhs < rhs;
        }
        return insensitive < 0;
    });

    return QStringLiteral("dm-%1-%2").arg(ordered.at(0), ordered.at(1));
}

QString extractMessageText(const QJsonObject &obj)
{
    QString text = obj.value(QStringLiteral("text")).toString().trimmed();
    if (!text.isEmpty()) {
        return text;
    }

    const QString ciphertext = obj.value(QStringLiteral("ciphertext_b64")).toString();
    if (!ciphertext.isEmpty()) {
        const QByteArray decoded = QByteArray::fromBase64(ciphertext.toUtf8());
        const QString decodedText = QString::fromUtf8(decoded);
        if (!decodedText.trimmed().isEmpty()) {
            return decodedText.trimmed();
        }
    }

    return {};
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

    loadCredentials();
    loadRegistration();
    if (m_isRegistered) {
        ensureSessionToken(true);
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

QVariantList AppController::conversationList() const
{
    return buildConversationList();
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
    if (trimmed.isEmpty()) {
        const bool changed = !m_currentConversation.isEmpty();
        m_currentConversation.clear();
        if (changed) {
            emit currentConversationChanged();
            emit conversationChanged();
        }
        return;
    }
    if (!isConversationVisible(trimmed)) {
        appendLog(QStringLiteral("Messaging.Visibility -> канал %1 недоступен текущему профилю")
                      .arg(trimmed));
        return;
    }
    if (!m_conversations.contains(trimmed)) {
        m_conversations.insert(trimmed, {});
    }
    promoteConversation(trimmed, lastActivityForConversation(trimmed));

    const bool changed = trimmed != m_currentConversation;
    m_currentConversation = trimmed;
    markConversationRead(trimmed);
    emit conversationListChanged();

    if (changed) {
        appendLog(QStringLiteral("Messaging.Pull -> подписка обновлена, канал %1").arg(m_currentConversation));
        emit currentConversationChanged();
        emit conversationChanged();
    }
}

bool AppController::isRegistered() const
{
    return m_isRegistered;
}

QString AppController::nickname() const
{
    return m_registeredNickname;
}

bool AppController::isAuthBusy() const
{
    return m_authBusy;
}

void AppController::send(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (m_currentConversation.trimmed().isEmpty()) {
        appendLog(QStringLiteral("Messaging.Send -> канал не выбран"));
        return;
    }

    if (!m_conversations.contains(m_currentConversation)) {
        m_conversations.insert(m_currentConversation, {});
    }
    promoteConversation(m_currentConversation, QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    emit conversationListChanged();

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

    QString targetId = trimmed;
    for (const User &user : m_directory) {
        if (QString::compare(user.nickname, trimmed, Qt::CaseInsensitive) == 0) {
            targetId = user.userId.trimmed();
            break;
        }
    }

    const QString myId = m_authenticatedUser.userId.trimmed();
    if (myId.isEmpty()) {
        appendLog(QStringLiteral("Messaging.Direct -> профиль не активирован"));
        return;
    }

    if (targetId == myId) {
        appendLog(QStringLiteral("Messaging.Direct -> попытка открыть чат с самим собой отклонена"));
        return;
    }

    const QString directChannel = canonicalDirectConversationId(targetId, myId);
    if (directChannel.isEmpty()) {
        appendLog(QStringLiteral("Messaging.Direct -> не удалось вычислить идентификатор канала"));
        return;
    }

    setCurrentConversation(directChannel);
    appendLog(QStringLiteral("Messaging.Direct -> активирован канал %1").arg(directChannel));
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
    const QString identityPath = QDir(resolveDataDirectory()).filePath(QStringLiteral("identity.db"));
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

    if (m_isRegistered) {
        fetchUsersFromServer();
    }
}

void AppController::simulatePull()
{
    appendLog(QStringLiteral("Messaging.Pull -> ручной запрос обновлений"));
    fetchHistoryFromServer(m_lastServerMsgId);
}

QString AppController::authenticate(const QString &nickname,
                                    const QString &password,
                                    const QString &certificatePath)
{
    if (m_authBusy) {
        return tr("Дождитесь завершения предыдущей операции");
    }

    const QString trimmedNickname = nickname.trimmed();
    if (trimmedNickname.isEmpty()) {
        return tr("Введите никнейм");
    }
    const QString trimmedPassword = password.trimmed();
    if (trimmedPassword.isEmpty()) {
        return tr("Введите пароль");
    }

    const QString certificateFile = certificatePath.trimmed();
    if (certificateFile.isEmpty()) {
        return tr("Укажите сертификат устройства");
    }

    QByteArray certDer;
    QString certError;
    if (!loadCertificateFromFile(certificateFile, certDer, certError)) {
        return certError;
    }
    const QString certBase64 = QString::fromLatin1(certDer.toBase64()).trimmed();

    QJsonObject payload;
    payload.insert(QStringLiteral("nickname"), trimmedNickname);
    payload.insert(QStringLiteral("password"), trimmedPassword);
    payload.insert(QStringLiteral("certificate"), certBase64);

    QJsonObject response;
    const QString requestError = sendAuthRequest(QStringLiteral("/api/auth/login"),
                                                 tr("Вход"),
                                                 payload,
                                                 response,
                                                 true);
    if (!requestError.isEmpty()) {
        return requestError;
    }

    AuthSession session;
    QString parseError;
    if (!parseAuthSession(response, session, parseError)) {
        return parseError;
    }
    if (session.certificateBase64.isEmpty()) {
        session.certificateBase64 = certBase64;
    }

    const QString storeError = storeCredential(session.userId,
                                               session.nickname,
                                               trimmedPassword,
                                               session.certificateBase64,
                                               true);
    if (!storeError.isEmpty()) {
        return storeError;
    }

    applySessionState(session);

    appendLog(QStringLiteral("Auth.Login -> пользователь %1 вошёл в систему")
                  .arg(session.nickname));

    emit authInfoChanged();
    emit registrationChanged();

    initializeAfterRegistration();

    return {};
}

QString AppController::completeRegistration(const QString &nickname,
                                            const QString &password,
                                            const QString &certificatePath)
{
    const QString trimmed = nickname.trimmed();
    if (trimmed.isEmpty()) {
        return tr("Введите никнейм");
    }

    const QString trimmedPassword = password.trimmed();
    if (trimmedPassword.isEmpty()) {
        return tr("Введите пароль");
    }

    if (m_registrationInFlight) {
        return tr("Дождитесь завершения предыдущей регистрации");
    }

    const QString certificateFile = certificatePath.trimmed();
    if (certificateFile.isEmpty()) {
        return tr("Укажите путь к клиентскому сертификату");
    }

    QByteArray certDer;
    QString certError;
    if (!loadCertificateFromFile(certificateFile, certDer, certError)) {
        return certError;
    }
    const QString certBase64 = QString::fromLatin1(certDer.toBase64()).trimmed();

    QJsonObject payload;
    payload.insert(QStringLiteral("nickname"), trimmed);
    payload.insert(QStringLiteral("password"), trimmedPassword);
    payload.insert(QStringLiteral("certificate"), certBase64);

    QScopedValueRollback<bool> inFlight(m_registrationInFlight, true);

    QJsonObject response;
    const QString requestError = sendAuthRequest(QStringLiteral("/api/auth/register"),
                                                 tr("Регистрация"),
                                                 payload,
                                                 response,
                                                 true);
    if (!requestError.isEmpty()) {
        return requestError;
    }

    AuthSession session;
    QString parseError;
    if (!parseAuthSession(response, session, parseError)) {
        return parseError;
    }
    if (session.certificateBase64.isEmpty()) {
        session.certificateBase64 = certBase64;
    }

    const QString storeError = storeCredential(session.userId,
                                               session.nickname,
                                               trimmedPassword,
                                               session.certificateBase64,
                                               true);
    if (!storeError.isEmpty()) {
        return storeError;
    }

    applySessionState(session);

    appendLog(QStringLiteral("Registration -> зарегистрирован профиль %1 (%2)")
                  .arg(session.nickname, session.userId));

    emit authInfoChanged();
    emit registrationChanged();

    initializeAfterRegistration();

    return {};
}

void AppController::resetRegistration()
{
    if (m_registrationInFlight) {
        appendLog(QStringLiteral("Registration -> дождитесь завершения регистрации"));
        return;
    }

    QSettings settings;
    settings.remove(QStringLiteral("registration"));
    settings.sync();

    if (m_pollTimer) {
        m_pollTimer->stop();
    }

    m_registrationInFlight = false;
    m_isRegistered = false;
    m_registeredNickname.clear();
    m_registeredUserId.clear();
    m_initialized = false;
    m_authenticatedUser = User{};
    m_authenticatedRoles.clear();
    m_directory.clear();
    m_conversations.clear();
    m_conversationOrder.clear();
    m_currentConversation.clear();
    m_serverLog.clear();
    m_knownServerMsgIds.clear();
    m_conversationActivity.clear();
    m_conversationReadMarkers.clear();
    m_lastServerMsgId.clear();
    m_nextMessageId = 1;
    m_accessToken.clear();
    m_tokenExpiry = QDateTime();
    settings.remove(QStringLiteral("messaging/readMarkers"));
    setAuthBusy(false);

    appendLog(QStringLiteral("Registration -> профиль сброшен, повторите регистрацию"));

    emit authInfoChanged();
    emit userListChanged();
    emit conversationChanged();
    emit conversationListChanged();
    emit currentConversationChanged();
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
    if (!m_accessToken.trimmed().isEmpty()) {
        map.insert(QStringLiteral("accessToken"), m_accessToken);
        if (m_tokenExpiry.isValid()) {
            map.insert(QStringLiteral("tokenExpiry"), m_tokenExpiry.toString(Qt::ISODate));
        }
    }
    return map;
}

QVariantList AppController::buildUserList() const
{
    QVariantList list;
    for (const User &user : m_directory) {
        const QString nickname = user.nickname.trimmed();
        if (nickname.isEmpty() || nickname == tr("Неизвестный")) {
            continue;
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("userId"), user.userId);
        entry.insert(QStringLiteral("nickname"), nickname);
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
    if (!isConversationVisible(m_currentConversation)) {
        return list;
    }
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
        entry.insert(QStringLiteral("delivered"), message.delivered);
        entry.insert(QStringLiteral("read"), message.readByPeer);
        list.append(entry);
    }
    return list;
}

QVariantList AppController::buildConversationList() const
{
    QVariantList list;
    for (const QString &conversationId : m_conversationOrder) {
        if (!isConversationVisible(conversationId)) {
            continue;
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), conversationId);
        entry.insert(QStringLiteral("title"), conversationDisplayName(conversationId));
        const QString subtitle = conversationSubtitle(conversationId);
        if (!subtitle.isEmpty()) {
            entry.insert(QStringLiteral("subtitle"), subtitle);
        }

        const QList<Message> &messages = m_conversations.value(conversationId);
        qint64 lastActivity = m_conversationActivity.value(conversationId, 0);
        if (!messages.isEmpty()) {
            const Message &last = messages.constLast();
            entry.insert(QStringLiteral("lastMessage"), last.text);
            const qint64 messageActivity = last.sentUnixSec > 0 ? last.sentUnixSec
                                                                : parseServerMsgNumeric(last.serverMsgId);
            lastActivity = std::max(lastActivity, messageActivity);
        } else {
            entry.insert(QStringLiteral("lastMessage"), tr("Нет сообщений"));
        }

        if (lastActivity > 0) {
            const QDateTime lastMoment = QDateTime::fromSecsSinceEpoch(lastActivity).toLocalTime();
            const qint64 ageSeconds = qAbs(lastMoment.secsTo(QDateTime::currentDateTime()));
            const bool recent = ageSeconds < 24 * 3600;
            const QString displayTime = recent ? lastMoment.toString(QStringLiteral("HH:mm"))
                                               : QLocale().toString(lastMoment.date(), QLocale::ShortFormat);
            entry.insert(QStringLiteral("lastTimestamp"), displayTime);
        } else {
            entry.insert(QStringLiteral("lastTimestamp"), QString());
        }

        const int unread = unreadCountFor(conversationId);
        if (unread > 0) {
            entry.insert(QStringLiteral("unreadCount"), unread);
        }
        list.append(entry);
    }
    return list;
}

void AppController::initializeAfterRegistration()
{
    if (m_initialized) {
        applyRegisteredIdentity();
        ensureDirectoryContainsAuthUser();
        emit authInfoChanged();
        emit userListChanged();
        emit conversationListChanged();
        if (m_isRegistered) {
            fetchUsersFromServer();
        }
        return;
    }

    m_initialized = true;

    loadServerData();
    applyRegisteredIdentity();
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
    emit conversationListChanged();
    emit currentConversationChanged();

    fetchHistoryFromServer();
    if (m_isRegistered) {
        fetchUsersFromServer();
    }
    m_pollTimer->start();
}

void AppController::applyRegisteredIdentity()
{
    const QString trimmedUserId = m_registeredUserId.trimmed();
    const QString trimmedNickname = m_registeredNickname.trimmed();

    if (trimmedUserId.isEmpty()) {
        return;
    }

    if (User *existing = findUser(trimmedUserId)) {
        if (!trimmedNickname.isEmpty()) {
            existing->nickname = trimmedNickname;
        } else if (existing->nickname.isEmpty()) {
            existing->nickname = trimmedUserId;
        }

        m_authenticatedUser = *existing;
        if (!trimmedNickname.isEmpty()) {
            m_authenticatedUser.nickname = trimmedNickname;
        } else if (m_authenticatedUser.nickname.isEmpty()) {
            m_authenticatedUser.nickname = trimmedUserId;
        }
        return;
    }

    User user;
    user.userId = trimmedUserId;
    user.nickname = trimmedNickname.isEmpty() ? trimmedUserId : trimmedNickname;

    m_authenticatedUser = user;
    m_directory.prepend(user);
}

void AppController::loadRegistration()
{
    QSettings settings;
    const QString storedNickname = settings.value(QStringLiteral("registration/nickname")).toString().trimmed();
    const QString storedUserId = settings.value(QStringLiteral("registration/userId")).toString().trimmed();

    const Credential *credential = findCredentialByUserId(storedUserId);
    if (storedNickname.isEmpty() || storedUserId.isEmpty() || !credential) {
        m_isRegistered = false;
        if (storedNickname.isEmpty()) {
            m_registeredNickname.clear();
        } else {
            m_registeredNickname = storedNickname;
        }
        if (storedUserId.isEmpty() || !credential) {
            m_registeredUserId.clear();
        }
    } else {
        m_registeredUserId = credential->userId;
        m_registeredNickname = credential->nickname;
        m_isRegistered = true;
    }
}

void AppController::persistRegistration(const QString &userId, const QString &nickname)
{
    QSettings settings;
    settings.setValue(QStringLiteral("registration/userId"), userId.trimmed());
    settings.setValue(QStringLiteral("registration/nickname"), nickname.trimmed());
    settings.sync();
}

void AppController::loadServerData()
{
    m_directory.clear();
    m_conversations.clear();
    m_conversationOrder.clear();
    m_authenticatedRoles.clear();
    m_authenticatedUser = User{};
    m_lastServerMsgId.clear();
    m_knownServerMsgIds.clear();
    m_conversationActivity.clear();
    m_conversationReadMarkers.clear();
    m_peerReadMarkers.clear();
    m_nextMessageId = 1;

    const QString dataDir = resolveDataDirectory();
    const QString identityPath = QDir(dataDir).filePath(QStringLiteral("identity.db"));
    const QString messagesPath = QDir(dataDir).filePath(QStringLiteral("messages.db"));

    const bool usersLoaded = loadUserDirectory(identityPath);
    ensureDirectoryContainsAuthUser();
    if (m_authenticatedRoles.isEmpty()) {
        m_authenticatedRoles << QStringLiteral("user");
    }

    const bool historyLoaded = loadMessageHistory(messagesPath);
    loadReadMarkers();

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
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return false;
    }
    return applyDirectoryFromJson(doc);
}

bool AppController::applyDirectoryFromJson(const QJsonDocument &doc)
{
    if (!doc.isObject()) {
        return false;
    }

    const QJsonArray usersArray = doc.object().value(QStringLiteral("users")).toArray();
    if (usersArray.isEmpty()) {
        return false;
    }

    QList<User> parsedUsers;
    parsedUsers.reserve(usersArray.size());

    QString preferredUserId = m_registeredUserId.trimmed();
    if (preferredUserId.isEmpty()) {
        preferredUserId = qEnvironmentVariable("SM_AUTH_USER_ID").trimmed();
    }

    User selectedUser;
    QStringList selectedRoles;
    QStringList firstUserRoles;

    for (const QJsonValue &userValue : usersArray) {
        if (!userValue.isObject()) {
            continue;
        }

        const QJsonObject obj = userValue.toObject();
        User user;
        user.userId = obj.value(QStringLiteral("user_id")).toString().trimmed();
        user.nickname = obj.value(QStringLiteral("nickname")).toString(user.userId).trimmed();

        const QJsonValue devicesValue = obj.value(QStringLiteral("devices"));
        if (devicesValue.isObject()) {
            const QJsonObject devicesObj = devicesValue.toObject();
            for (auto it = devicesObj.constBegin(); it != devicesObj.constEnd(); ++it) {
                if (!it.value().isObject()) {
                    continue;
                }
                const QJsonObject deviceObj = it.value().toObject();
                Device device;
                device.deviceId = deviceObj.value(QStringLiteral("device_id")).toString(it.key());
                device.certificate = deviceObj.value(QStringLiteral("cert_der")).toString();
                device.revoked = deviceObj.value(QStringLiteral("revoked")).toBool(false);
                user.devices.append(device);
            }
        } else if (devicesValue.isArray()) {
            const QJsonArray devicesArray = devicesValue.toArray();
            for (const QJsonValue &deviceValue : devicesArray) {
                if (!deviceValue.isObject()) {
                    continue;
                }
                const QJsonObject deviceObj = deviceValue.toObject();
                Device device;
                device.deviceId = deviceObj.value(QStringLiteral("device_id")).toString(deviceObj.value(QStringLiteral("id")).toString());
                device.certificate = deviceObj.value(QStringLiteral("cert_der")).toString();
                device.revoked = deviceObj.value(QStringLiteral("revoked")).toBool(false);
                user.devices.append(device);
            }
        }

        parsedUsers.append(user);

        QStringList rolesForUser;
        const QJsonArray rolesArray = obj.value(QStringLiteral("roles")).toArray();
        for (const QJsonValue &roleValue : rolesArray) {
            const QString role = roleValue.toString().trimmed();
            if (!role.isEmpty()) {
                rolesForUser.append(role);
            }
        }
        if (parsedUsers.size() == 1) {
            firstUserRoles = rolesForUser;
        }
        if (!preferredUserId.isEmpty() && user.userId == preferredUserId) {
            selectedUser = user;
            selectedRoles = rolesForUser;
        }
    }

    if (parsedUsers.isEmpty()) {
        return false;
    }

    if (selectedUser.userId.isEmpty()) {
        selectedUser = parsedUsers.first();
        selectedRoles = firstUserRoles;
    }

    if (selectedRoles.isEmpty()) {
        selectedRoles.append(QStringLiteral("user"));
    }

    m_directory = parsedUsers;
    m_authenticatedUser = selectedUser;
    m_authenticatedRoles = selectedRoles;
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
        const QString conversationId = obj.value(QStringLiteral("conversation_id")).toString().trimmed();
        if (conversationId.isEmpty()) {
            continue;
        }
        if (!isConversationVisible(conversationId)) {
            continue;
        }
        const QString senderId = obj.value(QStringLiteral("sender_user_id")).toString();
        QString text = extractMessageText(obj);
        if (text.isEmpty()) {
            text = tr("Сообщение недоступно");
        }
        const QString serverMsgId = obj.value(QStringLiteral("server_msg_id")).toString().trimmed();
        const qint64 id = static_cast<qint64>(obj.value(QStringLiteral("id")).toDouble());
        const qint64 sentUnix = static_cast<qint64>(obj.value(QStringLiteral("sent_unix_sec")).toDouble());

        Message message;
        message.serverMsgId = !serverMsgId.isEmpty() ? serverMsgId : QStringLiteral("msg-%1").arg(id);
        message.senderUserId = senderId;
        message.author = nicknameForUserId(senderId);
        message.text = text;
        message.outgoing = senderId == m_authenticatedUser.userId;
        message.delivered = message.serverMsgId.startsWith(QStringLiteral("msg-"));
        message.readByPeer = false;
        if (sentUnix > 0) {
            message.sentUnixSec = sentUnix;
            message.timestamp = QDateTime::fromSecsSinceEpoch(sentUnix).toString(QStringLiteral("HH:mm:ss"));
        } else {
            message.sentUnixSec = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
            message.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
        }

        QList<Message> &conversation = m_conversations[conversationId];
        conversation.append(message);
        m_knownServerMsgIds.insert(message.serverMsgId);
        updateLastServerMsgId(message.serverMsgId);
        touchConversationActivity(conversationId, message.sentUnixSec);
    }

    rebuildConversationOrder();

    return true;
}

QString AppController::resolveDataDirectory() const
{
    const auto ensureWritableDir = [](const QString &path) -> QString {
        const QString trimmed = path.trimmed();
        if (trimmed.isEmpty()) {
            return {};
        }

        QDir dir(trimmed);
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            return {};
        }

        QFileInfo info(dir.absolutePath());
        if (!info.isWritable()) {
            return {};
        }

        return dir.absolutePath();
    };

    const auto writableFallback = [&ensureWritableDir]() -> QString {
        const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (const QString writable = ensureWritableDir(appData); !writable.isEmpty()) {
            return writable;
        }
        return ensureWritableDir(QDir::currentPath());
    };

    QString readOnlyMatch;

    const auto hasDataArtifacts = [](const QDir &dir) {
        return dir.exists(QStringLiteral("identity.db"))
               || dir.exists(QStringLiteral("messages.db"));
    };

    const auto canonicalIfValid = [&](const QString &path) -> QString {
        const QString trimmed = path.trimmed();
        if (trimmed.isEmpty()) {
            return {};
        }

        QFileInfo info(trimmed);
        if (!info.exists()) {
            return {};
        }

        QDir dir(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
        if (hasDataArtifacts(dir)) {
            return dir.absolutePath();
        }

        if (!info.isDir()) {
            const QString baseName = info.fileName();
            if (baseName == QStringLiteral("identity.db") || baseName == QStringLiteral("messages.db")) {
                return dir.absolutePath();
            }
        }

        QDir nested(dir);
        if (nested.cd(QStringLiteral("data")) && hasDataArtifacts(nested)) {
            return nested.absolutePath();
        }

        return {};
    };

    const auto searchParents = [&](const QString &start) -> QString {
        const QString trimmed = start.trimmed();
        if (trimmed.isEmpty()) {
            return {};
        }

        QDir probe(trimmed);
        QSet<QString> visited;
        while (true) {
            const QString absolute = probe.absolutePath();
            if (!visited.contains(absolute)) {
                visited.insert(absolute);
                const QString match = canonicalIfValid(absolute);
                if (!match.isEmpty()) {
                    return match;
                }
            }

            if (!probe.cdUp()) {
                break;
            }
        }

        return {};
    };

    const QString envPath = qEnvironmentVariable("SM_DATA_DIR").trimmed();
    if (const QString envMatch = canonicalIfValid(envPath); !envMatch.isEmpty()) {
        readOnlyMatch = envMatch;
        if (const QString writable = ensureWritableDir(envMatch); !writable.isEmpty()) {
            return writable;
        }
    }

    const QStringList explicitCandidates = {QStringLiteral("data"),
                                            QStringLiteral("../data"),
                                            QStringLiteral("../../data")};
    QDir base(QCoreApplication::applicationDirPath());
    for (const QString &candidate : explicitCandidates) {
        QDir probe(base);
        if (probe.cd(candidate)) {
            const QString match = canonicalIfValid(probe.absolutePath());
            if (!match.isEmpty()) {
                readOnlyMatch = match;
                if (const QString writable = ensureWritableDir(match); !writable.isEmpty()) {
                    return writable;
                }
            }
        }
    }

    const QStringList roots = {QCoreApplication::applicationDirPath(), QDir::currentPath()};
    for (const QString &root : roots) {
        const QString match = searchParents(root);
        if (!match.isEmpty()) {
            readOnlyMatch = match;
            if (const QString writable = ensureWritableDir(match); !writable.isEmpty()) {
                return writable;
            }
        }
    }

    QString fallback = ensureWritableDir(canonicalIfValid(QDir::currentPath()));
    if (fallback.isEmpty()) {
        fallback = writableFallback();
    }

    if (!fallback.isEmpty() && !readOnlyMatch.isEmpty() && fallback != readOnlyMatch) {
        QDir source(readOnlyMatch);
        QDir target(fallback);

        const QStringList artifacts = {QStringLiteral("identity.db"), QStringLiteral("messages.db")};
        for (const QString &artifact : artifacts) {
            const QString srcPath = source.filePath(artifact);
            const QString dstPath = target.filePath(artifact);
            if (!QFile::exists(dstPath) && QFile::exists(srcPath)) {
                QFile::copy(srcPath, dstPath);
            }
        }
    }

    if (!fallback.isEmpty()) {
        return fallback;
    }

    const QString writableCurrent = ensureWritableDir(QDir::currentPath());
    if (!writableCurrent.isEmpty()) {
        return writableCurrent;
    }

    const QString tempDir = ensureWritableDir(QDir::tempPath());
    return tempDir.isEmpty() ? QDir::currentPath() : tempDir;
}

QString AppController::nicknameForUserId(const QString &userId) const
{
    if (userId == m_authenticatedUser.userId) {
        const QString selfName = m_authenticatedUser.nickname.trimmed();
        if (!selfName.isEmpty()) {
            return selfName;
        }
    }
    for (const User &user : m_directory) {
        if (user.userId == userId) {
            const QString name = user.nickname.trimmed();
            if (!name.isEmpty()) {
                return name;
            }
            break;
        }
    }
    return tr("Неизвестный");
}

void AppController::fetchHistoryFromServer(const QString &sinceServerMsgId)
{
    if (!m_networkManager) {
        appendLog(QStringLiteral("Messaging.HTTP -> сетевой менеджер не инициализирован"));
        return;
    }

    if (!ensureSessionToken(false)) {
        appendLog(QStringLiteral("Messaging.HTTP -> нет действующего токена, синхронизация отменена"));
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
    applyAuthHeaders(request);
    auto *reply = m_networkManager->get(request);
    const bool initialLoad = marker.isEmpty();
    connect(reply, &QNetworkReply::finished, this, [this, reply, initialLoad]() {
        const QNetworkReply::NetworkError error = reply->error();
        const QString errorText = reply->errorString();
        const QByteArray payload = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (error != QNetworkReply::NoError) {
            appendLog(QStringLiteral("Messaging.HTTP -> ошибка получения истории: %1")
                          .arg(errorText));
            return;
        }

        if (statusCode >= 400) {
            const QString serverMsg = QString::fromUtf8(payload).trimmed();
            appendLog(QStringLiteral("Messaging.HTTP -> сервер вернул %1 %2")
                          .arg(statusCode)
                          .arg(serverMsg.isEmpty() ? QStringLiteral("")
                                                   : QStringLiteral("(%1)").arg(serverMsg)));
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

void AppController::fetchUsersFromServer()
{
    if (!m_isRegistered) {
        return;
    }
    if (!m_networkManager) {
        appendLog(QStringLiteral("Directory.HTTP -> сетевой менеджер не инициализирован"));
        return;
    }

    if (!ensureSessionToken(false)) {
        appendLog(QStringLiteral("Directory.HTTP -> нет действующего токена, запрос отклонён"));
        return;
    }

    const QUrl url = buildApiUrl(QStringLiteral("/api/auth/users"));
    if (!url.isValid()) {
        appendLog(QStringLiteral("Directory.HTTP -> некорректный адрес API (%1)").arg(m_apiBaseUrl));
        return;
    }

    appendLog(QStringLiteral("Directory.HTTP -> запрос каталога пользователей"));

    QNetworkRequest request(url);
    applyAuthHeaders(request);
    auto *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QNetworkReply::NetworkError error = reply->error();
        const QString errorText = reply->errorString();
        const QByteArray payload = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (error != QNetworkReply::NoError) {
            appendLog(QStringLiteral("Directory.HTTP -> ошибка получения каталога: %1").arg(errorText));
            return;
        }
        if (statusCode >= 400) {
            const QString serverMsg = QString::fromUtf8(payload).trimmed();
            appendLog(QStringLiteral("Directory.HTTP -> сервер вернул %1 %2")
                          .arg(statusCode)
                          .arg(serverMsg.isEmpty() ? QStringLiteral("")
                                                   : QStringLiteral("(%1)").arg(serverMsg)));
            return;
        }

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            appendLog(QStringLiteral("Directory.HTTP -> некорректный JSON: %1").arg(parseError.errorString()));
            return;
        }

        if (!applyDirectoryFromJson(doc)) {
            appendLog(QStringLiteral("Directory.HTTP -> не удалось обновить каталог"));
            return;
        }

        ensureDirectoryContainsAuthUser();

        const QString previousUserId = m_registeredUserId;
        const QString previousNickname = m_registeredNickname;

        if (!m_authenticatedUser.userId.isEmpty()) {
            m_registeredUserId = m_authenticatedUser.userId;
            m_registeredNickname = m_authenticatedUser.nickname;
            persistRegistration(m_registeredUserId, m_registeredNickname);
        }

        if (m_registeredUserId != previousUserId || m_registeredNickname != previousNickname) {
            emit registrationChanged();
        }

        appendLog(QStringLiteral("Directory.HTTP -> обновлено, %1 профиля")
                      .arg(m_directory.size()));

        emit authInfoChanged();
        emit userListChanged();
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
        if (!isConversationVisible(conversationId)) {
            continue;
        }
        const QString senderUserId = obj.value(QStringLiteral("sender_user_id")).toString();
        const QString text = extractMessageText(obj);
        const qint64 sentUnixSec = static_cast<qint64>(obj.value(QStringLiteral("sent_unix_sec")).toDouble());

        const QString author = nicknameForUserId(senderUserId);
        const bool outgoing = senderUserId == m_authenticatedUser.userId;
        addServerMessage(conversationId, serverMsgId, senderUserId, author, text, outgoing, sentUnixSec);
        ++added;
    }

    const QString lastId = root.value(QStringLiteral("last_server_msg_id")).toString().trimmed();
    if (!lastId.isEmpty()) {
        updateLastServerMsgId(lastId);
    }

    applyReadMarkersFromServer(root);

    return added;
}

void AppController::postMessageToServer(const QString &conversationId, const QString &text)
{
    if (!m_networkManager) {
        appendLog(QStringLiteral("Messaging.Send -> сетевой менеджер не инициализирован"));
        return;
    }

    if (!ensureSessionToken(false)) {
        appendLog(QStringLiteral("Messaging.Send -> нет действующего токена, сообщение не отправлено"));
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
    applyAuthHeaders(request);
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
        QString deliveredText = extractMessageText(obj);
        if (deliveredText.isEmpty()) {
            deliveredText = text;
        }
        const qint64 sentUnixSec = static_cast<qint64>(obj.value(QStringLiteral("sent_unix_sec")).toDouble());

        if (!serverMsgId.isEmpty() && !m_knownServerMsgIds.contains(serverMsgId)) {
            const QString author = nicknameForUserId(senderUserId);
            addServerMessage(convId,
                             serverMsgId,
                             senderUserId,
                             author,
                             deliveredText,
                             senderUserId == m_authenticatedUser.userId,
                             sentUnixSec);
            appendLog(QStringLiteral("Messaging.Send -> доставлено %1 (conv=%2)")
                          .arg(serverMsgId, convId));
        }
    });
}

void AppController::applyReadMarkersFromServer(const QJsonObject &root)
{
    const QJsonObject markersObj = root.value(QStringLiteral("read_markers")).toObject();
    if (markersObj.isEmpty()) {
        return;
    }

    bool persisted = false;
    for (auto it = markersObj.constBegin(); it != markersObj.constEnd(); ++it) {
        const QString conversationId = it.key().trimmed();
        if (conversationId.isEmpty()) {
            continue;
        }
        const QJsonObject perConversation = it.value().toObject();
        for (auto markIt = perConversation.constBegin(); markIt != perConversation.constEnd(); ++markIt) {
            const QString userId = markIt.key().trimmed();
            const qint64 marker = parseServerMsgNumeric(markIt.value().toString());
            if (userId.isEmpty() || marker <= 0) {
                continue;
            }
            if (QString::compare(userId, m_authenticatedUser.userId, Qt::CaseInsensitive) == 0) {
                if (marker > m_conversationReadMarkers.value(conversationId, 0)) {
                    m_conversationReadMarkers.insert(conversationId, marker);
                    persisted = true;
                }
            } else {
                const qint64 currentPeer = m_peerReadMarkers.value(conversationId, 0);
                if (marker > currentPeer) {
                    m_peerReadMarkers.insert(conversationId, marker);
                }
            }
        }
        updatePeerReadState(conversationId);
    }

    if (persisted) {
        persistReadMarkers();
    }
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

qint64 AppController::latestServerMessageId(const QString &conversationId) const
{
    qint64 latest = 0;
    const QList<Message> messages = m_conversations.value(conversationId);
    for (const Message &message : messages) {
        const qint64 numeric = parseServerMsgNumeric(message.serverMsgId);
        if (numeric > latest) {
            latest = numeric;
        }
    }
    return latest;
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
                                      const QString &senderUserId,
                                      const QString &author,
                                      const QString &text,
                                      bool outgoing,
                                      qint64 sentUnixSec)
{
    const QString trimmedId = conversationId.trimmed();
    if (trimmedId.isEmpty() || serverMsgId.trimmed().isEmpty()) {
        return;
    }
    if (!isConversationVisible(trimmedId)) {
        return;
    }

    Message message;
    message.serverMsgId = serverMsgId;
    message.senderUserId = senderUserId;
    message.author = author;
    message.text = text;
    message.outgoing = outgoing;
    message.delivered = true;
    if (sentUnixSec > 0) {
        message.sentUnixSec = sentUnixSec;
        message.timestamp = QDateTime::fromSecsSinceEpoch(sentUnixSec).toString(QStringLiteral("HH:mm:ss"));
    } else {
        message.sentUnixSec = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        message.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    }

    QList<Message> &messages = m_conversations[trimmedId];
    messages.append(message);
    m_knownServerMsgIds.insert(serverMsgId);
    updateLastServerMsgId(serverMsgId);
    touchConversationActivity(trimmedId, message.sentUnixSec);

    updatePeerReadState(trimmedId);

    promoteConversation(trimmedId, message.sentUnixSec);
    if (trimmedId == m_currentConversation) {
        markConversationRead(trimmedId);
    }
    emit conversationListChanged();

    if (trimmedId == m_currentConversation) {
        emit conversationChanged();
    }

    persistMessageHistory();
}

QString AppController::addMessage(const QString &conversationId, const QString &author, const QString &text, bool outgoing)
{
    const QString trimmedId = conversationId.trimmed();
    if (trimmedId.isEmpty()) {
        return {};
    }

    Message message;
    message.serverMsgId = QStringLiteral("local-%1").arg(m_nextMessageId++);
    message.senderUserId = outgoing ? m_authenticatedUser.userId : QString();
    message.author = author;
    message.text = text;
    message.outgoing = outgoing;
    message.delivered = false;
    message.readByPeer = false;
    message.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    message.sentUnixSec = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    QList<Message> &messages = m_conversations[trimmedId];
    messages.append(message);
    promoteConversation(trimmedId, message.sentUnixSec);
    if (trimmedId == m_currentConversation) {
        markConversationRead(trimmedId);
    }
    emit conversationListChanged();
    if (trimmedId == m_currentConversation) {
        emit conversationChanged();
    }
    persistMessageHistory();
    return message.serverMsgId;
}

void AppController::appendLog(const QString &entry)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_serverLog.append(QStringLiteral("[%1] %2").arg(ts, entry));
    emit serverLogChanged();
}

void AppController::persistMessageHistory()
{
    const QString dataDir = resolveDataDirectory();
    if (dataDir.isEmpty()) {
        return;
    }

    QDir dir(dataDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return;
    }

    QJsonArray messages;
    for (auto it = m_conversations.constBegin(); it != m_conversations.constEnd(); ++it) {
        const QString conversationId = it.key();
        for (const Message &message : it.value()) {
            QJsonObject obj;
            obj.insert(QStringLiteral("server_msg_id"), message.serverMsgId);
            obj.insert(QStringLiteral("conversation_id"), conversationId);
            obj.insert(QStringLiteral("sender_user_id"), message.senderUserId);
            obj.insert(QStringLiteral("sent_unix_sec"), static_cast<double>(message.sentUnixSec));
            obj.insert(QStringLiteral("text"), message.text);
            messages.append(obj);
        }
    }

    QJsonObject root;
    root.insert(QStringLiteral("messages"), messages);

    QSaveFile file(dir.filePath(QStringLiteral("messages.db")));
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
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

void AppController::touchConversationActivity(const QString &conversationId, qint64 unixTimestamp)
{
    const QString trimmed = conversationId.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    qint64 effectiveTs = unixTimestamp;
    if (effectiveTs <= 0) {
        const qint64 lastKnown = lastActivityForConversation(trimmed);
        if (lastKnown > 0) {
            effectiveTs = lastKnown;
        } else {
            effectiveTs = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        }
    }
    const auto it = m_conversationActivity.find(trimmed);
    if (it == m_conversationActivity.end() || effectiveTs > it.value()) {
        m_conversationActivity.insert(trimmed, effectiveTs);
    }
}

qint64 AppController::lastActivityForConversation(const QString &conversationId) const
{
    const QList<Message> &messages = m_conversations.value(conversationId);
    if (!messages.isEmpty()) {
        const Message &last = messages.constLast();
        if (last.sentUnixSec > 0) {
            return last.sentUnixSec;
        }
        const qint64 parsed = parseServerMsgNumeric(last.serverMsgId);
        if (parsed > 0) {
            return parsed;
        }
    }

    return m_conversationActivity.value(conversationId, 0);
}

void AppController::loadReadMarkers()
{
    QSettings settings;
    const QVariantMap stored = settings.value(QStringLiteral("messaging/readMarkers")).toMap();
    for (auto it = stored.constBegin(); it != stored.constEnd(); ++it) {
        const QString conversationId = it.key().trimmed();
        const qint64 marker = it.value().toLongLong();
        if (conversationId.isEmpty() || marker <= 0) {
            continue;
        }
        if (!isConversationVisible(conversationId)) {
            continue;
        }
        if (!m_conversations.contains(conversationId)) {
            continue;
        }
        m_conversationReadMarkers.insert(conversationId, marker);
    }
}

void AppController::persistReadMarkers() const
{
    QSettings settings;
    QVariantMap serialized;
    for (auto it = m_conversationReadMarkers.constBegin(); it != m_conversationReadMarkers.constEnd(); ++it) {
        if (it.value() > 0) {
            serialized.insert(it.key(), it.value());
        }
    }
    settings.setValue(QStringLiteral("messaging/readMarkers"), serialized);
    settings.sync();
}

void AppController::syncReadMarkerWithServer(const QString &conversationId, qint64 lastServerMsgId)
{
    if (!m_isRegistered || !m_networkManager || lastServerMsgId <= 0) {
        return;
    }
    if (!ensureSessionToken(false)) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("conversation_id"), conversationId);
    payload.insert(QStringLiteral("last_server_msg_id"), QStringLiteral("msg-%1").arg(lastServerMsgId));

    const QUrl url = buildApiUrl(QStringLiteral("/api/read_markers"));
    if (!url.isValid()) {
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    applyAuthHeaders(request);
    auto *reply = m_networkManager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply]() { reply->deleteLater(); });
}

void AppController::markConversationRead(const QString &conversationId)
{
    const QString trimmed = conversationId.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    const qint64 lastServerMsgId = latestServerMessageId(trimmed);
    if (lastServerMsgId > 0) {
        const qint64 previous = m_conversationReadMarkers.value(trimmed, 0);
        m_conversationReadMarkers.insert(trimmed, lastServerMsgId);
        if (lastServerMsgId > previous) {
            syncReadMarkerWithServer(trimmed, lastServerMsgId);
        }
    } else {
        m_conversationReadMarkers.remove(trimmed);
    }
    persistReadMarkers();
}

int AppController::unreadCountFor(const QString &conversationId) const
{
    const qint64 readMarker = m_conversationReadMarkers.value(conversationId, 0);
    int unread = 0;

    const QList<Message> messages = m_conversations.value(conversationId);
    for (const Message &message : messages) {
        const qint64 serverId = parseServerMsgNumeric(message.serverMsgId);
        const qint64 timestamp = serverId > 0 ? serverId : message.sentUnixSec;
        if (timestamp > readMarker && !message.outgoing) {
            ++unread;
        }
    }

    return unread;
}

void AppController::updatePeerReadState(const QString &conversationId)
{
    const qint64 peerMarker = m_peerReadMarkers.value(conversationId, 0);
    if (peerMarker <= 0) {
        return;
    }

    QList<Message> &messages = m_conversations[conversationId];
    bool changed = false;
    for (Message &message : messages) {
        if (!message.outgoing) {
            continue;
        }
        const qint64 msgId = parseServerMsgNumeric(message.serverMsgId);
        const bool read = msgId > 0 && msgId <= peerMarker;
        if (read != message.readByPeer) {
            message.readByPeer = read;
            changed = true;
        }
        if (msgId > 0 && !message.delivered) {
            message.delivered = true;
            changed = true;
        }
    }

    if (changed) {
        emit conversationListChanged();
        if (conversationId == m_currentConversation) {
            emit conversationChanged();
        }
    }
}

void AppController::promoteConversation(const QString &conversationId, qint64 activityHint)
{
    const QString trimmed = conversationId.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (!isConversationVisible(trimmed)) {
        return;
    }
    if (!m_conversations.contains(trimmed)) {
        m_conversations.insert(trimmed, {});
    }
    qint64 activity = activityHint;
    if (activity <= 0) {
        activity = lastActivityForConversation(trimmed);
    }
    touchConversationActivity(trimmed, activity);
    rebuildConversationOrder();
}

void AppController::rebuildConversationOrder()
{
    QStringList keys;
    for (auto it = m_conversations.constBegin(); it != m_conversations.constEnd(); ++it) {
        if (isConversationVisible(it.key())) {
            keys.append(it.key());
        }
    }
    auto scoreFor = [this](const QString &id) -> qint64 {
        const QList<Message> &messages = m_conversations.value(id);
        qint64 score = m_conversationActivity.value(id, 0);
        if (!messages.isEmpty()) {
            const Message &last = messages.constLast();
            if (last.sentUnixSec > 0) {
                score = std::max(score, last.sentUnixSec);
            } else {
                score = std::max(score, parseServerMsgNumeric(last.serverMsgId));
            }
        }
        return score;
    };

    std::sort(keys.begin(), keys.end(), [&](const QString &left, const QString &right) {
        const qint64 leftScore = scoreFor(left);
        const qint64 rightScore = scoreFor(right);
        if (leftScore == rightScore) {
            return left < right;
        }
        return leftScore > rightScore;
    });

    m_conversationOrder = keys;
}

QString AppController::conversationDisplayName(const QString &conversationId) const
{
    const QString trimmed = conversationId.trimmed();
    if (trimmed.compare(QStringLiteral("corp-secure-room"), Qt::CaseInsensitive) == 0) {
        return tr("Общий канал");
    }

    const QString myId = m_authenticatedUser.userId.trimmed();
    if (!myId.isEmpty() && trimmed.startsWith(QStringLiteral("dm-"))) {
        const QString payload = trimmed.mid(3);
        QString partnerId;
        const QString prefix = myId + QLatin1Char('-');
        if (payload.startsWith(prefix)) {
            partnerId = payload.mid(prefix.size());
        } else {
            const QString suffix = QLatin1Char('-') + myId;
            if (payload.endsWith(suffix)) {
                partnerId = payload.left(payload.size() - suffix.size());
            }
        }
        if (!partnerId.isEmpty()) {
            const QString partnerName = nicknameForUserId(partnerId);
            return partnerName;
        }
    }

    return trimmed;
}

QString AppController::conversationSubtitle(const QString &conversationId) const
{
    const QString trimmed = conversationId.trimmed();
    if (trimmed.compare(QStringLiteral("corp-secure-room"), Qt::CaseInsensitive) == 0) {
        return tr("Внутренний канал");
    }

    const QString myId = m_authenticatedUser.userId.trimmed();
    if (!myId.isEmpty() && trimmed.startsWith(QStringLiteral("dm-"))) {
        const QString payload = trimmed.mid(3);
        const QString prefix = myId + QLatin1Char('-');
        const QString suffix = QLatin1Char('-') + myId;
        if (payload.startsWith(prefix) || payload.endsWith(suffix)) {
            return tr("Личный чат");
        }
    }

    return QString();
}

QString AppController::conversationPeerId(const QString &conversationId) const
{
    const QString trimmed = conversationId.trimmed();
    if (!trimmed.startsWith(QStringLiteral("dm-"))) {
        return {};
    }
    const QStringList parts = trimmed.mid(3).split(QStringLiteral("-"), Qt::SkipEmptyParts);
    if (parts.size() != 2) {
        return {};
    }
    const QString selfId = m_authenticatedUser.userId;
    if (QString::compare(parts.first(), selfId, Qt::CaseInsensitive) == 0) {
        return parts.last();
    }
    if (QString::compare(parts.last(), selfId, Qt::CaseInsensitive) == 0) {
        return parts.first();
    }
    return {};
}

bool AppController::isConversationVisible(const QString &conversationId) const
{
    const QString trimmed = conversationId.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const QString lowered = trimmed.toLower();
    if (lowered == QStringLiteral("corp-secure-room")) {
        return true;
    }

    const QString myId = m_authenticatedUser.userId.trimmed();
    if (myId.isEmpty()) {
        return false;
    }

    if (lowered.startsWith(QStringLiteral("dm-"))) {
        const QString myIdLower = myId.toLower();
        return lowered.contains(myIdLower);
    }

    return false;
}

void AppController::loadCredentials()
{
    m_credentials.clear();

    const QString path = identityStoreFilePath();
    QFile file(path);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    const QJsonArray users = doc.object().value(QStringLiteral("users")).toArray();
    for (const QJsonValue &value : users) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        Credential credential;
        credential.userId = obj.value(QStringLiteral("user_id")).toString().trimmed();
        credential.nickname = obj.value(QStringLiteral("nickname")).toString(credential.userId).trimmed();
        credential.password = obj.value(QStringLiteral("password")).toString();
        credential.certificateDer = obj.value(QStringLiteral("cert_der")).toString().trimmed();
        if (credential.userId.isEmpty() || credential.nickname.isEmpty() || credential.password.trimmed().isEmpty()) {
            continue;
        }
        credential.password = credential.password.trimmed();
        m_credentials.append(credential);
    }
}

bool AppController::persistCredentials() const
{
    const QString path = identityStoreFilePath();
    QFileInfo info(path);
    QDir dir = info.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            return false;
        }
    }

    QJsonObject root;
    QJsonArray existingUsers;

    {
        QFile file(path);
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QJsonParseError parseError{};
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                root = doc.object();
                existingUsers = root.value(QStringLiteral("users")).toArray();
            }
        }
    }

    QHash<QString, QJsonObject> usersById;
    QStringList order;
    QJsonArray preserved;

    for (const QJsonValue &value : existingUsers) {
        if (!value.isObject()) {
            preserved.append(value);
            continue;
        }
        const QJsonObject obj = value.toObject();
        const QString userId = obj.value(QStringLiteral("user_id")).toString().trimmed();
        if (userId.isEmpty()) {
            preserved.append(value);
            continue;
        }
        usersById.insert(userId, obj);
        order.append(userId);
    }

    for (const Credential &credential : m_credentials) {
        const QString userId = credential.userId.trimmed();
        const QString nickname = credential.nickname.trimmed();
        const QString password = credential.password.trimmed();
        if (userId.isEmpty() || nickname.isEmpty() || password.isEmpty()) {
            continue;
        }

        QJsonObject obj = usersById.value(userId);
        obj.insert(QStringLiteral("user_id"), userId);
        obj.insert(QStringLiteral("nickname"), nickname);
        obj.insert(QStringLiteral("password"), password);
        if (!credential.certificateDer.trimmed().isEmpty()) {
            obj.insert(QStringLiteral("cert_der"), credential.certificateDer.trimmed());
        }

        const QJsonValue rolesValue = obj.value(QStringLiteral("roles"));
        if (!rolesValue.isArray() || rolesValue.toArray().isEmpty()) {
            obj.insert(QStringLiteral("roles"), QJsonArray{QJsonValue(QStringLiteral("user"))});
        }
        if (!obj.contains(QStringLiteral("devices"))) {
            obj.insert(QStringLiteral("devices"), QJsonObject());
        }

        usersById.insert(userId, obj);
        if (!order.contains(userId)) {
            order.append(userId);
        }
    }

    QJsonArray users;
    for (const QString &userId : order) {
        users.append(usersById.value(userId));
    }
    for (const QJsonValue &value : preserved) {
        users.append(value);
    }
    root.insert(QStringLiteral("users"), users);

    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (file.write(data) == data.size() && file.commit()) {
            return true;
        }
    }

    QFile fallback(path);
    if (!fallback.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (fallback.write(data) != data.size()) {
        return false;
    }

    fallback.flush();
    return fallback.error() == QFile::NoError;
}

QString AppController::identityStoreFilePath() const
{
    return QDir(resolveDataDirectory()).filePath(QStringLiteral("identity.db"));
}

QString AppController::generateUserIdForNickname(const QString &nickname) const
{
    QString sanitized = nickname.trimmed().toLower();
    sanitized.replace(QRegularExpression(QStringLiteral("[^a-z0-9_-]+")), QStringLiteral("-"));
    while (sanitized.startsWith(QLatin1Char('-'))) {
        sanitized.remove(0, 1);
    }
    while (sanitized.endsWith(QLatin1Char('-'))) {
        sanitized.chop(1);
    }
    if (sanitized.isEmpty()) {
        sanitized = QStringLiteral("user");
    }

    QString candidate = QStringLiteral("local-%1").arg(sanitized);
    int counter = 1;
    while (userIdExists(candidate)) {
        candidate = QStringLiteral("local-%1-%2").arg(sanitized).arg(++counter);
    }
    return candidate;
}

bool AppController::userIdExists(const QString &userId) const
{
    for (const Credential &credential : m_credentials) {
        if (credential.userId.compare(userId, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    for (const User &user : m_directory) {
        if (user.userId.compare(userId, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

AppController::Credential *AppController::findCredentialByNickname(const QString &nickname)
{
    const QString trimmed = nickname.trimmed();
    for (Credential &credential : m_credentials) {
        if (QString::compare(credential.nickname, trimmed, Qt::CaseInsensitive) == 0) {
            return &credential;
        }
    }
    return nullptr;
}

const AppController::Credential *AppController::findCredentialByNickname(const QString &nickname) const
{
    const QString trimmed = nickname.trimmed();
    for (const Credential &credential : m_credentials) {
        if (QString::compare(credential.nickname, trimmed, Qt::CaseInsensitive) == 0) {
            return &credential;
        }
    }
    return nullptr;
}

AppController::Credential* AppController::findCredentialByUserId(const QString& userId) {
    auto it = std::find_if(m_credentials.begin(), m_credentials.end(),
                           [&](const Credential& c){ return c.userId == userId; });
    return (it != m_credentials.end()) ? &(*it) : nullptr;
}

const AppController::Credential *AppController::findCredentialByUserId(const QString &userId) const
{
    const QString trimmed = userId.trimmed();
    if (trimmed.isEmpty()) {
        return nullptr;
    }
    for (const Credential &credential : m_credentials) {
        if (QString::compare(credential.userId, trimmed, Qt::CaseInsensitive) == 0) {
            return &credential;
        }
    }
    return nullptr;
}

void AppController::setAuthBusy(bool busy)
{
    if (m_authBusy == busy) {
        return;
    }
    m_authBusy = busy;
    emit authBusyChanged();
}

QString AppController::resolveCertificatePath(const QString &path) const
{
    QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }

    const QUrl candidateUrl(trimmed);
    if (candidateUrl.isValid() && !candidateUrl.scheme().isEmpty()) {
        if (candidateUrl.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) == 0) {
            const QString local = candidateUrl.toLocalFile();
            if (!local.isEmpty()) {
                trimmed = local;
            }
        } else {
            return trimmed;
        }
    }

    if (trimmed == QStringLiteral("~")) {
        trimmed = QDir::homePath();
    } else if (trimmed.startsWith(QStringLiteral("~/"))) {
        QDir homeDir = QDir::home();
        trimmed = homeDir.filePath(trimmed.mid(2));
    }

    QFileInfo info(trimmed);
    if (!info.isAbsolute()) {
        trimmed = QDir::current().absoluteFilePath(trimmed);
    } else {
        trimmed = info.absoluteFilePath();
    }

    return QDir::cleanPath(trimmed);
}

bool AppController::loadCertificateFromFile(const QString &path, QByteArray &der, QString &error) const
{
    const QString resolvedPath = resolveCertificatePath(path);
    QFile certFile(resolvedPath);
    if (!certFile.exists()) {
        error = tr("Файл сертификата не найден: %1").arg(resolvedPath);
        return false;
    }
    if (!certFile.open(QIODevice::ReadOnly)) {
        error = tr("Не удалось прочитать сертификат: %1").arg(certFile.errorString());
        return false;
    }
    const QByteArray certData = certFile.readAll();
    certFile.close();

    QList<QSslCertificate> parsed = QSslCertificate::fromData(certData, QSsl::Pem);
    if (parsed.isEmpty()) {
        parsed = QSslCertificate::fromData(certData, QSsl::Der);
    }
    if (parsed.isEmpty() || parsed.first().isNull()) {
        error = tr("Файл не содержит валидный сертификат");
        return false;
    }
    der = parsed.first().toDer();
    if (der.isEmpty()) {
        error = tr("Не удалось преобразовать сертификат");
        return false;
    }
    return true;
}

QString AppController::sendAuthRequest(const QString &path,
                                       const QString &operation,
                                       const QJsonObject &payload,
                                       QJsonObject &response,
                                       bool markBusy)
{
    if (!m_networkManager) {
        return tr("%1: сетевой менеджер не инициализирован").arg(operation);
    }

    const QUrl url = buildApiUrl(path);
    if (!url.isValid()) {
        return tr("%1: некорректный адрес API (%2)").arg(operation, m_apiBaseUrl);
    }

    QScopeGuard busyGuard([this, markBusy]() {
        if (markBusy) {
            setAuthBusy(false);
        }
    });
    if (markBusy) {
        setAuthBusy(true);
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QEventLoop loop;
    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QNetworkReply::NetworkError networkError = reply->error();
    const QByteArray responseBytes = reply->readAll();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorText = reply->errorString();
    const QString serverMsg = QString::fromUtf8(responseBytes).trimmed();
    reply->deleteLater();

    if (networkError != QNetworkReply::NoError) {
        if (networkError == QNetworkReply::AuthenticationRequiredError) {
            if (!serverMsg.isEmpty()) {
                return tr("%1: %2").arg(operation, serverMsg);
            }
            return tr("%1: сервер запросил HTTP-аутентификацию").arg(operation);
        }
    }
    if (statusCode >= 400) {
        
        if (!serverMsg.isEmpty()) {
            return tr("%1: %2").arg(operation, serverMsg);
        }
        if (statusCode == 401 && networkError == QNetworkReply::AuthenticationRequiredError) {
            return tr("%1: проверьте никнейм и пароль").arg(operation);
        }
        if (networkError != QNetworkReply::NoError && !errorText.isEmpty()) {
            return tr("%1: ошибка запроса: %2").arg(operation, errorText);
        }
        return tr("%1: сервер вернул ошибку (%2)").arg(operation).arg(statusCode);
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(responseBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return tr("%1: сервер вернул некорректный ответ").arg(operation);
    }

    response = doc.object();
    return {};
}

bool AppController::parseAuthSession(const QJsonObject &obj, AuthSession &session, QString &error) const
{
    const QString userId = obj.value(QStringLiteral("user_id")).toString().trimmed();
    if (userId.isEmpty()) {
        error = tr("Сервер не присвоил идентификатор пользователю");
        return false;
    }
    QString nickname = obj.value(QStringLiteral("nickname")).toString(userId).trimmed();
    if (nickname.isEmpty()) {
        nickname = userId;
    }
    const QString token = obj.value(QStringLiteral("token")).toString().trimmed();
    if (token.isEmpty()) {
        error = tr("Сервер не выдал токен авторизации");
        return false;
    }

    QStringList roles;
    const QJsonArray rolesArray = obj.value(QStringLiteral("roles")).toArray();
    for (const QJsonValue &value : rolesArray) {
        const QString role = value.toString().trimmed();
        if (!role.isEmpty()) {
            roles.append(role);
        }
    }
    if (roles.isEmpty()) {
        roles << QStringLiteral("user");
    }

    const QString certificate = obj.value(QStringLiteral("certificate")).toString().trimmed();
    QDateTime expires = QDateTime::fromString(obj.value(QStringLiteral("expires_at")).toString().trimmed(), Qt::ISODate);
    if (!expires.isValid()) {
        expires = QDateTime::currentDateTimeUtc().addSecs(3600);
    }
    if (expires.timeSpec() != Qt::UTC) {
        expires = expires.toUTC();
    }

    session.userId = userId;
    session.nickname = nickname;
    session.roles = roles;
    session.token = token;
    session.expiresAtUtc = expires;
    session.certificateBase64 = certificate;
    return true;
}

QString AppController::storeCredential(const QString &userId,
                                       const QString &nickname,
                                       const QString &password,
                                       const QString &certificateBase64,
                                       bool persistFile)
{
    const QString trimmedUserId = userId.trimmed();
    const QString trimmedNickname = nickname.trimmed();
    const QString trimmedPassword = password.trimmed();
    const QString trimmedCert = certificateBase64.trimmed();

    Credential *target = nullptr;
    if (!trimmedUserId.isEmpty()) {
        target = findCredentialByUserId(trimmedUserId);
    }
    if (!target && !trimmedNickname.isEmpty()) {
        target = findCredentialByNickname(trimmedNickname);
    }

    Credential backup;
    bool created = false;
    if (!target) {
        if (!persistFile) {
            return tr("Сохранённые учётные данные недоступны");
        }
        Credential credential;
        credential.userId = trimmedUserId;
        credential.nickname = trimmedNickname.isEmpty() ? trimmedUserId : trimmedNickname;
        credential.password = trimmedPassword;
        credential.certificateDer = trimmedCert;
        m_credentials.append(credential);
        target = &m_credentials.last();
        created = true;
    } else {
        backup = *target;
        if (!trimmedUserId.isEmpty()) {
            target->userId = trimmedUserId;
        }
        if (!trimmedNickname.isEmpty()) {
            target->nickname = trimmedNickname;
        }
        if (!trimmedPassword.isEmpty()) {
            target->password = trimmedPassword;
        }
        if (!trimmedCert.isEmpty()) {
            target->certificateDer = trimmedCert;
        }
    }

    if (persistFile && !persistCredentials()) {
        if (created) {
            m_credentials.removeLast();
        } else {
            *target = backup;
        }
        return tr("Не удалось сохранить данные пользователя");
    }

    return {};
}

bool AppController::applySessionState(const AuthSession &session)
{
    const QString previousUserId = m_registeredUserId;
    const QString previousNickname = m_registeredNickname;

    m_accessToken = session.token.trimmed();
    m_tokenExpiry = session.expiresAtUtc;
    if (!m_tokenExpiry.isValid()) {
        m_tokenExpiry = QDateTime::currentDateTimeUtc().addSecs(3600);
    }
    if (m_tokenExpiry.timeSpec() != Qt::UTC) {
        m_tokenExpiry = m_tokenExpiry.toUTC();
    }

    m_authenticatedRoles = session.roles;
    if (m_authenticatedRoles.isEmpty()) {
        m_authenticatedRoles = QStringList{QStringLiteral("user")};
    }

    m_registeredUserId = session.userId.trimmed();
    if (m_registeredUserId.isEmpty()) {
        m_registeredUserId = QStringLiteral("user-unknown");
    }
    m_registeredNickname = session.nickname.trimmed();
    if (m_registeredNickname.isEmpty()) {
        m_registeredNickname = m_registeredUserId;
    }
    m_isRegistered = true;

    m_authenticatedUser.userId = m_registeredUserId;
    m_authenticatedUser.nickname = m_registeredNickname;

    persistRegistration(m_registeredUserId, m_registeredNickname);

    return previousUserId != m_registeredUserId || previousNickname != m_registeredNickname;
}

bool AppController::ensureSessionToken(bool logErrors)
{
    if (!m_isRegistered) {
        return false;
    }
    if (hasValidSessionToken()) {
        return true;
    }

    const Credential *credential = findCredentialByUserId(m_registeredUserId);
    if (!credential && !m_registeredNickname.isEmpty()) {
        credential = findCredentialByNickname(m_registeredNickname);
    }
    if (!credential) {
        if (logErrors) {
            appendLog(QStringLiteral("Auth.Session -> сохранённых учётных данных не найдено"));
        }
        return false;
    }
    if (credential->password.trimmed().isEmpty() || credential->certificateDer.trimmed().isEmpty()) {
        if (logErrors) {
            appendLog(QStringLiteral("Auth.Session -> учётные данные неполные"));
        }
        return false;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("nickname"), credential->nickname);
    payload.insert(QStringLiteral("password"), credential->password);
    payload.insert(QStringLiteral("certificate"), credential->certificateDer);

    QJsonObject response;
    const QString requestError = sendAuthRequest(QStringLiteral("/api/auth/login"),
                                                 tr("Вход"),
                                                 payload,
                                                 response,
                                                 false);
    if (!requestError.isEmpty()) {
        if (logErrors) {
            appendLog(QStringLiteral("Auth.Session -> %1").arg(requestError));
        }
        return false;
    }

    AuthSession session;
    QString parseError;
    if (!parseAuthSession(response, session, parseError)) {
        if (logErrors) {
            appendLog(QStringLiteral("Auth.Session -> %1").arg(parseError));
        }
        return false;
    }
    if (session.certificateBase64.isEmpty()) {
        session.certificateBase64 = credential->certificateDer;
    }

    const QString storeError = storeCredential(session.userId,
                                               session.nickname,
                                               credential->password,
                                               session.certificateBase64,
                                               false);
    if (!storeError.isEmpty()) {
        if (logErrors) {
            appendLog(QStringLiteral("Auth.Session -> %1").arg(storeError));
        }
        return false;
    }

    const bool changed = applySessionState(session);
    if (logErrors) {
        appendLog(QStringLiteral("Auth.Session -> получен новый токен, действует до %1")
                      .arg(m_tokenExpiry.toLocalTime().toString(QStringLiteral("HH:mm"))));
    }

    emit authInfoChanged();
    if (changed) {
        emit registrationChanged();
    }

    return true;
}

bool AppController::hasValidSessionToken() const
{
    if (m_accessToken.trimmed().isEmpty()) {
        return false;
    }
    if (!m_tokenExpiry.isValid()) {
        return false;
    }
    return QDateTime::currentDateTimeUtc() < m_tokenExpiry.addSecs(-30);
}

void AppController::applyAuthHeaders(QNetworkRequest &request) const
{
    const QString token = m_accessToken.trimmed();
    if (token.isEmpty()) {
        return;
    }
    request.setRawHeader(QByteArrayLiteral("Authorization"),
                         QByteArrayLiteral("Bearer ") + token.toUtf8());
}
