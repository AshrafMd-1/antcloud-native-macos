#include "providerworker.h"
#include "tokenstore.h"
#include "streambridge.h"
#include <QUrlQuery>
#include <QThread>
#include <QDateTime>

namespace AntCloud {
namespace {
bool accessExpired(int status, const QJsonObject& value) {
    const QString message = value.value("message").toString();
    return status == 401 || message == "Token Expired" || message == "Session Expired";
}
bool credentialsRejected(int status, const QJsonObject& value) {
    const QString message = value.value("message").toString();
    return status == 401 || status == 403 || message == "Invalid Token" || message == "Token Expired"
        || message == "Session Expired" || message == "Re-authentication needed!"
        || message == "Forbidden Request!" || message == "Refresh Token Expired";
}
bool expiryPreflightNeeded(const QString& token) {
    const auto parts = token.toLatin1().split('.');
    if (parts.size() != 3) return false;
    const auto expiry = QJsonDocument::fromJson(QByteArray::fromBase64(parts[1], QByteArray::Base64UrlEncoding)).object().value("exp");
    // Decoded claims only select an authenticated read; they never establish login.
    return expiry.isDouble() && std::isfinite(expiry.toDouble()) && expiry.toDouble() > 0
        && std::floor(expiry.toDouble()) == expiry.toDouble() && expiry.toDouble() <= QDateTime::currentSecsSinceEpoch();
}
}
ProviderWorker::ProviderWorker(QObject* parent) : QObject(parent) {}

void ProviderWorker::initialize() {
    Q_ASSERT(QThread::currentThread() == thread());
    m_Network = new QNetworkAccessManager(this);
    m_Poll = new QTimer(this); m_Poll->setInterval(2000);
    m_Startup = new QTimer(this); m_Startup->setSingleShot(true); m_Startup->setInterval(StartupTimeoutMs);
    m_Heartbeat = new QTimer(this); m_Heartbeat->setSingleShot(true);
    m_AccountPoll = new QTimer(this); m_AccountPoll->setInterval(60000);
    connect(m_Poll, &QTimer::timeout, this, &ProviderWorker::poll);
    connect(m_Startup, &QTimer::timeout, this, [this] { finish("The VM did not connect within five minutes. Check the AntCloud dashboard before retrying."); });
    connect(m_Heartbeat, &QTimer::timeout, this, [this] { finish("The provider connection stopped responding. Your stream has been disconnected."); });
    connect(m_AccountPoll, &QTimer::timeout, this, &ProviderWorker::refreshRemainingMinutes);
}

void ProviderWorker::request(const QString& path, const QJsonObject& body, bool post, bool session, Reply callback, bool allowRenewal) {
    const int generation = session ? m_Generation : m_AuthGeneration;
    const int authGeneration = m_AuthGeneration;
    const qulonglong tokenRevision = m_TokenRevision;
#ifdef ANTCLOUD_TESTING
    const QUrl url(m_Api.toString() + path);
#else
    const QUrl url("https://api.antcloud.co" + path);
#endif
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(20000);
    request.setRawHeader("Accept", "application/json");
    if (path == "/api/users/refresh") request.setRawHeader("refresh", "JWT " + m_RefreshToken.toUtf8());
    else if (!m_Token.isEmpty()) request.setRawHeader("Authorization", "JWT " + m_Token.toUtf8());
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
    connect(reply, &QNetworkReply::finished, this, [this, reply, path, post, session, generation, authGeneration, tokenRevision, callback, allowRenewal] {
        m_Requests.remove(reply);
        reply->deleteLater();
        if (authGeneration != m_AuthGeneration || generation != (session ? m_Generation : m_AuthGeneration)) return;
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes = reply->read(1024 * 1024 + 1);
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(bytes, &error);
        QJsonObject value;
        if (bytes.size() <= 1024 * 1024 && error.error == QJsonParseError::NoError && doc.isObject()) value = doc.object();
        else status = status >= 400 ? status : 0;
        if (!post && allowRenewal && path != "/api/users/refresh" && accessExpired(status, value) && validToken(m_RefreshToken)) {
            if (tokenRevision != m_TokenRevision) {
                this->request(path, {}, false, session, callback, false); return;
            }
            renewCredentials([this, path, session, generation, authGeneration, callback](RenewalResult result) {
                if (authGeneration != m_AuthGeneration || generation != (session ? m_Generation : m_AuthGeneration)) return;
                if (result == RenewalResult::Success) this->request(path, {}, false, session, callback, false);
                else callback(result == RenewalResult::Rejected ? 401 : 0, {});
            });
            return;
        }
        callback(status, value);
    });
}

void ProviderWorker::renewCredentials(std::function<void(RenewalResult)> callback) {
    m_RenewalWaiters.append(std::move(callback));
    if (m_RenewalPending) return;
    m_RenewalPending = true;
    request("/api/users/refresh", {}, false, false, [this](int status, const QJsonObject& value) {
        m_RenewalPending = false;
        RenewalResult result = RenewalResult::TemporaryFailure;
        const QString access = value.value("accessToken").toString(), refresh = value.value("refreshToken").toString();
        if (status >= 200 && status < 300 && validToken(access) && validToken(refresh)) {
            m_Token = access; m_RefreshToken = refresh; ++m_TokenRevision;
            if (m_Remember) {
                QString error;
                if (!TokenStore::save({m_Token, m_RefreshToken}, error)) emit notice(error);
            }
            result = RenewalResult::Success;
        } else if (credentialsRejected(status, value)) result = RenewalResult::Rejected;
        const auto waiters = std::move(m_RenewalWaiters);
        m_RenewalWaiters.clear();
        for (const auto& waiter : waiters) waiter(result);
    }, false);
}

void ProviderWorker::restore() {
    if (m_Active) return;
    QString error;
    const auto credentials = TokenStore::load(error);
    if (credentials.accessToken.isEmpty()) { resetAccount(); emit account(false, 0, error); return; }
    emit stage("authenticating", "Checking your saved login…");
    authenticateToken(credentials.accessToken, credentials.refreshToken, true);
}

void ProviderWorker::authenticateToken(const QString& token, const QString& refreshToken, bool remember) {
    resetAccount(); m_Token = token; m_RefreshToken = refreshToken; m_Remember = remember;
    request("/api/users/me", {}, false, false, [this](int status, const QJsonObject& value) {
        if (credentialsRejected(status, value)) {
            QString error; TokenStore::clear(error); m_Token.clear(); m_RefreshToken.clear();
            emit account(false, 0, error);
            emit authenticationFailed("Your saved login was rejected. Please sign in again.");
        } else if (status != 200 || !accountLimit(value)) {
            m_Token.clear(); m_RefreshToken.clear(); emit account(false, 0, {});
            emit authenticationFailed("Could not validate the saved login. Check your connection and retry; the saved login was kept.");
        } else { acceptAccount(value, m_Token, m_Remember, m_RefreshToken); }
    });
}

void ProviderWorker::login(QString email, QString password, bool remember) {
    if (m_Active) return;
    if (email.trimmed().isEmpty() || email.size() > 320 || password.isEmpty() || password.size() > 4096) {
        emit authenticationFailed("Enter your AntCloud email and password."); return;
    }
    resetAccount();
    emit stage("authenticating", "Signing in to AntCloud…");
    request("/api/users/login", {{"email", email.trimmed()}, {"password", password}}, true, false,
        [this, remember](int status, const QJsonObject& value) {
        if (status < 200 || status >= 300) {
            emit authenticationFailed(status == 400 || status == 401 ? "Sign-in was rejected. Check your email and password." : "Could not sign in. Check your connection and try again.");
            return;
        }
        const QString token = value.value("token").toString();
        const auto user = value.value("user").toObject();
        const QString refresh = user.value("refreshToken").toString();
        if (!validToken(token) || !accountLimit(value)) {
            emit authenticationFailed("AntCloud returned unexpected account details. This does not mean the password was wrong."); return;
        }
        if (user.contains("refreshToken") && !validToken(refresh)) {
            emit authenticationFailed("AntCloud returned unexpected login renewal details. Please try signing in again."); return;
        }
        if (!remember) {
            QString error; TokenStore::clear(error);
            if (!error.isEmpty()) emit notice(error);
        }
        acceptAccount(value, token, remember, refresh);
    });
    password.fill(QChar('\0'));
}

void ProviderWorker::resetAccount() {
    ++m_AuthGeneration; clearRequests(false); m_Token.clear(); m_RefreshToken.clear(); m_Limit = 0;
    ++m_TokenRevision; m_Remember = false; m_RenewalPending = false; m_RenewalWaiters.clear();
    m_AccountPoll->stop(); m_AccountRefreshPending = false; m_AccountRefreshAgain = false;
    m_RemainingMinutes = -1; ++m_TimeRevision;
    StreamBridge::setRemainingMinutes(-1);
    emit remainingMinutesChanged(-1);
}

void ProviderWorker::refreshRemainingMinutes() {
    if (!validToken(m_Token)) return;
    if (m_AccountRefreshPending) { m_AccountRefreshAgain = true; return; }
    m_AccountRefreshPending = true;
    const int generation = m_Generation, revision = m_TimeRevision;
    request("/api/users/me", {}, false, false, [this, generation, revision](int status, const QJsonObject& value) {
        m_AccountRefreshPending = false;
        const bool refreshAgain = m_AccountRefreshAgain;
        m_AccountRefreshAgain = false;
        // Do not apply stale data, but honor a refresh requested at a boundary
        // during this read. Authentication guards run in request().
        if (generation == m_Generation && revision == m_TimeRevision && status == 200 && accountLimit(value)) {
            const int minutes = accountRemainingMinutes(value);
            if (minutes >= 0) updateRemainingMinutes(minutes);
        }
        if (refreshAgain) refreshRemainingMinutes();
    });
}

void ProviderWorker::updateRemainingMinutes(int minutes) {
    // Invalid stream data cannot erase either last validated balance.
    if (minutes < 0) return;
    StreamBridge::setRemainingMinutes(m_Active ? minutes : -1);
    m_RemainingMinutes = minutes; ++m_TimeRevision;
    emit remainingMinutesChanged(m_RemainingMinutes);
}

void ProviderWorker::acceptAccount(const QJsonObject& value, const QString& token, bool remember, const QString& refreshToken) {
    m_Token = token; m_RefreshToken = refreshToken; m_Remember = remember; m_Limit = accountLimit(value);
    updateRemainingMinutes(accountRemainingMinutes(value));
    QString message;
    if (remember) TokenStore::save({m_Token, m_RefreshToken}, message);
    emit account(true, m_Limit, message);
    m_AccountPoll->start();
    if (m_RemainingMinutes < 0) refreshRemainingMinutes();
}

void ProviderWorker::signOut() {
    stop(); resetAccount();
    QString error; TokenStore::clear(error); emit account(false, 0, error);
}

void ProviderWorker::start(int width, int height, int fps, int bitrate) {
    if (m_Active) return;
    m_Settings = {width, height, fps, bitrate};
    if (!validToken(m_Token) || !m_Settings.isValid() || m_Settings.preset() > m_Limit) {
        emit ended("Choose supported stream settings within your account’s resolution limit."); return;
    }
    m_Active = true; ++m_Generation; m_Ready = false; m_Shutdown = false;
    StreamBridge::setPairingGeneration(qulonglong(m_Generation));
    StreamBridge::setRemainingMinutes(-1);
    StreamBridge::setExpectedEnd(false);
    m_PinSubmitted = false;
    m_Polling = false; m_RequestPending = false; m_Namespace = false; m_Address.clear();
    // An old close cannot end this attempt while its login preflight is pending.
    if (m_Socket) { m_Socket->disconnect(this); delete m_Socket; m_Socket = nullptr; }
    m_Startup->start();
    if (expiryPreflightNeeded(m_Token)) {
        emit stage("connecting", "Checking your AntCloud login before connecting…");
        request("/api/users/me", {}, false, true, [this](int status, const QJsonObject& value) {
            if (status != 200 || !accountLimit(value)) {
                finish("Could not renew your AntCloud login before connecting. Retry, or sign in again if the saved login expired."); return;
            }
            m_Limit = accountLimit(value);
            if (m_Settings.preset() > m_Limit) {
                finish("Choose stream settings within your current account’s resolution limit."); return;
            }
            updateRemainingMinutes(accountRemainingMinutes(value));
            openProviderSocket();
        });
        return;
    }
    openProviderSocket();
}

void ProviderWorker::openProviderSocket() {
    if (!m_Active || m_Shutdown) return;
    emit stage("connecting", "Opening your provider session…");
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
    m_HeartbeatMs = 45000; m_Heartbeat->start(m_HeartbeatMs);
    refreshRemainingMinutes();
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
    } else if (frame.name == "control" && frame.value.isObject()) {
        const auto value = frame.value.toObject();
        if (value.contains("timeLeft")) updateRemainingMinutes(providerMinutes(value.value("timeLeft")));
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

void ProviderWorker::pair(QString pin, qulonglong generation) {
    static const QRegularExpression pattern(QRegularExpression::anchoredPattern("[0-9]{4}"));
    if (generation != qulonglong(m_Generation) || !m_Active || m_Shutdown || m_Address.isEmpty()
        || m_PinSubmitted || !pattern.match(pin).hasMatch()) return;
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
    StreamBridge::setPairingGeneration(0);
    StreamBridge::setRemainingMinutes(-1);
    StreamBridge::setHostRequestsAllowed(false);
    m_Poll->stop(); m_Startup->stop(); m_Heartbeat->stop();
    clearRequests(true); m_RequestPending = false; m_Polling = false; m_Namespace = false;
    if (m_Socket) m_Socket->close();
    StreamBridge::interrupt(); emit ended(StreamBridge::expectedEnd() ? QString() : message);
    refreshRemainingMinutes();
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
    StreamBridge::setPairingGeneration(0);
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
