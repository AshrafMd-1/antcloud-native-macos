#include "providerworker.h"
#include "tokenstore.h"
#include "streambridge.h"
#include <QUrlQuery>
#include <QThread>

namespace AntCloud {
ProviderWorker::ProviderWorker(QObject* parent) : QObject(parent) {}

void ProviderWorker::initialize() {
    Q_ASSERT(QThread::currentThread() == thread());
    m_Network = new QNetworkAccessManager(this);
    m_Poll = new QTimer(this); m_Poll->setInterval(2000);
    m_Startup = new QTimer(this); m_Startup->setSingleShot(true); m_Startup->setInterval(StartupTimeoutMs);
    m_Heartbeat = new QTimer(this); m_Heartbeat->setSingleShot(true);
    connect(m_Poll, &QTimer::timeout, this, &ProviderWorker::poll);
    connect(m_Startup, &QTimer::timeout, this, [this] { finish("The VM did not connect within five minutes. Check the AntCloud dashboard before retrying."); });
    connect(m_Heartbeat, &QTimer::timeout, this, [this] { finish("The provider connection stopped responding. Your stream has been disconnected."); });
}

void ProviderWorker::request(const QString& path, const QJsonObject& body, bool post, bool session, Reply callback) {
    const int generation = session ? m_Generation : m_AuthGeneration;
#ifdef ANTCLOUD_TESTING
    const QUrl url(m_Api.toString() + path);
#else
    const QUrl url("https://api.antcloud.co" + path);
#endif
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(20000);
    request.setRawHeader("Accept", "application/json");
    if (!m_Token.isEmpty()) request.setRawHeader("Authorization", "JWT " + m_Token.toUtf8());
    QByteArray payload;
    if (post) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    }
    auto reply = post ? m_Network->post(request, payload) : m_Network->get(request);
    payload.fill('\0'); // Best effort; Qt owns its request buffer until completion.
    reply->setProperty("session", session);
    reply->setReadBufferSize(1024 * 1024 + 1);
    m_Requests.insert(reply);
    connect(reply, &QNetworkReply::readyRead, this, [reply] {
        if (reply->bytesAvailable() > 1024 * 1024) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, session, generation, callback] {
        m_Requests.remove(reply);
        reply->deleteLater();
        if (generation != (session ? m_Generation : m_AuthGeneration)) return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes = reply->read(1024 * 1024 + 1);
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(bytes, &error);
        if (bytes.size() > 1024 * 1024 || error.error != QJsonParseError::NoError || !doc.isObject()) {
            callback(status >= 400 ? status : 0, {}); return;
        }
        callback(status, doc.object());
    });
}

void ProviderWorker::restore() {
    QString error;
    const QString token = TokenStore::load(error);
    if (token.isEmpty()) { emit account(false, 0, error); return; }
    emit stage("authenticating", "Checking your saved login…");
    authenticateToken(token);
}

void ProviderWorker::authenticateToken(const QString& token) {
    ++m_AuthGeneration; clearRequests(false); m_Token = token;
    request("/api/users/me", {}, false, false, [this, token](int status, const QJsonObject& value) {
        if (status == 401) {
            QString error; TokenStore::clear(error); m_Token.clear();
            emit account(false, 0, error);
            emit authenticationFailed("Your saved login was rejected. Please sign in again.");
        } else if (status != 200 || !accountLimit(value)) {
            m_Token.clear(); emit account(false, 0, {});
            emit authenticationFailed("Could not validate the saved login. Check your connection and sign in; the saved token was kept.");
        } else { acceptAccount(value, token, false); }
    });
}

void ProviderWorker::login(QString email, QString password, bool remember) {
    if (m_Active) return;
    if (email.trimmed().isEmpty() || email.size() > 320 || password.isEmpty() || password.size() > 4096) {
        emit authenticationFailed("Enter your AntCloud email and password."); return;
    }
    ++m_AuthGeneration; clearRequests(false); m_Token.clear();
    emit stage("authenticating", "Signing in to AntCloud…");
    request("/api/users/login", {{"email", email.trimmed()}, {"password", password}}, true, false,
        [this, remember](int status, const QJsonObject& value) {
        if (status < 200 || status >= 300) {
            emit authenticationFailed(status == 400 || status == 401 ? "Sign-in was rejected. Check your email and password." : "Could not sign in. Check your connection and try again.");
            return;
        }
        const QString token = value.value("token").toString();
        if (!validToken(token) || !accountLimit(value)) {
            emit authenticationFailed("AntCloud returned unexpected account details. This does not mean the password was wrong."); return;
        }
        if (!remember) {
            QString error; TokenStore::clear(error);
            if (!error.isEmpty()) emit notice(error);
        }
        acceptAccount(value, token, remember);
    });
    password.fill(QChar('\0'));
}

void ProviderWorker::acceptAccount(const QJsonObject& value, const QString& token, bool remember) {
    m_Token = token; m_Limit = accountLimit(value);
    QString message;
    if (remember) TokenStore::save(token, message);
    emit account(true, m_Limit, message);
}

void ProviderWorker::signOut() {
    stop(); ++m_AuthGeneration; clearRequests(false); m_Token.clear(); m_Limit = 0;
    QString error; TokenStore::clear(error); emit account(false, 0, error);
}

void ProviderWorker::start(int width, int height, int fps, int bitrate) {
    if (m_Active) return;
    m_Settings = {width, height, fps, bitrate};
    if (!validToken(m_Token) || !m_Settings.isValid() || m_Settings.preset() > m_Limit) {
        emit ended("Choose supported stream settings within your account’s resolution limit."); return;
    }
    m_Active = true; ++m_Generation; m_Ready = false; m_Shutdown = false;
    StreamBridge::setExpectedEnd(false);
    m_PinSubmitted = false;
    m_Polling = false; m_RequestPending = false; m_Namespace = false; m_Address.clear();
    emit stage("connecting", "Opening your provider session…");
    // Recreate the socket so a previous connection cannot deliver events into a new session.
    if (m_Socket) { m_Socket->disconnect(this); delete m_Socket; }
    m_Socket = new QWebSocket(QStringLiteral("https://antcloud.co"), QWebSocketProtocol::VersionLatest, this);
    m_Socket->setMaxAllowedIncomingMessageSize(65536);
    m_Socket->setMaxAllowedIncomingFrameSize(65536);
    m_Socket->setReadBufferSize(65536);
    connect(m_Socket, &QWebSocket::textMessageReceived, this, &ProviderWorker::receive);
    connect(m_Socket, &QWebSocket::disconnected, this, [this] { if (m_Active) finish(m_Shutdown ? QString() : QString("The provider session ended. Check the dashboard before reconnecting.")); });
    #if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(m_Socket, &QWebSocket::errorOccurred, this,
#else
    connect(m_Socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this,
#endif
        [this](QAbstractSocket::SocketError) { if (m_Active) finish(m_Shutdown ? QString() : QString("Could not keep the provider connection open. Check your connection and the dashboard.")); });
#ifdef ANTCLOUD_TESTING
    QUrl url(m_Stream);
#else
    QUrl url("wss://socket.antcloud.co:8000/socket.io/");
#endif
    QUrlQuery query;
    query.addQueryItem("EIO", "4"); query.addQueryItem("transport", "websocket");
    query.addQueryItem("type", "client"); query.addQueryItem("game", "desktop");
    query.addQueryItem("token", m_Token); query.addQueryItem("connection", "mobile");
    query.addQueryItem("stream", m_Settings.providerSpec()); url.setQuery(query);
    m_Socket->open(url);
    m_HeartbeatMs = 45000; m_Heartbeat->start(m_HeartbeatMs); m_Startup->start();
}

bool ProviderWorker::send(const QString& packet) {
    return m_Active && m_Socket && m_Socket->state() == QAbstractSocket::ConnectedState
        && m_Socket->sendTextMessage(packet) == packet.toUtf8().size();
}

void ProviderWorker::receive(const QString& packet) {
    if (!m_Active) return;
    const Frame frame = parseFrame(packet);
    switch (frame.kind) {
    case Frame::Hello:
        m_HeartbeatMs = frame.heartbeatMs;
        m_Heartbeat->start(m_HeartbeatMs);
        if (!send("40")) finish("Could not join the provider session.");
        return;
    case Frame::Ping:
        m_Heartbeat->start(m_HeartbeatMs);
        if (!send("3")) finish("The provider heartbeat could not be sent.");
        return;
    case Frame::NamespaceReady:
        m_Namespace = true; emit stage("booting", "Waiting for the VM to boot. This can take a minute or two."); return;
    case Frame::Closed:
        if (m_Shutdown) { finish({}); return; }
        [[fallthrough]];
    case Frame::NamespaceError:
        finish("The provider ended or rejected this session. Check the dashboard."); return;
    case Frame::Invalid:
        finish("The provider sent an unsupported message. Export diagnostics before retrying."); return;
    case Frame::Ignored: return;
    case Frame::Event: break;
    }
    if (!m_Namespace) return;
    if (!frame.ackId.isEmpty()) send("43" + frame.ackId + "[]");
    if (frame.name == "inactive" || frame.name == "reset" || frame.name == "gamequit") {
        if (m_Shutdown) { finish({}); return; }
        finish(frame.value == "videonotstarting" ? "The provider ended the session because video did not start. Check the dashboard, then retry." : "The provider ended the session. Check the dashboard for the VM state.");
    } else if (frame.name == "status") {
        const QString status = frame.value.isString() ? frame.value.toString() : frame.value.toObject().value("status").toString();
        if (status == "starting" || status == "waiting") emit stage("booting", "Waiting for the VM to boot. This can take a minute or two.");
        else if (status == "done") emit stage("host", "The VM has booted. Waiting for its native streaming host…");
    } else if (frame.name == "new_connection") {
        const auto clients = frame.value.toObject().value("clients");
        if (clients.isDouble() && clients.toDouble() >= 2 && !m_Polling) {
            m_Polling = true; m_Poll->start(); poll();
        }
    }
}

void ProviderWorker::poll() {
    if (!m_Active || !m_Namespace || m_Ready || m_RequestPending) return;
    m_RequestPending = true;
    if (m_Address.isEmpty()) {
        request("/api/vm/getVmIp", {}, false, true, [this](int status, const QJsonObject& value) {
            m_RequestPending = false;
            if (status == 200) {
                m_Address = vmAddress(value);
                if (m_Address.isEmpty()) { finish("VM lookup returned an unsupported address."); return; }
                emit stage("host", "Native host found. Connecting this Mac…");
                emit hostAvailable(m_Address);
                checkConnection();
            } else if (status != 0 && status != 400 && status != 408 && status != 429 && status < 500) {
                finish("AntCloud rejected the VM lookup. Check your account and dashboard.");
            }
        });
    } else { checkConnection(); }
}

void ProviderWorker::checkConnection() {
    if (!m_Active || m_Ready) return;
    m_RequestPending = true;
    request("/api/vm/checkConnectionStatus", {}, false, true, [this](int status, const QJsonObject& value) {
        m_RequestPending = false;
        if (status == 200 && connected(value)) {
            m_RequestPending = true;
            request("/api/vm/getVmIp", {}, false, true, [this](int status, const QJsonObject& value) {
                m_RequestPending = false;
                if (status != 200 || vmAddress(value) != m_Address) {
                    finish("The VM address could not be revalidated. Check the dashboard before restarting the stream."); return;
                }
                if (!send(eventPacket("control", "video"))) { finish("Could not send the provider’s video-start control."); return; }
                m_Ready = true; m_Poll->stop(); m_Startup->stop(); emit nativeReady();
            });
        } else if (status == 200 && !value.value("connected").isBool()) {
            finish("The native readiness response had an unexpected format.");
        } else if (status != 0 && status != 400 && status != 408 && status != 429 && status < 500) {
            finish("AntCloud rejected native readiness. Check your account and dashboard.");
        }
        // HTTP 400 is pending, never success. The native client is already launching.
    });
}

void ProviderWorker::pair(QString pin) {
    static const QRegularExpression pattern("^[0-9]{4}$");
    if (!m_Active || m_Address.isEmpty() || m_PinSubmitted || !pattern.match(pin).hasMatch()) return;
    m_PinSubmitted = true;
    request("/api/vm/vmauth", {{"pin", pin}}, true, true, [this](int status, const QJsonObject& value) {
        if (status != 200 || !pinAccepted(value)) finish("The pairing PIN was not confirmed. Check the dashboard before retrying; it will not be submitted again automatically.");
    });
}

void ProviderWorker::clearRequests(bool sessionOnly) {
    const auto requests = m_Requests;
    for (auto reply : requests) {
        if (!sessionOnly || reply->property("session").toBool()) reply->abort();
    }
}

void ProviderWorker::finish(const QString& message) {
    if (!m_Active) return;
    m_Active = false; ++m_Generation;
    StreamBridge::setHostRequestsAllowed(false);
    m_Poll->stop(); m_Startup->stop(); m_Heartbeat->stop();
    clearRequests(true); m_RequestPending = false; m_Polling = false; m_Namespace = false;
    if (m_Socket) m_Socket->close();
    StreamBridge::interrupt(); emit ended(StreamBridge::expectedEnd() ? QString() : message);
}

void ProviderWorker::stop() { finish({}); }
void ProviderWorker::stopAfterStream() { if (!m_Shutdown) stop(); }

void ProviderWorker::shutdown() {
    if (m_Shutdown) return;
    if (!m_Active || !m_Namespace) {
        emit notice("Shutdown could not be sent because the provider connection is not active. Check the dashboard.");
        return;
    }
    m_Shutdown = true;
    // Cancel pending readiness replies so they cannot launch video after shutdown.
    ++m_Generation; m_Poll->stop(); m_Startup->stop();
    clearRequests(true); m_RequestPending = false;
    // Send once. Socket write acceptance is not a confirmed VM power state.
    if (!send(eventPacket("closestream"))) {
        finish("The shutdown request could not be sent. Check the dashboard; it was not retried."); return;
    }
    StreamBridge::setExpectedEnd(true);
    emit shutdownRequested();
    const int generation = m_Generation;
    QTimer::singleShot(1000, this, [this, generation] { if (generation == m_Generation) finish({}); });
}
}
