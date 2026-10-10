#include "antcloud/providerworker.h"
#include "antcloud/streambridge.h"
#include "antcloud/tokenstore.h"
#include <QtTest>
#include <QWebSocketServer>
#include <QTcpServer>
#include <QUrlQuery>
#include <QPointer>
#include <atomic>

using namespace AntCloud;

static const QString ExpiredAccessToken = "eyJhbGciOiJIUzI1NiJ9.eyJleHAiOjEsImlhdCI6MH0.fixture";

class Fixture : public QObject {
    Q_OBJECT
public:
    QTcpServer* http = nullptr;
    QWebSocketServer* web = nullptr;
    QWebSocket* peer = nullptr;
    std::atomic<int> lookups{0}, checks{0}, pins{0}, controls{0}, shutdowns{0}, pongs{0};
    std::atomic<int> accountReads{0}, accountStatus{200}, accountDelayMs{0};
    std::atomic<int> refreshes{0}, refreshStatus{200}, refreshDelayMs{0}, expiredStatus{200}, pinStatus{200};
    std::atomic<bool> loginRefresh{false}, expiredAccess{false}, malformedRefresh{false}, loginExpiredJwt{false}, rejectRefreshBody{false};
    std::atomic<int> loginTotal{1800}, loginUsed{200}, accountTotal{1800}, accountUsed{220};
    std::atomic<int> expectedBitrate{6000};
    std::atomic<bool> ready{false}, wrongAuth{false}, wrongQuery{false}, malformed{false};
    QString api, stream;
    static QByteArray headerValue(const QByteArray& header, const QByteArray& name) {
        for (const auto& line : header.split('\n')) {
            const int separator = line.indexOf(':');
            if (separator > 0 && line.left(separator).trimmed().compare(name, Qt::CaseInsensitive) == 0)
                return line.mid(separator + 1).trimmed();
        }
        return {};
    }
public slots:
    void initialize() {
        http = new QTcpServer(this);
        QVERIFY(http->listen(QHostAddress::LocalHost));
        api = QString("http://127.0.0.1:%1").arg(http->serverPort());
        web = new QWebSocketServer("test", QWebSocketServer::NonSecureMode, this);
        QVERIFY(web->listen(QHostAddress::LocalHost));
        stream = QString("ws://127.0.0.1:%1/socket.io/").arg(web->serverPort());
        connect(http, &QTcpServer::newConnection, this, [this] {
            while (http->hasPendingConnections()) {
                auto socket = http->nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    auto bytes = socket->property("bytes").toByteArray() + socket->readAll();
                    socket->setProperty("bytes", bytes);
                    const int split = bytes.indexOf("\r\n\r\n");
                    if (split < 0) return;
                    const auto header = bytes.left(split);
                    QRegularExpression length("Content-Length: ([0-9]+)", QRegularExpression::CaseInsensitiveOption);
                    const auto match = length.match(QString::fromUtf8(header));
                    const int size = match.hasMatch() ? match.captured(1).toInt() : 0;
                    if (bytes.size() - split - 4 < size || socket->property("handled").toBool()) return;
                    socket->setProperty("handled", true);
                    const auto path = header.split(' ').value(1);
                    int status = 200;
                    QJsonObject response;
                    if (path == "/api/users/login") {
                        const auto body = QJsonDocument::fromJson(bytes.mid(split + 4, size)).object();
                        if (body.value("email") != "test@example.invalid" || body.value("password") != "fixture-password") wrongAuth = true;
                        response = {{"token", loginExpiredJwt ? ExpiredAccessToken : QString("fixture.access.token")}, {"user", QJsonObject{{"currentPlan", "pro"}, {"resolution", "1080"},
                            {"totalTimeMonth", loginTotal.load()}, {"timeUsedMonth", loginUsed.load()}}}};
                        if (loginRefresh) {
                            auto user = response.value("user").toObject();
                            user.insert("refreshToken", "fixture.refresh.token"); response.insert("user", user);
                        }
                    } else if (path == "/api/users/refresh") {
                        ++refreshes; status = refreshStatus.load();
                        if (!header.startsWith("GET ") || headerValue(header, "refresh") != "JWT fixture.refresh.token" || size)
                            wrongAuth = true;
                        response = status == 200
                            ? QJsonObject{{"message", "New Access Pairs Generated!"}, {"accessToken", "fixture.rotated.token"},
                                {"refreshToken", "fixture.rotated.refresh"}}
                            : QJsonObject{{"message", status == 401 ? "Refresh Token Expired" : "Temporary failure"}};
                        if (malformedRefresh) response.remove("refreshToken");
                        if (rejectRefreshBody) response = {{"message", "Refresh Token Expired"}};
                    } else {
                        const QByteArray authorization = headerValue(header, "Authorization");
                        const bool rotated = authorization == "JWT fixture.rotated.token";
                        if (!rotated && authorization != "JWT fixture.access.token"
                            && authorization != "JWT " + ExpiredAccessToken.toUtf8()) wrongAuth = true;
                        if (path == "/api/vm/getVmIp") { ++lookups; response = {{"vmip", "192.0.2.10"}}; }
                        else if (path == "/api/vm/checkConnectionStatus") {
                            ++checks;
                            status = ready ? 200 : 400;
                            response = {{"connected", ready.load()}};
                            if (malformed) response = {{"connected", "true"}};
                        } else if (path == "/api/vm/vmauth") {
                            ++pins;
                            status = pinStatus.load();
                            const auto body = QJsonDocument::fromJson(bytes.mid(split + 4, size)).object();
                            if (body.value("pin") != "1234") wrongAuth = true;
                            response = {{"message", "success"}, {"data", QJsonObject{{"status", true}}}};
                        } else if (path == "/api/users/me") {
                            ++accountReads; status = accountStatus.load();
                            response = {{"user", QJsonObject{{"currentPlan", "pro"}, {"resolution", "1080"},
                                {"totalTimeMonth", accountTotal.load()}, {"timeUsedMonth", accountUsed.load()}}}};
                            if (expiredAccess && !rotated) {
                                status = expiredStatus.load(); response = {{"message", "Token Expired"}};
                            }
                        }
                        else status = 404;
                    }
                    const auto payload = QJsonDocument(response).toJson(QJsonDocument::Compact);
                    const auto sendResponse = [socket, status, payload] {
                        socket->write("HTTP/1.1 " + QByteArray::number(status) + " Test\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload);
                        socket->disconnectFromHost();
                    };
                    if (path == "/api/users/me" && accountDelayMs > 0)
                        QTimer::singleShot(accountDelayMs.load(), socket, sendResponse);
                    else if (path == "/api/users/refresh" && refreshDelayMs > 0)
                        QTimer::singleShot(refreshDelayMs.load(), socket, sendResponse);
                    else sendResponse();
                });
            }
        });
        connect(web, &QWebSocketServer::newConnection, this, [this] {
            peer = web->nextPendingConnection();
            connect(peer, &QWebSocket::disconnected, peer, &QObject::deleteLater);
            auto query = QUrlQuery(peer->requestUrl());
            if ((query.queryItemValue("token") != "fixture.access.token" && query.queryItemValue("token") != "fixture.rotated.token")
                || query.queryItemValue("connection") != "mobile"
                || query.queryItemValue("stream") != QString("720 %1 60 10").arg(expectedBitrate.load())) wrongQuery = true;
            connect(peer, &QWebSocket::textMessageReceived, this, [this](const QString& packet) {
                if (packet == "40") {
                    peer->sendTextMessage("40{\"sid\":\"fixture\"}");
                    peer->sendTextMessage("42[\"status\",\"starting\"]");
                    peer->sendTextMessage("42[\"status\",\"done\"]");
                    peer->sendTextMessage("42[\"new_connection\",{\"clients\":2}]");
                } else if (packet == "3") ++pongs;
                else if (packet == "42[\"control\",\"video\"]") ++controls;
                else if (packet == "42[\"closestream\"]") ++shutdowns;
            });
            peer->sendTextMessage("0{\"pingInterval\":1000,\"pingTimeout\":2000}");
            auto timer = new QTimer(peer); timer->setInterval(100);
            connect(timer, &QTimer::timeout, peer, [this] {
                if (peer && peer->state() == QAbstractSocket::ConnectedState) peer->sendTextMessage("2");
            });
            timer->start();
        });
    }
    void inactive() { if (peer) peer->sendTextMessage("42[\"inactive\",\"videonotstarting\"]"); }
};

namespace AntCloud {
class ProviderWorkerTest : public QObject {
    Q_OBJECT
    QThread serverThread, workerThread;
    Fixture* fixture = nullptr;
    ProviderWorker* worker = nullptr;
private slots:
    void init() {
        QString storageError; QVERIFY(TokenStore::clear(storageError));
        fixture = new Fixture;
        fixture->moveToThread(&serverThread);
        connect(&serverThread, &QThread::finished, fixture, &QObject::deleteLater);
        serverThread.start();
        QMetaObject::invokeMethod(fixture, "initialize", Qt::BlockingQueuedConnection);
        worker = new ProviderWorker;
        worker->moveToThread(&workerThread);
        connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
        workerThread.start();
        QMetaObject::invokeMethod(worker, [this] {
            worker->initialize();
            worker->m_Api = QUrl(fixture->api); worker->m_Stream = QUrl(fixture->stream);
        }, Qt::BlockingQueuedConnection);
        QSignalSpy account(worker, &ProviderWorker::account);
        QMetaObject::invokeMethod(worker, "login", Qt::QueuedConnection,
            Q_ARG(QString, "test@example.invalid"), Q_ARG(QString, "fixture-password"), Q_ARG(bool, false));
        QVERIFY(account.wait(3000));
        QCOMPARE(account.first()[0].toBool(), true);
        QVERIFY(!fixture->wrongAuth);
    }
    void cleanup() {
        StreamBridge::install(nullptr); StreamBridge::setActive(false);
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        workerThread.quit(); QVERIFY(workerThread.wait(3000));
        serverThread.quit(); QVERIFY(serverThread.wait(3000));
    }
    void nativeStartsBeforeReadiness() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), connected(worker, &ProviderWorker::nativeReady);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QTRY_VERIFY_WITH_TIMEOUT(fixture->checks >= 1, 3000);
        QCOMPARE(connected.count(), 0); QCOMPARE(fixture->controls.load(), 0);
        QVERIFY(!fixture->wrongQuery);
        fixture->ready = true;
        QVERIFY(connected.wait(4000));
        QTRY_COMPARE_WITH_TIMEOUT(fixture->controls.load(), 1, 3000);
        QCOMPARE(fixture->lookups.load(), 2);
        QVERIFY(!fixture->wrongAuth);
    }
    void heartbeatSurvivesBlockedGuiAndShutdownOnce() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        StreamBridge::install(worker);
        const int before = fixture->pongs.load();
        QThread::msleep(650); // Main event loop is deliberately blocked like the SDL streaming loop.
        QVERIFY(fixture->pongs.load() >= before + 4);
        QVERIFY(StreamBridge::shutdown()); QVERIFY(StreamBridge::shutdown());
        QMetaObject::invokeMethod(worker, "stopAfterStream", Qt::QueuedConnection);
        QTRY_COMPARE_WITH_TIMEOUT(fixture->shutdowns.load(), 1, 2000);
    }
    void disconnectSendsNoShutdown() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(worker, "stop", Qt::QueuedConnection);
        QVERIFY(ended.wait(2000)); QCOMPARE(fixture->shutdowns.load(), 0);
    }
    void shutdownThenReconnectSameWorker() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        QSignalSpy sent(worker, &ProviderWorker::shutdownRequested);
        auto start = [this] {
            QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
                Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        };
        start(); QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection, Q_ARG(QString,"1234"), Q_ARG(qulonglong, StreamBridge::pairingGeneration()));
        QTRY_COMPARE(fixture->pins.load(),1);
        QMetaObject::invokeMethod(worker, "shutdown", Qt::QueuedConnection);
        QVERIFY(sent.wait(1000));
        QTRY_COMPARE(fixture->shutdowns.load(),1);
        QMetaObject::invokeMethod(fixture, "inactive", Qt::QueuedConnection);
        QVERIFY(ended.wait(2000));
        QCOMPARE(ended.first()[0].toString(), QString()); // Expected shutdown is not an error.
        host.clear(); ended.clear(); sent.clear();
        start(); QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection, Q_ARG(QString,"1234"), Q_ARG(qulonglong, StreamBridge::pairingGeneration()));
        QTRY_COMPARE(fixture->pins.load(),2); // Fresh session resets one-shot pairing.
        fixture->ready = true;
        QTRY_COMPARE_WITH_TIMEOUT(fixture->controls.load(),1,4000);
        QMetaObject::invokeMethod(worker, "shutdown", Qt::QueuedConnection);
        QVERIFY(sent.wait(1000));
        QTRY_COMPARE(fixture->shutdowns.load(),2); // Exactly once per session.
    }
    void shutdownCancelsReadinessPolling() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), sent(worker, &ProviderWorker::shutdownRequested);
        QSignalSpy ended(worker, &ProviderWorker::ended);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(worker, "shutdown", Qt::QueuedConnection);
        QVERIFY(sent.wait(1000));
        fixture->ready = true;
        QVERIFY(ended.wait(2000));
        QCOMPARE(ended.first()[0].toString(), QString());
        QCOMPARE(fixture->controls.load(), 0);
    }
    void failedShutdownWriteRemainsAnError() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        QSignalSpy sent(worker, &ProviderWorker::shutdownRequested);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(worker, [this] {
            worker->m_Socket->blockSignals(true);
            worker->m_Socket->abort(); // Fail the write, rather than deliver an expected close.
            worker->shutdown();
        }, Qt::BlockingQueuedConnection);
        QTRY_COMPARE(ended.count(), 1);
        QVERIFY(ended.first()[0].toString().contains("shutdown request could not be sent"));
        QCOMPARE(sent.count(), 0);
        QVERIFY(!StreamBridge::expectedEnd());
    }
    void pairingSubmittedOnlyOnce() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection, Q_ARG(QString,"1234"), Q_ARG(qulonglong, StreamBridge::pairingGeneration()));
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection, Q_ARG(QString,"1234"), Q_ARG(qulonglong, StreamBridge::pairingGeneration()));
        QTRY_COMPARE_WITH_TIMEOUT(fixture->pins.load(), 1, 2000);
    }
    void invalidPairingPinsDoNotConsumeOneShot() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        for (const auto& pin : {QString("1234\n"), QString("1234\r\n"), QString("１２３４"), QString("12345")})
            QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection,
                Q_ARG(QString,pin), Q_ARG(qulonglong,StreamBridge::pairingGeneration()));
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection,
            Q_ARG(QString,"1234"), Q_ARG(qulonglong,StreamBridge::pairingGeneration()));
        QTRY_COMPARE(fixture->pins.load(),1);
        QVERIFY(!fixture->wrongAuth);
    }
    void stalePairingGenerationCannotSubmitIntoReconnect() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable);
        auto start = [this] {
            QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
                Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        };
        start(); QVERIFY(host.wait(3000));
        const auto oldGeneration = StreamBridge::pairingGeneration();
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        QCOMPARE(StreamBridge::pairingGeneration(), qulonglong(0));
        host.clear(); start(); QVERIFY(host.wait(3000));
        QVERIFY(StreamBridge::pairingGeneration() != oldGeneration);
        QMetaObject::invokeMethod(worker, [this, oldGeneration] { worker->pair("1234", oldGeneration); }, Qt::BlockingQueuedConnection);
        QCOMPARE(fixture->pins.load(), 0);
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection,
            Q_ARG(QString,"1234"), Q_ARG(qulonglong, StreamBridge::pairingGeneration()));
        QTRY_COMPARE(fixture->pins.load(), 1);
    }
    void rejectedPairingMutationIsNeverRenewedOrRetried() {
        fixture->pinStatus = 401;
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        QMetaObject::invokeMethod(worker, [this] { worker->m_RefreshToken = "fixture.refresh.token"; }, Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection,
            Q_ARG(QString,"1234"), Q_ARG(qulonglong, StreamBridge::pairingGeneration()));
        QVERIFY(ended.wait(3000)); QCOMPARE(fixture->pins.load(), 1); QCOMPARE(fixture->refreshes.load(), 0);
    }
    void inactiveStopsPromptly() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(fixture, "inactive", Qt::QueuedConnection);
        QVERIFY(ended.wait(2000)); QVERIFY(ended.first()[0].toString().contains("video did not start"));
    }
    void localStopWinsQueuedProviderClosure() {
        StreamBridge::setExpectedEnd(false);
        StreamBridge::install(worker);
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        auto start = [this] {
            QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
                Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        };
        start(); QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(worker, [this] {
            StreamBridge::requestStop();
            // Deliver closure before the queued Stop slot executes.
            worker->receive("42[\"inactive\",\"videonotstarting\"]");
        }, Qt::BlockingQueuedConnection);
        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(ended.first()[0].toString(), QString());
        QCOMPARE(fixture->shutdowns.load(), 0);
        host.clear(); ended.clear();
        start(); QVERIFY(host.wait(3000));
        QVERIFY(!StreamBridge::expectedEnd());
        QMetaObject::invokeMethod(fixture, "inactive", Qt::QueuedConnection);
        QVERIFY(ended.wait(2000));
        QVERIFY(!ended.first()[0].toString().isEmpty()); // Genuine next-session error is retained.
    }
    void selectedBitrateReachesProvider_data() {
        QTest::addColumn<int>("bitrate");
        QTest::newRow("25 Mbps") << 25000;
        QTest::newRow("45 Mbps") << 45000;
        QTest::newRow("50 Mbps") << 50000;
    }
    void selectedBitrateReachesProvider() {
        QFETCH(int, bitrate);
        fixture->expectedBitrate = bitrate;
        QSignalSpy host(worker, &ProviderWorker::hostAvailable);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,bitrate));
        QVERIFY(host.wait(3000));
        QVERIFY(!fixture->wrongQuery); // Inspect the actual received WebSocket request.
    }
    void loginQuotaIsAccountDataOutsideStream() {
        QSignalSpy account(worker, &ProviderWorker::account), time(worker, &ProviderWorker::remainingMinutesChanged);
        QMetaObject::invokeMethod(worker, "login", Qt::QueuedConnection,
            Q_ARG(QString, "test@example.invalid"), Q_ARG(QString, "fixture-password"), Q_ARG(bool, false));
        QVERIFY(account.wait(3000));
        QCOMPARE(time.last()[0].toInt(), 1600);
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        QCOMPARE(fixture->accountReads.load(), 0);
    }
    void savedAuthenticationReadsQuota() {
        QSignalSpy account(worker, &ProviderWorker::account), time(worker, &ProviderWorker::remainingMinutesChanged);
        QMetaObject::invokeMethod(worker, [this] { worker->authenticateToken("fixture.access.token"); }, Qt::QueuedConnection);
        QVERIFY(account.wait(3000));
        QCOMPARE(account.last()[0].toBool(), true);
        QCOMPARE(time.last()[0].toInt(), 1580);
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        QCOMPARE(fixture->accountReads.load(), 1);
    }
    void storedCredentialFormatKeepsLegacyAccessAndAtomicPair() {
        QString error;
        const auto legacy = TokenStore::decode("fixture.access.token", error);
        QCOMPARE(legacy.accessToken, QString("fixture.access.token")); QVERIFY(legacy.refreshToken.isEmpty());
        const TokenStore::Credentials pair{"fixture.access.token", "fixture.refresh.token"};
        QVERIFY(TokenStore::save(pair, error)); QVERIFY(error.isEmpty());
        const auto stored = TokenStore::load(error);
        QCOMPARE(stored.accessToken, pair.accessToken); QCOMPARE(stored.refreshToken, pair.refreshToken);
        QVERIFY(TokenStore::decode("{\"version\":1,\"accessToken\":\"fixture.access.token\",\"refreshToken\":\"bad token\"}", error).accessToken.isEmpty());
        QVERIFY(!error.isEmpty());
    }
    void expiredSavedAccessIsRenewedAndRotationRemembered_data() {
        QTest::addColumn<int>("status");
        QTest::newRow("expired JSON with 200") << 200;
        QTest::newRow("expired HTTP401") << 401;
    }
    void expiredSavedAccessIsRenewedAndRotationRemembered() {
        QFETCH(int, status);
        fixture->expiredAccess = true; fixture->expiredStatus = status;
        QString error; QVERIFY(TokenStore::save({"fixture.access.token", "fixture.refresh.token"}, error));
        QSignalSpy account(worker, &ProviderWorker::account), failed(worker, &ProviderWorker::authenticationFailed);
        QMetaObject::invokeMethod(worker, "restore", Qt::QueuedConnection);
        QVERIFY(account.wait(3000)); QCOMPARE(account.last()[0].toBool(), true); QCOMPARE(failed.count(), 0);
        QCOMPARE(fixture->refreshes.load(), 1); QCOMPARE(fixture->accountReads.load(), 2); QVERIFY(!fixture->wrongAuth);
        const auto stored = TokenStore::load(error);
        QCOMPARE(stored.accessToken, QString("fixture.rotated.token")); QCOMPARE(stored.refreshToken, QString("fixture.rotated.refresh"));
    }
    void temporaryRenewalFailurePreservesRememberedLogin() {
        fixture->expiredAccess = true; fixture->refreshStatus = 503;
        QString error; QVERIFY(TokenStore::save({"fixture.access.token", "fixture.refresh.token"}, error));
        QSignalSpy failed(worker, &ProviderWorker::authenticationFailed), account(worker, &ProviderWorker::account);
        QMetaObject::invokeMethod(worker, "restore", Qt::QueuedConnection);
        QVERIFY(failed.wait(3000)); QCOMPARE(fixture->refreshes.load(), 1);
        QCOMPARE(TokenStore::load(error).refreshToken, QString("fixture.refresh.token"));
        fixture->refreshStatus = 200; account.clear();
        QMetaObject::invokeMethod(worker, "restore", Qt::QueuedConnection);
        QVERIFY(account.wait(3000)); QCOMPARE(account.last()[0].toBool(), true); QCOMPARE(fixture->refreshes.load(), 2);
    }
    void rejectedRenewalClearsRememberedLogin_data() {
        QTest::addColumn<int>("status");
        QTest::newRow("401 rejection") << 401;
        QTest::newRow("200 rejection body") << 200;
    }
    void rejectedRenewalClearsRememberedLogin() {
        QFETCH(int, status);
        fixture->expiredAccess = true; fixture->refreshStatus = status; fixture->rejectRefreshBody = true;
        QString error; QVERIFY(TokenStore::save({"fixture.access.token", "fixture.refresh.token"}, error));
        QSignalSpy failed(worker, &ProviderWorker::authenticationFailed);
        QMetaObject::invokeMethod(worker, "restore", Qt::QueuedConnection);
        QVERIFY(failed.wait(3000)); QVERIFY(TokenStore::load(error).accessToken.isEmpty());
        QCOMPARE(fixture->refreshes.load(), 1);
    }
    void loginRefreshRotatesDuringStreamWithoutSavingOptOut() {
        fixture->loginRefresh = true;
        QSignalSpy account(worker, &ProviderWorker::account);
        QMetaObject::invokeMethod(worker, "login", Qt::QueuedConnection,
            Q_ARG(QString,"test@example.invalid"), Q_ARG(QString,"fixture-password"), Q_ARG(bool,false));
        QVERIFY(account.wait(3000));
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000)); QTRY_COMPARE(fixture->accountReads.load(), 1);
        fixture->expiredAccess = true;
        QMetaObject::invokeMethod(worker, [this] { worker->refreshRemainingMinutes(); }, Qt::QueuedConnection);
        QTRY_COMPARE(fixture->refreshes.load(), 1); QTRY_COMPARE(fixture->accountReads.load(), 3);
        QMetaObject::invokeMethod(worker, [this] {
            QCOMPARE(worker->m_Token, QString("fixture.rotated.token"));
            QCOMPARE(worker->m_RefreshToken, QString("fixture.rotated.refresh")); QVERIFY(worker->m_Active);
        }, Qt::BlockingQueuedConnection);
        QCOMPARE(ended.count(), 0); QString error; QVERIFY(TokenStore::load(error).accessToken.isEmpty());
    }
    void concurrentExpiredReadsShareOneRenewal() {
        fixture->expiredAccess = true; fixture->accountDelayMs = 80; fixture->refreshDelayMs = 100;
        std::atomic<int> completed{0};
        QMetaObject::invokeMethod(worker, [this, &completed] {
            worker->m_RefreshToken = "fixture.refresh.token";
            for (int i = 0; i < 2; ++i) worker->request("/api/users/me", {}, false, false,
                [&completed](int status, const QJsonObject& value) { if (status == 200 && accountLimit(value)) ++completed; });
        }, Qt::QueuedConnection);
        QTRY_COMPARE_WITH_TIMEOUT(completed.load(), 2, 3000);
        QCOMPARE(fixture->refreshes.load(), 1); QCOMPARE(fixture->accountReads.load(), 4); QVERIFY(!fixture->wrongAuth);
    }
    void signOutInvalidatesDelayedRenewal() {
        fixture->expiredAccess = true; fixture->refreshDelayMs = 300;
        QString error; QVERIFY(TokenStore::save({"fixture.access.token", "fixture.refresh.token"}, error));
        QSignalSpy account(worker, &ProviderWorker::account);
        QMetaObject::invokeMethod(worker, "restore", Qt::QueuedConnection);
        QTRY_COMPARE(fixture->refreshes.load(), 1);
        QMetaObject::invokeMethod(worker, "signOut", Qt::BlockingQueuedConnection);
        fixture->expiredAccess = false; fixture->loginRefresh = true;
        QMetaObject::invokeMethod(worker, "login", Qt::QueuedConnection,
            Q_ARG(QString,"test@example.invalid"), Q_ARG(QString,"fixture-password"), Q_ARG(bool,false));
        QTRY_VERIFY(!account.isEmpty() && account.last()[0].toBool()); QTest::qWait(400);
        QMetaObject::invokeMethod(worker, [this] {
            QCOMPARE(worker->m_Token, QString("fixture.access.token"));
            QCOMPARE(worker->m_RefreshToken, QString("fixture.refresh.token"));
        }, Qt::BlockingQueuedConnection);
        QVERIFY(TokenStore::load(error).accessToken.isEmpty());
    }
    void malformedRenewalPreservesSavedPair() {
        fixture->expiredAccess = true; fixture->malformedRefresh = true;
        QString error; QVERIFY(TokenStore::save({"fixture.access.token", "fixture.refresh.token"}, error));
        QSignalSpy failed(worker, &ProviderWorker::authenticationFailed);
        QMetaObject::invokeMethod(worker, "restore", Qt::QueuedConnection);
        QVERIFY(failed.wait(3000)); QCOMPARE(fixture->refreshes.load(), 1);
        QCOMPARE(TokenStore::load(error).accessToken, QString("fixture.access.token"));
        QCOMPARE(TokenStore::load(error).refreshToken, QString("fixture.refresh.token"));
    }
    void expiredAccessPreflightRenewsBeforeOpeningSocket() {
        fixture->loginRefresh = true; fixture->loginExpiredJwt = true; fixture->expiredAccess = true;
        QSignalSpy account(worker, &ProviderWorker::account), host(worker, &ProviderWorker::hostAvailable);
        QMetaObject::invokeMethod(worker, "login", Qt::QueuedConnection,
            Q_ARG(QString,"test@example.invalid"), Q_ARG(QString,"fixture-password"), Q_ARG(bool,false));
        QVERIFY(account.wait(3000));
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000)); QCOMPARE(fixture->refreshes.load(), 1); QVERIFY(!fixture->wrongQuery);
        QMetaObject::invokeMethod(fixture, [this] {
            QCOMPARE(QUrlQuery(fixture->peer->requestUrl()).queryItemValue("token"), QString("fixture.rotated.token"));
        }, Qt::BlockingQueuedConnection);
    }
    void stopCancelsDelayedAuthenticationPreflight() {
        fixture->loginRefresh = true; fixture->loginExpiredJwt = true; fixture->expiredAccess = true; fixture->refreshDelayMs = 300;
        QSignalSpy account(worker, &ProviderWorker::account), host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        QMetaObject::invokeMethod(worker, "login", Qt::QueuedConnection,
            Q_ARG(QString,"test@example.invalid"), Q_ARG(QString,"fixture-password"), Q_ARG(bool,false));
        QVERIFY(account.wait(3000));
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QTRY_COMPARE(fixture->refreshes.load(), 1);
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection); QTest::qWait(400);
        QCOMPARE(host.count(), 0); QCOMPARE(fixture->lookups.load(), 0); QCOMPARE(ended.count(), 1);
    }
    void previousSocketCannotEndDeferredReconnect() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000)); host.clear();
        fixture->expiredAccess = true; fixture->refreshDelayMs = 150;
        QMetaObject::invokeMethod(worker, [this] {
            QPointer<QWebSocket> previous = worker->m_Socket;
            worker->stop(); worker->m_Token = ExpiredAccessToken; worker->m_RefreshToken = "fixture.refresh.token";
            worker->start(1280, 720, 60, 6000);
            QVERIFY(previous.isNull()); QVERIFY(worker->m_Active);
        }, Qt::BlockingQueuedConnection);
        QVERIFY(host.wait(3000)); QCOMPARE(ended.count(), 1); QVERIFY(!fixture->wrongQuery);
    }
    void missingLoginQuotaRecoversWithoutBlockingAuthentication() {
        fixture->loginTotal = 0; fixture->accountDelayMs = 100;
        QSignalSpy account(worker, &ProviderWorker::account), time(worker, &ProviderWorker::remainingMinutesChanged);
        QMetaObject::invokeMethod(worker, "login", Qt::QueuedConnection,
            Q_ARG(QString, "test@example.invalid"), Q_ARG(QString, "fixture-password"), Q_ARG(bool, false));
        QVERIFY(account.wait(3000));
        QCOMPARE(account.last()[0].toBool(), true);
        QTRY_VERIFY_WITH_TIMEOUT(!time.isEmpty() && time.last()[0].toInt() == 1580, 3000);
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        QCOMPARE(fixture->accountReads.load(), 1);
    }
    void stopPreservesQuotaAndRefreshesAccount() {
        fixture->accountDelayMs = 100;
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        QSignalSpy time(worker, &ProviderWorker::remainingMinutesChanged);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QTRY_VERIFY_WITH_TIMEOUT(!time.isEmpty() && time.last()[0].toInt() == 1580, 3000);
        QCOMPARE(StreamBridge::remainingMinutes(), 1580); // Account refresh fills a missing stream report.
        fixture->accountUsed = 300;
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        QCOMPARE(time.last()[0].toInt(), 1580);
        QTRY_COMPARE_WITH_TIMEOUT(time.last()[0].toInt(), 1500, 3000);
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        QCOMPARE(fixture->accountReads.load(), 2);
        for (const auto& report : time) QVERIFY(report[0].toInt() >= 0);
        QCOMPARE(ended.count(), 1);
    }
    void failedAccountRefreshPreservesBalanceAndSession_data() {
        QTest::addColumn<int>("status"); QTest::addColumn<int>("total");
        QTest::newRow("temporary failure") << 503 << 1800;
        QTest::newRow("rejected background read") << 401 << 1800;
        QTest::newRow("missing quota") << 200 << 0;
    }
    void failedAccountRefreshPreservesBalanceAndSession() {
        QFETCH(int, status); QFETCH(int, total);
        fixture->accountStatus = status; fixture->accountTotal = total;
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), ended(worker, &ProviderWorker::ended);
        QSignalSpy time(worker, &ProviderWorker::remainingMinutesChanged), account(worker, &ProviderWorker::account);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QTRY_COMPARE_WITH_TIMEOUT(fixture->accountReads.load(), 1, 3000);
        QCOMPARE(ended.count(), 0); QCOMPARE(account.count(), 0);
        QMetaObject::invokeMethod(worker, [this] {
            QVERIFY(worker->m_Active); QCOMPARE(worker->m_Token, QString("fixture.access.token"));
            worker->receive(eventPacket("control", QJsonObject{{"timeLeft", 1400}}));
        }, Qt::BlockingQueuedConnection);
        QTRY_COMPARE(time.last()[0].toInt(), 1400);
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        QTRY_COMPARE_WITH_TIMEOUT(fixture->accountReads.load(), 2, 3000);
        QCOMPARE(time.last()[0].toInt(), 1400);
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        QCOMPARE(account.count(), 0);
    }
    void delayedAccountReadCannotReplaceNewerStreamQuota() {
        fixture->accountDelayMs = 300;
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), time(worker, &ProviderWorker::remainingMinutesChanged);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QTRY_COMPARE_WITH_TIMEOUT(fixture->accountReads.load(), 1, 3000);
        QMetaObject::invokeMethod(worker, [this] {
            worker->receive(eventPacket("control", QJsonObject{{"timeLeft", 1400}}));
        }, Qt::BlockingQueuedConnection);
        QTRY_COMPARE(time.last()[0].toInt(), 1400);
        QTest::qWait(400);
        QCOMPARE(time.last()[0].toInt(), 1400);
        QCOMPARE(StreamBridge::remainingMinutes(), 1400);
    }
    void delayedAccountReadIsDiscardedAcrossSignOutAndLogin() {
        fixture->accountDelayMs = 300;
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), time(worker, &ProviderWorker::remainingMinutesChanged);
        QSignalSpy account(worker, &ProviderWorker::account);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QTRY_COMPARE_WITH_TIMEOUT(fixture->accountReads.load(), 1, 3000);
        QMetaObject::invokeMethod(worker, "signOut", Qt::BlockingQueuedConnection);
        QTRY_VERIFY(!account.isEmpty() && !account.last()[0].toBool());
        QTRY_COMPARE(time.last()[0].toInt(), -1);
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        fixture->loginUsed = 900;
        QMetaObject::invokeMethod(worker, "login", Qt::QueuedConnection,
            Q_ARG(QString, "test@example.invalid"), Q_ARG(QString, "fixture-password"), Q_ARG(bool, false));
        QTRY_VERIFY_WITH_TIMEOUT(account.last()[0].toBool(), 3000);
        QTRY_COMPARE(time.last()[0].toInt(), 900);
        QTest::qWait(400); QCOMPARE(time.last()[0].toInt(), 900);
        // A new account can still refresh after the prior request was invalidated.
        host.clear(); fixture->accountUsed = 950;
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QTRY_COMPARE_WITH_TIMEOUT(time.last()[0].toInt(), 850, 3000);
        QCOMPARE(StreamBridge::remainingMinutes(), 850);
    }
    void overlappingSessionRefreshesHaveOneReadInFlight() {
        fixture->accountDelayMs = 300;
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), time(worker, &ProviderWorker::remainingMinutesChanged);
        auto start = [this] {
            QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
                Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        };
        start(); QVERIFY(host.wait(3000));
        QTRY_COMPARE_WITH_TIMEOUT(fixture->accountReads.load(), 1, 3000);
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        fixture->accountUsed = 400;
        host.clear(); start(); QVERIFY(host.wait(3000));
        QCOMPARE(fixture->accountReads.load(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(!time.isEmpty() && time.last()[0].toInt() == 1400, 3000);
        QCOMPARE(fixture->accountReads.load(), 2);
        QCOMPARE(StreamBridge::remainingMinutes(), 1400);
    }
    void boundaryRefreshSurvivesNewerStreamReport_data() {
        QTest::addColumn<bool>("reconnect");
        QTest::newRow("stop") << false;
        QTest::newRow("reconnect") << true;
    }
    void boundaryRefreshSurvivesNewerStreamReport() {
        QFETCH(bool, reconnect);
        fixture->accountDelayMs = 300;
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), time(worker, &ProviderWorker::remainingMinutesChanged);
        auto start = [this] {
            QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
                Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        };
        start(); QVERIFY(host.wait(3000));
        QTRY_COMPARE_WITH_TIMEOUT(fixture->accountReads.load(), 1, 3000);
        QMetaObject::invokeMethod(worker, [this] {
            worker->receive(eventPacket("control", QJsonObject{{"timeLeft", 1400}}));
        }, Qt::BlockingQueuedConnection);
        QTRY_COMPARE(time.last()[0].toInt(), 1400);
        fixture->accountUsed = 550;
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        if (reconnect) { host.clear(); start(); QVERIFY(host.wait(3000)); }
        QCOMPARE(fixture->accountReads.load(), 1); // The boundary does not overlap the pending read.
        QTRY_COMPARE_WITH_TIMEOUT(fixture->accountReads.load(), 2, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(time.last()[0].toInt(), 1250, 3000);
        QCOMPARE(StreamBridge::remainingMinutes(), reconnect ? 1250 : -1);
        for (const auto& report : time) QVERIFY(report[0].toInt() != 1580); // Discard the older read.
    }
    void invalidTimeReportsPreserveCurrentBalance() {
        fixture->accountStatus = 503;
        QSignalSpy host(worker, &ProviderWorker::hostAvailable), time(worker, &ProviderWorker::remainingMinutesChanged);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(worker, [this] {
            worker->receive(eventPacket("control", QJsonObject{{"timeLeft", 1400}}));
        }, Qt::BlockingQueuedConnection);
        QTRY_COMPARE(time.last()[0].toInt(), 1400);
        const int reports = time.count();
        const QJsonValue invalid[] = {QJsonValue(), "1400", -1, 0.5, true, 2147483648.0, QJsonObject{}};
        for (const auto& value : invalid) {
            QMetaObject::invokeMethod(worker, [this, value] {
                worker->receive(eventPacket("control", QJsonObject{{"timeLeft", value}}));
            }, Qt::BlockingQueuedConnection);
            QCOMPARE(StreamBridge::remainingMinutes(), 1400);
            QCOMPARE(time.last()[0].toInt(), 1400);
            QCOMPARE(time.count(), reports);
        }
    }
    void idleAccountTimerRefreshesWithoutStartingSession() {
        fixture->accountUsed = 500; fixture->accountDelayMs = 80;
        QSignalSpy time(worker, &ProviderWorker::remainingMinutesChanged), ended(worker, &ProviderWorker::ended);
        QMetaObject::invokeMethod(worker, [this] {
            QCOMPARE(worker->m_AccountPoll->interval(), 60000);
            QVERIFY(worker->m_AccountPoll->isActive());
            worker->m_AccountPoll->setInterval(20);
        }, Qt::BlockingQueuedConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!time.isEmpty() && time.last()[0].toInt() == 1300, 3000);
        QMetaObject::invokeMethod(worker, [this] { worker->m_AccountPoll->stop(); }, Qt::BlockingQueuedConnection);
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        QCOMPARE(fixture->lookups.load(), 0); QCOMPARE(ended.count(), 0);
    }
    void reportedTimeIsValidatedAndResetAcrossSessions() {
        QSignalSpy host(worker, &ProviderWorker::hostAvailable);
        QSignalSpy time(worker, &ProviderWorker::remainingMinutesChanged);
        QSignalSpy ended(worker, &ProviderWorker::ended);
        auto start = [this] {
            QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
                Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        };
        fixture->accountStatus = 503; // No account refresh should obscure stream validation.
        start(); QVERIFY(host.wait(3000));
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        auto report = [this](QJsonValue minutes) {
            QMetaObject::invokeMethod(fixture, [this, minutes] {
                fixture->peer->sendTextMessage(eventPacket("control", QJsonObject{{"timeLeft", minutes}}));
            }, Qt::QueuedConnection);
        };
        report(750); QTRY_COMPARE(StreamBridge::remainingMinutes(), 750);
        QCOMPARE(time.last()[0].toInt(), 750);
        QMetaObject::invokeMethod(worker, [this] {
            worker->receive(eventPacket("control", QJsonObject{{"timeLeft", "750"}}));
        }, Qt::BlockingQueuedConnection);
        QCOMPARE(StreamBridge::remainingMinutes(), 750);
        QCOMPARE(time.last()[0].toInt(), 750); // Malformed stream quota does not erase either balance.
        report(0); QTRY_COMPARE(StreamBridge::remainingMinutes(), 0);
        QCOMPARE(ended.count(), 0); // Reporting quota does not guess a shutdown.
        QMetaObject::invokeMethod(worker, "stop", Qt::BlockingQueuedConnection);
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        host.clear(); start(); QVERIFY(host.wait(3000));
        QCOMPARE(StreamBridge::remainingMinutes(), -1);
        report(500); QTRY_COMPARE(StreamBridge::remainingMinutes(), 500);
    }
    void malformedReadinessNeverStartsVideo() {
        fixture->ready = true; fixture->malformed = true;
        QSignalSpy ended(worker, &ProviderWorker::ended);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(ended.wait(3000)); QCOMPARE(fixture->controls.load(), 0);
    }
    void accountLimitRejectsBeforeSocket() {
        QSignalSpy ended(worker, &ProviderWorker::ended);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,3840), Q_ARG(int,2160), Q_ARG(int,60), Q_ARG(int,6000));
        QVERIFY(ended.wait(2000)); QCOMPARE(fixture->lookups.load(), 0);
    }
};
}
QTEST_GUILESS_MAIN(AntCloud::ProviderWorkerTest)
#include "provider_test.moc"
