#pragma once

#include <QDateTime>
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
class QNetworkRequest;
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
    Q_PROPERTY(bool authBusy READ isAuthBusy NOTIFY authBusyChanged)

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
    bool isAuthBusy() const;

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
    void authBusyChanged();

private:
    struct AuthSession {
        QString userId;
        QString nickname;
        QStringList roles;
        QString token;
        QDateTime expiresAtUtc;
        QString certificateBase64;
    };

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
        QString senderUserId;
        QString author;
        QString text;
        QString timestamp;
        bool outgoing = false;
        bool delivered = false;
        bool readByPeer = false;
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
    void loadReadMarkers();
    void persistReadMarkers() const;
    void syncReadMarkerWithServer(const QString &conversationId, qint64 lastServerMsgId);
    qint64 parseServerMsgNumeric(const QString &serverMsgId) const;
    qint64 latestServerMessageId(const QString &conversationId) const;
    QUrl buildApiUrl(const QString &path, const QUrlQuery &query = {}) const;
    void addServerMessage(const QString &conversationId,
                          const QString &serverMsgId,
                          const QString &senderUserId,
                          const QString &author,
                          const QString &text,
                          bool outgoing,
                          qint64 sentUnixSec);
    void applyReadMarkersFromServer(const QJsonObject &root);
    void updatePeerReadState(const QString &conversationId);
    QString conversationPeerId(const QString &conversationId) const;

    QString resolveDataDirectory() const;
    QString nicknameForUserId(const QString &userId) const;

    QString addMessage(const QString &conversationId, const QString &author, const QString &text, bool outgoing);
    void appendLog(const QString &entry);
    void ensureDirectoryContainsAuthUser();
    User *findUser(const QString &userId);
    Device *findDevice(const QString &userId, const QString &deviceId);
    void promoteConversation(const QString &conversationId, qint64 activityHint = 0);
    void rebuildConversationOrder();
    void touchConversationActivity(const QString &conversationId, qint64 unixTimestamp);
    qint64 lastActivityForConversation(const QString &conversationId) const;
    void markConversationRead(const QString &conversationId);
    int unreadCountFor(const QString &conversationId) const;
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

    Credential* findCredentialByUserId(const QString& userId);
    const Credential *findCredentialByUserId(const QString &userId) const;

    void setAuthBusy(bool busy);
    QString resolveCertificatePath(const QString &path) const;
    bool loadCertificateFromFile(const QString &path, QByteArray &der, QString &error) const;
    QString sendAuthRequest(const QString &path,
                            const QString &operation,
                            const QJsonObject &payload,
                            QJsonObject &response,
                            bool markBusy);
    bool parseAuthSession(const QJsonObject &obj, AuthSession &session, QString &error) const;
    QString storeCredential(const QString &userId,
                            const QString &nickname,
                            const QString &password,
                            const QString &certificateBase64,
                            bool persistFile);
    bool applySessionState(const AuthSession &session);
    bool ensureSessionToken(bool logErrors);
    bool hasValidSessionToken() const;
    void applyAuthHeaders(QNetworkRequest &request) const;
    void persistMessageHistory();

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
    QHash<QString, qint64> m_conversationActivity;
    QHash<QString, qint64> m_conversationReadMarkers;
    QHash<QString, qint64> m_peerReadMarkers;
    QNetworkAccessManager *m_networkManager = nullptr;
    QTimer *m_pollTimer = nullptr;
    QString m_apiBaseUrl;
    bool m_registrationInFlight = false;
    bool m_authBusy = false;
    QString m_accessToken;
    QDateTime m_tokenExpiry;
    QList<Credential> m_credentials;
};
