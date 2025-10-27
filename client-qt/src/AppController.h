#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap authInfo READ authInfo NOTIFY authInfoChanged)
    Q_PROPERTY(QVariantList userList READ userList NOTIFY userListChanged)
    Q_PROPERTY(QVariantList conversation READ conversation NOTIFY conversationChanged)
    Q_PROPERTY(QStringList serverLog READ serverLog NOTIFY serverLogChanged)
    Q_PROPERTY(QString currentConversation READ currentConversation WRITE setCurrentConversation NOTIFY currentConversationChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    QVariantMap authInfo() const;
    QVariantList userList() const;
    QVariantList conversation() const;
    QStringList serverLog() const;
    QString currentConversation() const;
    void setCurrentConversation(const QString &conversationId);

    Q_INVOKABLE void send(const QString &text);
    Q_INVOKABLE void startConversationWith(const QString &userId);
    Q_INVOKABLE void rotateDevice(const QString &userId, const QString &deviceId);
    Q_INVOKABLE void revokeDevice(const QString &userId, const QString &deviceId);
    Q_INVOKABLE void refreshUsers();
    Q_INVOKABLE void simulatePull();

signals:
    void authInfoChanged();
    void userListChanged();
    void conversationChanged();
    void serverLogChanged();
    void currentConversationChanged();

private:
    struct Device {
        QString deviceId;
        QString certificate;
        bool revoked = false;
    };

    struct User {
        QString userId;
        QString displayName;
        QList<Device> devices;
    };

    struct Message {
        QString serverMsgId;
        QString author;
        QString text;
        QString timestamp;
        bool outgoing = false;
    };

    QVariantMap buildAuthInfo() const;
    QVariantList buildUserList() const;
    QVariantList buildConversation() const;

    QString addMessage(const QString &conversationId, const QString &author, const QString &text, bool outgoing);
    void appendLog(const QString &entry);
    void ensureDirectoryContainsAuthUser();
    User *findUser(const QString &userId);
    Device *findDevice(const QString &userId, const QString &deviceId);

    User m_authenticatedUser;
    QList<User> m_directory;
    QHash<QString, QList<Message>> m_conversations;
    QStringList m_serverLog;
    QString m_currentConversation;
    qint64 m_nextMessageId = 1;
};
