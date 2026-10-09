// All provider network objects and timers live on this worker's own QThread.
#pragma once
#include "protocol.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QTimer>
#include <QWebSocket>
#include <functional>

namespace AntCloud {
class ProviderWorker : public QObject {
    Q_OBJECT
public:
    explicit ProviderWorker(QObject* parent = nullptr);
public slots:
    void initialize();
    void restore();
    void login(QString email, QString password, bool remember);
    void signOut();
    void start(int width, int height, int fps, int bitrate);
    void pair(QString pin);
    void stop();
    void stopAfterStream();
    void shutdown();
signals:
    void account(bool signedIn, int limit, QString notice);
    void authenticationFailed(QString message);
    void stage(QString name, QString detail);
    void hostAvailable(QString address);
    void nativeReady();
    void ended(QString message);
    void notice(QString message);
    void shutdownRequested();
    void remainingMinutesChanged(int minutes);
private:
    using Reply = std::function<void(int, QJsonObject)>;
    void request(const QString& path, const QJsonObject& body, bool post, bool session, Reply callback);
    void authenticateToken(const QString& token);
    void acceptAccount(const QJsonObject& value, const QString& token, bool remember);
    void receive(const QString& packet);
    void updateRemainingMinutes(int minutes);
    void poll();
    void checkConnection();
    void finish(const QString& message);
    bool send(const QString& packet);
    void clearRequests(bool sessionOnly);
    QNetworkAccessManager* m_Network = nullptr;
    QWebSocket* m_Socket = nullptr;
    QTimer *m_Poll = nullptr, *m_Startup = nullptr, *m_Heartbeat = nullptr;
    QSet<QNetworkReply*> m_Requests;
    QString m_Token, m_Address;
    int m_Limit = 0, m_Generation = 0, m_AuthGeneration = 0, m_HeartbeatMs = 45000;
    bool m_Active = false, m_Namespace = false, m_Polling = false, m_RequestPending = false;
    bool m_Ready = false, m_Shutdown = false, m_PinSubmitted = false;
    StreamSettings m_Settings;
#ifdef ANTCLOUD_TESTING
    friend class ProviderWorkerTest;
    QUrl m_Api = QUrl("https://api.antcloud.co");
    QUrl m_Stream = QUrl("wss://socket.antcloud.co:8000/socket.io/");
#endif
};
}
