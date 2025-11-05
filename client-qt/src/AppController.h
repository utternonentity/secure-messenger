#pragma once

#include <QHash>
#include <QJsonDocument>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;
class QTimer;

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap authInfo READ authInfo NOTIFY authInfoChanged)
    Q_PROPERTY(QVariantList userList READ userList NOTIFY userListChanged)
    Q_PROPERTY(QVariantList conversation READ conversation NOTIFY conversationChanged)
    Q_PROPERTY(QVariantList conversationList READ conversationList NOTIFY conversationListChanged)
    Q_PROPERTY(QStringList serverLog READ serverLog NOTIFY serverLogChanged)
    Q_PROPERTY(QString currentConversation READ currentConversation WRITE setCurrentConversation NOTIFY currentConversationChanged)
    Q_PROPERTY(bool registered READ isRegistered NOTIFY registrationChanged)
    Q_PROPERTY(QString nickname READ nickname NOTIFY registrationChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    QVariantMap authInfo() const;
    QVariantList userList() const;
    QVariantList conversation() const;
    QVariantList conversationList() const;
    QStringList serverLog() const;
    QString currentConversation() const;
    void setCurrentConversation(const QString &conversationId);

    bool isRegistered() const;
    QString nickname() const;

    Q_INVOKABLE void send(const QString &text);
    Q_INVOKABLE void startConversationWith(const QString &userId);
    Q_INVOKABLE void rotateDevice(const QString &userId, const QString &deviceId);
    Q_INVOKABLE void revokeDevice(const QString &userId, const QString &deviceId);
    Q_INVOKABLE void refreshUsers();
    Q_INVOKABLE void simulatePull();
    Q_INVOKABLE QString authenticate(const QString &nickname,
                                     const QString &password,
                                     const QString &certificatePath);
    Q_INVOKABLE QString completeRegistration(const QString &nickname,
                                             const QString &password,
                                             const QString &certificatePath);
    Q_INVOKABLE void resetRegistration();

signals:
    void authInfoChanged();
    void userListChanged();
    void conversationChanged();
    void conversationListChanged();
    void serverLogChanged();
    void currentConversationChanged();
    void registrationChanged();

private:
    struct Device {
        QString deviceId;
        QString certificate;
        bool revoked = false;
    };

    struct User {
        QString userId;
        QString nickname;
        QList<Device> devices;
    };

    struct Message {
        QString serverMsgId;
        QString author;
        QString text;
        QString timestamp;
        bool outgoing = false;
        qint64 sentUnixSec = 0;
    };

    QVariantMap buildAuthInfo() const;
    QVariantList buildUserList() const;
    QVariantList buildConversation() const;
    QVariantList buildConversationList() const;

    void loadServerData();
    void initializeAfterRegistration();
    void applyRegisteredIdentity();
    void loadRegistration();
    void persistRegistration(const QString &userId, const QString &nickname);
    bool loadUserDirectory(const QString &path);
    bool applyDirectoryFromJson(const QJsonDocument &doc);
    bool loadMessageHistory(const QString &path);
    void fetchHistoryFromServer(const QString &sinceServerMsgId = QString());
    void fetchUsersFromServer();
    int handleMessagesResponse(const QJsonDocument &doc);
    void postMessageToServer(const QString &conversationId, const QString &text);
    void updateLastServerMsgId(const QString &serverMsgId);
    qint64 parseServerMsgNumeric(const QString &serverMsgId) const;
    QUrl buildApiUrl(const QString &path, const QUrlQuery &query = {}) const;
    void addServerMessage(const QString &conversationId,
                          const QString &serverMsgId,
                          const QString &author,
                          const QString &text,
                          bool outgoing,
                          qint64 sentUnixSec);

    QString resolveDataDirectory() const;
    QString nicknameForUserId(const QString &userId) const;

    QString addMessage(const QString &conversationId, const QString &author, const QString &text, bool outgoing);
    void appendLog(const QString &entry);
    void ensureDirectoryContainsAuthUser();
    User *findUser(const QString &userId);
    Device *findDevice(const QString &userId, const QString &deviceId);
    void promoteConversation(const QString &conversationId);
    void rebuildConversationOrder();
    QString conversationDisplayName(const QString &conversationId) const;
    QString conversationSubtitle(const QString &conversationId) const;
    bool isConversationVisible(const QString &conversationId) const;
    void loadCredentials();
    bool persistCredentials() const;
    QString identityStoreFilePath() const;
    QString generateUserIdForNickname(const QString &nickname) const;
    bool userIdExists(const QString &userId) const;

    struct Credential {
        QString userId;
        QString nickname;
        QString password;
        QString certificateDer;
    };

    Credential *findCredentialByNickname(const QString &nickname);
    const Credential *findCredentialByNickname(const QString &nickname) const;
    const Credential *findCredentialByUserId(const QString &userId) const;

    bool m_isRegistered = false;
    QString m_registeredNickname;
    QString m_registeredUserId;
    bool m_initialized = false;
    User m_authenticatedUser;
    QList<User> m_directory;
    QHash<QString, QList<Message>> m_conversations;
    QStringList m_conversationOrder;
    QStringList m_serverLog;
    QString m_currentConversation;
    QStringList m_authenticatedRoles;
    qint64 m_nextMessageId = 1;
    QString m_lastServerMsgId;
    QSet<QString> m_knownServerMsgIds;
    QNetworkAccessManager *m_networkManager = nullptr;
    QTimer *m_pollTimer = nullptr;
    QString m_apiBaseUrl;
    bool m_registrationInFlight = false;
    QList<Credential> m_credentials;
};
