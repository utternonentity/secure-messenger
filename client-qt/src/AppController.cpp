#include "AppController.h"

#include <QDateTime>

namespace {
QString demoCertificate(const QString &deviceId, const QString &label)
{
    return QStringLiteral("MIIF-%1-%2-BASE64==").arg(deviceId, label);
}
}

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_authenticatedUser.userId = QStringLiteral("user-0001");
    m_authenticatedUser.displayName = QStringLiteral("Иван Петров");
    m_authenticatedUser.devices.append({QStringLiteral("device-01"), demoCertificate(QStringLiteral("device-01"), QStringLiteral("primary")), false});

    User maria;
    maria.userId = QStringLiteral("user-0002");
    maria.displayName = QStringLiteral("Мария Сидорова");
    maria.devices.append({QStringLiteral("device-A1"), demoCertificate(QStringLiteral("device-A1"), QStringLiteral("laptop")), false});
    maria.devices.append({QStringLiteral("device-A2"), demoCertificate(QStringLiteral("device-A2"), QStringLiteral("mobile")), false});

    User oleg;
    oleg.userId = QStringLiteral("user-0003");
    oleg.displayName = QStringLiteral("Олег Ким");
    oleg.devices.append({QStringLiteral("device-B1"), demoCertificate(QStringLiteral("device-B1"), QStringLiteral("desktop")), false});

    m_directory.append(m_authenticatedUser);
    m_directory.append(maria);
    m_directory.append(oleg);
    ensureDirectoryContainsAuthUser();

    m_currentConversation = QStringLiteral("corp-secure-room");
    m_conversations.insert(m_currentConversation, {});

    appendLog(QStringLiteral("Auth.WhoAmI -> %1 (%2)")
                  .arg(m_authenticatedUser.userId, m_authenticatedUser.displayName));
    appendLog(QStringLiteral("Directory.ListUsers -> %1 профиля")
                  .arg(m_directory.size()));
    appendLog(QStringLiteral("Messaging.Pull -> подписка на %1")
                  .arg(m_currentConversation));

    addMessage(m_currentConversation,
               QStringLiteral("Мария Сидорова"),
               QStringLiteral("Привет! Сервер подтвердил наш общий ключ."),
               false);
    addMessage(m_currentConversation,
               QStringLiteral("Сервер"),
               QStringLiteral("msg-0001 доставлено подписчикам (%1)").arg(m_currentConversation),
               false);
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

void AppController::send(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    if (!m_conversations.contains(m_currentConversation)) {
        m_conversations.insert(m_currentConversation, {});
    }

    const QString serverMsgId = addMessage(m_currentConversation, m_authenticatedUser.displayName, trimmed, true);
    appendLog(QStringLiteral("Messaging.Send -> сохранено %1 (conv=%2)")
                  .arg(serverMsgId, m_currentConversation));
    appendLog(QStringLiteral("Messaging.broadcast -> доставлено %1 подписчикам")
                  .arg(serverMsgId));

    addMessage(m_currentConversation,
               QStringLiteral("Сервер"),
               QStringLiteral("Доставка %1 подтверждена").arg(serverMsgId),
               false);
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
        const QString partnerName = user ? user->displayName : trimmed;
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

    device->certificate = demoCertificate(deviceId, QStringLiteral("rotated-%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("hhmmss"))));
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
    appendLog(QStringLiteral("Directory.ListUsers -> %1 профиля")
                  .arg(m_directory.size()));
    emit userListChanged();
}

void AppController::simulatePull()
{
    const QString incomingId = addMessage(m_currentConversation,
                                          QStringLiteral("Мария Сидорова"),
                                          QStringLiteral("Новое сообщение из %1").arg(m_currentConversation),
                                          false);
    appendLog(QStringLiteral("Messaging.Pull -> получено %1 из %2")
                  .arg(incomingId, m_currentConversation));
}

QVariantMap AppController::buildAuthInfo() const
{
    QVariantMap map;
    map.insert(QStringLiteral("userId"), m_authenticatedUser.userId);
    map.insert(QStringLiteral("displayName"), m_authenticatedUser.displayName);
    if (!m_authenticatedUser.devices.isEmpty()) {
        const Device &device = m_authenticatedUser.devices.first();
        map.insert(QStringLiteral("deviceId"), device.deviceId);
        map.insert(QStringLiteral("certificate"), device.certificate);
    }
    QStringList roles;
    roles << QStringLiteral("admin") << QStringLiteral("user");
    map.insert(QStringLiteral("roles"), roles);
    return map;
}

QVariantList AppController::buildUserList() const
{
    QVariantList list;
    for (const User &user : m_directory) {
        QVariantMap entry;
        entry.insert(QStringLiteral("userId"), user.userId);
        entry.insert(QStringLiteral("displayName"), user.displayName);
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

QString AppController::addMessage(const QString &conversationId, const QString &author, const QString &text, bool outgoing)
{
    Message message;
    message.serverMsgId = QStringLiteral("msg-%1").arg(m_nextMessageId++, 4, 10, QChar('0'));
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
