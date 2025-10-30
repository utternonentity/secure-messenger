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
#include <QScopedValueRollback>
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
        return;
    }
    if (!m_conversations.contains(trimmed)) {
        m_conversations.insert(trimmed, {});
    }
    promoteConversation(trimmed);
    emit conversationListChanged();

    if (trimmed == m_currentConversation) {
        return;
    }

    m_currentConversation = trimmed;
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
    promoteConversation(m_currentConversation);
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

    if (m_isRegistered) {
        fetchUsersFromServer();
    }
}

void AppController::simulatePull()
{
    appendLog(QStringLiteral("Messaging.Pull -> ручной запрос обновлений"));
    fetchHistoryFromServer(m_lastServerMsgId);
}

QString AppController::authenticate(const QString &nickname, const QString &password)
{
    const QString trimmedNickname = nickname.trimmed();
    if (trimmedNickname.isEmpty()) {
        return tr("Введите никнейм");
    }
    if (password.trimmed().isEmpty()) {
        return tr("Введите пароль");
    }

    Credential *credential = findCredentialByNickname(trimmedNickname);
    if (!credential) {
        return tr("Пользователь не найден");
    }
    if (credential->password != password) {
        return tr("Неверный пароль");
    }

    m_registeredUserId = credential->userId;
    m_registeredNickname = credential->nickname;
    m_isRegistered = true;

    persistRegistration(m_registeredUserId, m_registeredNickname);

    appendLog(QStringLiteral("Auth.Login -> пользователь %1 вошёл в систему")
                  .arg(m_registeredNickname));

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

    if (password.trimmed().isEmpty()) {
        return tr("Введите пароль");
    }

    if (m_registrationInFlight) {
        return tr("Дождитесь завершения предыдущей регистрации");
    }

    const QString certificateFile = certificatePath.trimmed();
    if (certificateFile.isEmpty()) {
        return tr("Укажите путь к клиентскому сертификату");
    }

    if (findCredentialByNickname(trimmed)) {
        return tr("Пользователь с таким ником уже существует");
    }

    QFile certFile(certificateFile);
    if (!certFile.exists() || !certFile.open(QIODevice::ReadOnly)) {
        return tr("Не удалось прочитать сертификат");
    }
    const QByteArray certData = certFile.readAll();
    certFile.close();

    QList<QSslCertificate> parsed = QSslCertificate::fromData(certData, QSsl::Pem);
    if (parsed.isEmpty()) {
        parsed = QSslCertificate::fromData(certData, QSsl::Der);
    }
    if (parsed.isEmpty() || parsed.first().isNull()) {
        return tr("Файл не содержит валидный сертификат");
    }
    const QByteArray certDer = parsed.first().toDer();
    if (certDer.isEmpty()) {
        return tr("Не удалось преобразовать сертификат");
    }

    if (!m_networkManager) {
        return tr("Сетевой менеджер не инициализирован");
    }

    const QUrl url = buildApiUrl(QStringLiteral("/api/auth/register"));
    if (!url.isValid()) {
        return tr("Некорректный адрес API (%1)").arg(m_apiBaseUrl);
    }

    const QString certBase64 = QString::fromLatin1(certDer.toBase64());

    QJsonObject payload;
    payload.insert(QStringLiteral("nickname"), trimmed);
    payload.insert(QStringLiteral("certificate"), certBase64);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QEventLoop loop;
    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(payload).toJson());
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QScopedValueRollback<bool> inFlight(m_registrationInFlight, true);
    loop.exec();

    const QNetworkReply::NetworkError networkError = reply->error();
    const QByteArray responseBytes = reply->readAll();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorText = reply->errorString();
    reply->deleteLater();

    if (networkError != QNetworkReply::NoError) {
        return tr("Ошибка регистрации: %1").arg(errorText);
    }
    if (statusCode >= 400) {
        const QString serverMsg = QString::fromUtf8(responseBytes).trimmed();
        if (!serverMsg.isEmpty()) {
            return tr("Регистрация отклонена: %1").arg(serverMsg);
        }
        return tr("Регистрация отклонена (код %1)").arg(statusCode);
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(responseBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return tr("Сервер вернул некорректный ответ");
    }

    const QJsonObject obj = doc.object();
    const QString userId = obj.value(QStringLiteral("user_id")).toString().trimmed();
    QString serverNickname = obj.value(QStringLiteral("nickname"))
                                  .toString(trimmed)
                                  .trimmed();
    if (userId.isEmpty()) {
        return tr("Сервер не присвоил идентификатор пользователю");
    }
    if (serverNickname.isEmpty()) {
        serverNickname = trimmed;
    }

    Credential credential;
    credential.userId = userId;
    credential.nickname = serverNickname;
    credential.password = password.trimmed();
    credential.certificateDer = certBase64;
    m_credentials.append(credential);
    if (!persistCredentials()) {
        m_credentials.removeLast();
        return tr("Не удалось сохранить данные пользователя");
    }

    m_registeredUserId = userId;
    m_registeredNickname = serverNickname;
    m_isRegistered = true;
    persistRegistration(userId, serverNickname);

    appendLog(QStringLiteral("Registration -> зарегистрирован профиль %1 (%2)")
                  .arg(serverNickname, userId));

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
    m_lastServerMsgId.clear();
    m_nextMessageId = 1;

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

QVariantList AppController::buildConversationList() const
{
    QVariantList list;
    for (const QString &conversationId : m_conversationOrder) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), conversationId);
        entry.insert(QStringLiteral("title"), conversationDisplayName(conversationId));
        const QString subtitle = conversationSubtitle(conversationId);
        if (!subtitle.isEmpty()) {
            entry.insert(QStringLiteral("subtitle"), subtitle);
        }

        const QList<Message> &messages = m_conversations.value(conversationId);
        if (!messages.isEmpty()) {
            const Message &last = messages.constLast();
            entry.insert(QStringLiteral("lastMessage"), last.text);

            QString displayTime = last.timestamp;
            if (last.sentUnixSec > 0) {
                const QDateTime lastMoment = QDateTime::fromSecsSinceEpoch(last.sentUnixSec).toLocalTime();
                if (lastMoment.date() == QDate::currentDate()) {
                    displayTime = lastMoment.toString(QStringLiteral("HH:mm"));
                } else {
                    displayTime = QLocale().toString(lastMoment.date(), QLocale::ShortFormat);
                }
            }

            entry.insert(QStringLiteral("lastTimestamp"), displayTime);
        } else {
            entry.insert(QStringLiteral("lastMessage"), tr("Нет сообщений"));
            entry.insert(QStringLiteral("lastTimestamp"), QString());
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
        promoteConversation(QStringLiteral("corp-secure-room"));
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

        m_directory.append(m_authenticatedUser);
        m_directory.append(maria);
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
            message.sentUnixSec = sentUnix;
            message.timestamp = QDateTime::fromSecsSinceEpoch(sentUnix).toString(QStringLiteral("HH:mm:ss"));
        } else {
            message.sentUnixSec = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
            message.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
        }

        QList<Message> &conversation = m_conversations[conversationId];
        conversation.append(message);
        m_knownServerMsgIds.insert(serverMsgId);
        updateLastServerMsgId(serverMsgId);
    }

    rebuildConversationOrder();

    return true;
}

QString AppController::resolveDataDirectory() const
{
    const auto hasDataArtifacts = [](const QDir &dir) {
        return dir.exists(QStringLiteral("identity_store.json"))
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
            if (baseName == QStringLiteral("identity_store.json") || baseName == QStringLiteral("messages.db")) {
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
        return envMatch;
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
                return match;
            }
        }
    }

    const QStringList roots = {QCoreApplication::applicationDirPath(), QDir::currentPath()};
    for (const QString &root : roots) {
        const QString match = searchParents(root);
        if (!match.isEmpty()) {
            return match;
        }
    }

    if (const QString fallback = canonicalIfValid(QDir::currentPath()); !fallback.isEmpty()) {
        return fallback;
    }

    return QDir::currentPath();
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

void AppController::fetchUsersFromServer()
{
    if (!m_isRegistered) {
        return;
    }
    if (!m_networkManager) {
        appendLog(QStringLiteral("Directory.HTTP -> сетевой менеджер не инициализирован"));
        return;
    }

    const QUrl url = buildApiUrl(QStringLiteral("/api/auth/users"));
    if (!url.isValid()) {
        appendLog(QStringLiteral("Directory.HTTP -> некорректный адрес API (%1)").arg(m_apiBaseUrl));
        return;
    }

    appendLog(QStringLiteral("Directory.HTTP -> запрос каталога пользователей"));

    QNetworkRequest request(url);
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
    const QString trimmedId = conversationId.trimmed();
    if (trimmedId.isEmpty() || serverMsgId.trimmed().isEmpty()) {
        return;
    }

    Message message;
    message.serverMsgId = serverMsgId;
    message.author = author;
    message.text = text;
    message.outgoing = outgoing;
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

    promoteConversation(trimmedId);
    emit conversationListChanged();

    if (trimmedId == m_currentConversation) {
        emit conversationChanged();
    }
}

QString AppController::addMessage(const QString &conversationId, const QString &author, const QString &text, bool outgoing)
{
    const QString trimmedId = conversationId.trimmed();
    if (trimmedId.isEmpty()) {
        return {};
    }

    Message message;
    message.serverMsgId = QStringLiteral("local-%1").arg(m_nextMessageId++);
    message.author = author;
    message.text = text;
    message.outgoing = outgoing;
    message.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    message.sentUnixSec = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    QList<Message> &messages = m_conversations[trimmedId];
    messages.append(message);
    promoteConversation(trimmedId);
    emit conversationListChanged();
    if (trimmedId == m_currentConversation) {
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

void AppController::promoteConversation(const QString &conversationId)
{
    const QString trimmed = conversationId.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (!m_conversations.contains(trimmed)) {
        m_conversations.insert(trimmed, {});
    }
    rebuildConversationOrder();
}

void AppController::rebuildConversationOrder()
{
    QStringList keys = m_conversations.keys();
    auto scoreFor = [this](const QString &id) -> qint64 {
        const QList<Message> &messages = m_conversations.value(id);
        if (messages.isEmpty()) {
            return 0;
        }
        const Message &last = messages.constLast();
        if (last.sentUnixSec > 0) {
            return last.sentUnixSec;
        }
        return parseServerMsgNumeric(last.serverMsgId);
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

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

QString AppController::identityStoreFilePath() const
{
    return QDir(resolveDataDirectory()).filePath(QStringLiteral("identity_store.json"));
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
