#include "antcloud/providerworker.h"
#include "antcloud/streambridge.h"
#include <QtTest>
#include <QWebSocketServer>
#include <QTcpServer>
#include <QUrlQuery>
#include <atomic>

using namespace AntCloud;

class Fixture : public QObject {
    Q_OBJECT
public:
    QTcpServer* http = nullptr;
    QWebSocketServer* web = nullptr;
    QWebSocket* peer = nullptr;
    std::atomic<int> lookups{0}, checks{0}, pins{0}, controls{0}, shutdowns{0}, pongs{0};
    std::atomic<int> expectedBitrate{6000};
    std::atomic<bool> ready{false}, wrongAuth{false}, wrongQuery{false}, malformed{false};
    QString api, stream;
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
                        response = {{"token", "fixture.access.token"}, {"user", QJsonObject{{"currentPlan", "pro"}, {"resolution", "1080"}}}};
                    } else {
                        if (!header.contains("Authorization: JWT fixture.access.token")) wrongAuth = true;
                        if (path == "/api/vm/getVmIp") { ++lookups; response = {{"vmip", "192.0.2.10"}}; }
                        else if (path == "/api/vm/checkConnectionStatus") {
                            ++checks;
                            status = ready ? 200 : 400;
                            response = {{"connected", ready.load()}};
                            if (malformed) response = {{"connected", "true"}};
                        } else if (path == "/api/vm/vmauth") {
                            ++pins;
                            const auto body = QJsonDocument::fromJson(bytes.mid(split + 4, size)).object();
                            if (body.value("pin") != "1234") wrongAuth = true;
                            response = {{"message", "success"}, {"data", QJsonObject{{"status", true}}}};
                        } else if (path == "/api/users/me") response = {{"user", QJsonObject{{"currentPlan", "pro"}, {"resolution", "1080"}}}};
                        else status = 404;
                    }
                    const auto payload = QJsonDocument(response).toJson(QJsonDocument::Compact);
                    socket->write("HTTP/1.1 " + QByteArray::number(status) + " Test\r\nContent-Type: application/json\r\nContent-Length: " + QByteArray::number(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload);
                    socket->disconnectFromHost();
                });
            }
        });
        connect(web, &QWebSocketServer::newConnection, this, [this] {
            peer = web->nextPendingConnection();
            connect(peer, &QWebSocket::disconnected, peer, &QObject::deleteLater);
            auto query = QUrlQuery(peer->requestUrl());
            if (query.queryItemValue("token") != "fixture.access.token"
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
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection, Q_ARG(QString,"1234"));
        QTRY_COMPARE(fixture->pins.load(),1);
        QMetaObject::invokeMethod(worker, "shutdown", Qt::QueuedConnection);
        QVERIFY(sent.wait(1000));
        QTRY_COMPARE(fixture->shutdowns.load(),1);
        QMetaObject::invokeMethod(fixture, "inactive", Qt::QueuedConnection);
        QVERIFY(ended.wait(2000));
        QCOMPARE(ended.first()[0].toString(), QString()); // Expected shutdown is not an error.
        host.clear(); ended.clear(); sent.clear();
        start(); QVERIFY(host.wait(3000));
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection, Q_ARG(QString,"1234"));
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
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection, Q_ARG(QString,"1234"));
        QMetaObject::invokeMethod(worker, "pair", Qt::QueuedConnection, Q_ARG(QString,"1234"));
        QTRY_COMPARE_WITH_TIMEOUT(fixture->pins.load(), 1, 2000);
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
    void selectedBitrateReachesProvider() {
        fixture->expectedBitrate = 20000;
        QSignalSpy host(worker, &ProviderWorker::hostAvailable);
        QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection,
            Q_ARG(int,1280), Q_ARG(int,720), Q_ARG(int,60), Q_ARG(int,20000));
        QVERIFY(host.wait(3000));
        QVERIFY(!fixture->wrongQuery); // Inspect the actual received WebSocket request.
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
