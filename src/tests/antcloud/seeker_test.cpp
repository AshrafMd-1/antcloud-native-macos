#include "backend/computerseeker.h"
#include "backend/computermanager.h"
#include "backend/identitymanager.h"
#include "backend/nvhttp.h"
#include "backend/nvcomputer.h"
#include "antcloud/streambridge.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QSslCertificate>
#include <QTimer>

class PairingDispatchProbe : public QObject {
    Q_OBJECT
public:
    bool* challengeReady = nullptr;
    int submissions = 0;
    bool submittedBeforeChallenge = false;
public slots:
    void pair(QString, qulonglong) {
        ++submissions;
        submittedBeforeChallenge |= !*challengeReady;
    }
};

// Real host HTTP parsing, polling and discovery; only unrelated engine services
// (preferences/compatibility/downloads) are isolated in host_test_services.cpp.
class SeekerTest : public QObject {
    Q_OBJECT
    QTemporaryDir storage;
    QTcpServer server;
    QString address;
    int requests = 0;
    bool responding = true;
    int pairingRequests = 0;
    bool pairingChallengeReady = false;
    int pairingResponseDelayMs = 1400;
    int currentGame = 0;
    bool delayNextPoll = false;
    QPointer<QTcpSocket> delayedPoll;
    static bool sendResponse(QTcpSocket* socket, const QByteArray& xml) {
        const QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\nContent-Length: "
            + QByteArray::number(xml.size()) + "\r\nConnection: close\r\n\r\n" + xml;
        if (socket->write(response) != response.size()) return false;
        while (socket->bytesToWrite() > 0 && socket->waitForBytesWritten(1000)) {}
        if (socket->bytesToWrite() > 0) return false;
        socket->disconnectFromHost();
        return true;
    }
private slots:
    void initTestCase() {
        QVERIFY(storage.isValid());
        QCoreApplication::setOrganizationName("AntCloud isolated host fixture");
        QCoreApplication::setApplicationName("Host discovery tests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, storage.path());
        qRegisterMetaType<NvComputer*>();
        IdentityManager::get();
        QVERIFY(server.listen(QHostAddress::LocalHost));
        address = QString("127.0.0.1:%1").arg(server.serverPort());
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (server.hasPendingConnections()) {
                auto socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    const auto bytes = socket->property("bytes").toByteArray() + socket->readAll();
                    socket->setProperty("bytes", bytes);
                    if (!bytes.contains("\r\n\r\n") || socket->property("handled").toBool()) return;
                    socket->setProperty("handled", true); ++requests;
                    if (bytes.startsWith("GET /pair?")) {
                        ++pairingRequests;
                        // Receiving bytes does not mean the host has installed its
                        // pending challenge yet. The provider cannot accept a PIN before it.
                        QTimer::singleShot(250, socket, [this] { pairingChallengeReady = true; });
                        QTimer::singleShot(pairingResponseDelayMs, socket, [socket] {
                            const QByteArray xml = "<root status_code=\"200\"><paired>0</paired></root>";
                            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\nContent-Length: "
                                + QByteArray::number(xml.size()) + "\r\nConnection: close\r\n\r\n" + xml);
                            socket->disconnectFromHost();
                        });
                        return;
                    }
                    if (delayNextPoll) {
                        delayNextPoll = false; delayedPoll = socket; return;
                    }
                    const QByteArray xml = hostInfo(currentGame);
                    socket->write("HTTP/1.1 " + QByteArray(responding ? "200 OK" : "404 Not Ready")
                        + "\r\nContent-Type: text/xml\r\nContent-Length: " + QByteArray::number(xml.size())
                        + "\r\nConnection: close\r\n\r\n" + xml);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    static QByteArray hostInfo(int game) {
        return QByteArray("<root status_code=\"200\"><hostname>Fixture</hostname>")
            + "<uniqueid>fixture-host</uniqueid><appversion>7.1.0.0</appversion>"
            + "<GfeVersion>3.24.0.0</GfeVersion><state>SUNSHINE_SERVER_"
            + (game ? "BUSY" : "FREE") + "</state><PairStatus>0</PairStatus><currentgame>"
            + QByteArray::number(game) + "</currentgame><HttpsPort>47984</HttpsPort></root>";
    }
    void init() {
        QSettings().clear(); requests = 0; responding = true;
        pairingRequests = 0; pairingChallengeReady = false; pairingResponseDelayMs = 1400;
        currentGame = 0; delayNextPoll = false; delayedPoll = nullptr;
        AntCloud::StreamBridge::install(nullptr);
    }
    void cleanup() {
        AntCloud::StreamBridge::install(nullptr);
        AntCloud::StreamBridge::setPairingGeneration(0);
        AntCloud::StreamBridge::setHostRequestsAllowed(false);
    }
    void pairingDispatchWaitsForPendingHostChallenge() {
        PairingDispatchProbe provider;
        provider.challengeReady = &pairingChallengeReady;
        AntCloud::StreamBridge::install(&provider);
        AntCloud::StreamBridge::setPairingGeneration(1);
        AntCloud::StreamBridge::setHostRequestsAllowed(true);
        NvHTTP http(NvAddress(QHostAddress::LocalHost, server.serverPort()), 0, QSslCertificate(), true);
        http.setProviderPairingPin("1234");
        const auto response = http.openConnectionToString(http.m_BaseUrlHttp, "pair",
            "phrase=getservercert", 2000, NvHTTP::NVLL_NONE);
        AntCloud::StreamBridge::install(nullptr);
        QCOMPARE(NvHTTP::getXmlString(response, "paired"), QString("0"));
        QCOMPARE(pairingRequests, 1);
        QCOMPARE(provider.submissions, 1);
        QVERIFY(!provider.submittedBeforeChallenge);
    }
    void finishedPairingReplyCancelsPendingPin() {
        PairingDispatchProbe provider;
        provider.challengeReady = &pairingChallengeReady;
        AntCloud::StreamBridge::install(&provider);
        AntCloud::StreamBridge::setPairingGeneration(1);
        AntCloud::StreamBridge::setHostRequestsAllowed(true);
        pairingResponseDelayMs = 50;
        NvHTTP http(NvAddress(QHostAddress::LocalHost, server.serverPort()), 0, QSslCertificate(), true);
        http.setProviderPairingPin("1234");
        http.openConnectionToString(http.m_BaseUrlHttp, "pair", "phrase=getservercert", 2000, NvHTTP::NVLL_NONE);
        QTest::qWait(1100);
        QCOMPARE(pairingRequests, 1);
        QCOMPARE(provider.submissions, 0);
    }
    void supersededPairingCannotSubmitIntoNewSession() {
        PairingDispatchProbe provider;
        provider.challengeReady = &pairingChallengeReady;
        AntCloud::StreamBridge::install(&provider);
        AntCloud::StreamBridge::setPairingGeneration(1);
        AntCloud::StreamBridge::setHostRequestsAllowed(true);
        QTimer::singleShot(50, this, [] { AntCloud::StreamBridge::setPairingGeneration(2); });
        NvHTTP http(NvAddress(QHostAddress::LocalHost, server.serverPort()), 0, QSslCertificate(), true);
        http.setProviderPairingPin("1234");
        http.openConnectionToString(http.m_BaseUrlHttp, "pair", "phrase=getservercert", 2000, NvHTTP::NVLL_NONE);
        QCOMPARE(provider.submissions, 0);

        pairingChallengeReady = false;
        http.setProviderPairingPin("1234");
        http.openConnectionToString(http.m_BaseUrlHttp, "pair", "phrase=getservercert", 2000, NvHTTP::NVLL_NONE);
        QCOMPARE(provider.submissions, 1);
        QVERIFY(!provider.submittedBeforeChallenge);
    }
    void cancelledPairingCannotSubmitIntoReconnect() {
        PairingDispatchProbe provider;
        provider.challengeReady = &pairingChallengeReady;
        AntCloud::StreamBridge::install(&provider);
        AntCloud::StreamBridge::setPairingGeneration(1);
        AntCloud::StreamBridge::setHostRequestsAllowed(true);
        NvHTTP http(NvAddress(QHostAddress::LocalHost, server.serverPort()), 0, QSslCertificate(), true);
        http.setProviderPairingPin("1234");
        QTimer::singleShot(50, this, [] { AntCloud::StreamBridge::setHostRequestsAllowed(false); });
        bool cancelled = false;
        try {
            http.openConnectionToString(http.m_BaseUrlHttp, "pair", "phrase=getservercert", 2000, NvHTTP::NVLL_NONE);
        } catch (const QtNetworkReplyException&) { cancelled = true; }
        QVERIFY(cancelled);
        QTest::qWait(1100);
        QCOMPARE(provider.submissions, 0);

        // Reuse the same HTTP/client process after cancellation. The old callback
        // must not consume or submit the fresh attempt's one-shot PIN.
        AntCloud::StreamBridge::setHostRequestsAllowed(true);
        pairingChallengeReady = false;
        http.setProviderPairingPin("1234");
        http.openConnectionToString(http.m_BaseUrlHttp, "pair", "phrase=getservercert", 2000, NvHTTP::NVLL_NONE);
        AntCloud::StreamBridge::install(nullptr);
        QCOMPARE(pairingRequests, 2);
        QCOMPARE(provider.submissions, 1);
        QVERIFY(!provider.submittedBeforeChallenge);
    }
    void cachedOnlineHostReconnectsWithoutRestart() {
        ComputerManager manager(StreamingPreferences::get());
        {
            ComputerSeeker first(&manager, address);
            QSignalSpy found(&first, &ComputerSeeker::computerFound);
            first.start(2500); QVERIFY(found.wait(3000));
        }
        QTest::qWait(200);
        QVERIFY(!manager.getComputers().isEmpty());
        QCOMPARE(manager.getComputers().first()->state, NvComputer::CS_ONLINE);
        const int before = requests;
        ComputerSeeker second(&manager, address);
        QSignalSpy found(&second, &ComputerSeeker::computerFound);
        second.start(2500); QVERIFY(found.wait(3000));
        QVERIFY(requests > before); // A fresh response, never cached-online success.
        QCOMPARE(found.count(), 1);
    }
    void interruptedPollCannotOverwriteReconnectedHost() {
        ComputerManager manager(StreamingPreferences::get());
        {
            ComputerSeeker first(&manager, address);
            QSignalSpy found(&first, &ComputerSeeker::computerFound);
            first.start(2500); QVERIFY(found.wait(3000));
        }
        NvComputer* computer = manager.getComputers().first();
        delayNextPoll = true;
        manager.startPolling();
        QTRY_VERIFY_WITH_TIMEOUT(!delayedPoll.isNull(), 2000);
        manager.stopPollingAsync();

        // The resumed Desktop is reported by the new monitor before the
        // retiring monitor completes its pre-Stop response.
        currentGame = 17;
        QSignalSpy fresh(&manager, &ComputerManager::computerPolled);
        manager.startPolling();
        QVERIFY(fresh.wait(2000));
        QCOMPARE(computer->currentGameId, 17);
        const auto stale = hostInfo(0);
        delayedPoll->write("HTTP/1.1 200 OK\r\nContent-Type: text/xml\r\nContent-Length: "
            + QByteArray::number(stale.size()) + "\r\nConnection: close\r\n\r\n" + stale);
        delayedPoll->disconnectFromHost();
        QTest::qWait(250);
        QCOMPARE(computer->currentGameId, 17);
        QCOMPARE(fresh.count(), 1);
        manager.stopPollingAsync();
    }
    void queuedRetiredPollCannotSatisfyFreshDiscovery() {
        ComputerManager manager(StreamingPreferences::get());
        QSignalSpy added(&manager, &ComputerManager::computerAddCompleted);
        manager.addNewHostManually(address);
        QVERIFY(added.wait(3000));
        QVERIFY(added.first().first().toBool());
        NvComputer* computer = manager.getComputers().first();
        delayNextPoll = true;
        manager.startPolling();
        QTRY_VERIFY_WITH_TIMEOUT(!delayedPoll.isNull(), 2000);
        const auto info = hostInfo(17);
        QVERIFY(sendResponse(delayedPoll, info));

        // Let the worker finish while its notifications remain queued on this
        // thread. Retiring it must invalidate those notifications as well as reads.
        bool updated = false;
        for (int i = 0; i < 100 && !updated; ++i) {
            QThread::msleep(5);
            QReadLocker lock(&computer->lock);
            updated = computer->currentGameId == 17;
        }
        QVERIFY(updated);
        manager.stopPollingAsync();
        QSignalSpy polled(&manager, &ComputerManager::computerPolled);
        QSignalSpy changed(&manager, &ComputerManager::computerStateChanged);
        // The new attempt still waits for its own response while the retiring
        // monitor's notifications are delivered into this event loop.
        currentGame = 17; delayNextPoll = true; delayedPoll = nullptr;
        ComputerSeeker next(&manager, address);
        QSignalSpy found(&next, &ComputerSeeker::computerFound);
        next.start(2500);
        QTRY_VERIFY_WITH_TIMEOUT(!delayedPoll.isNull(), 2000);
        QCOMPARE(polled.count(), 0);
        QCOMPARE(changed.count(), 0);
        QCOMPARE(found.count(), 0);
        QVERIFY(sendResponse(delayedPoll, info));
        QVERIFY(found.wait(3000));
        QCOMPARE(found.count(), 1);
        QCOMPARE(polled.count(), 1);
    }
    void host4KCapabilityUsesItsServerType_data() {
        QTest::addColumn<QString>("state");
        QTest::addColumn<QString>("gfeVersion");
        QTest::addColumn<bool>("supported");
        QTest::newRow("nvidia-unknown") << QString("MJOLNIR_SERVER_FREE") << QString() << false;
        QTest::newRow("nvidia-legacy") << QString("MJOLNIR_SERVER_FREE") << QString("2.11.0.0") << false;
        QTest::newRow("nvidia-3") << QString("MJOLNIR_SERVER_FREE") << QString("3.0.0.0") << true;
        QTest::newRow("nvidia-current") << QString("MJOLNIR_SERVER_FREE") << QString("3.24.0.0") << true;
        QTest::newRow("sunshine-unknown") << QString("SUNSHINE_SERVER_FREE") << QString() << true;
        QTest::newRow("sunshine-legacy-version") << QString("SUNSHINE_SERVER_FREE") << QString("2.11.0.0") << true;
        QTest::newRow("sunshine-current") << QString("SUNSHINE_SERVER_FREE") << QString("3.24.0.0") << true;
    }
    void host4KCapabilityUsesItsServerType() {
        QFETCH(QString, state);
        QFETCH(QString, gfeVersion);
        QFETCH(bool, supported);
        NvHTTP http(NvAddress(QHostAddress::LocalHost, server.serverPort()), 0, QSslCertificate(), true);
        const QString xml = QString("<root status_code=\"200\"><hostname>Fixture</hostname>"
            "<uniqueid>fixture-capability</uniqueid><appversion>7.1.0.0</appversion>"
            "<state>%1</state><GfeVersion>%2</GfeVersion></root>").arg(state, gfeVersion);
        NvComputer computer(http, xml);
        QCOMPARE(computer.supports4K(), supported);
    }
    void cleanupTestCase() { QThreadPool::globalInstance()->waitForDone(); }
    void failedFirstLookupRecoversWithinDeadline() {
        responding = false;
        ComputerManager manager(StreamingPreferences::get());
        ComputerSeeker seeker(&manager, address);
        QSignalSpy found(&seeker, &ComputerSeeker::computerFound);
        QSignalSpy attempted(&manager, &ComputerManager::computerAddCompleted);
        seeker.start(5000, true);
        QVERIFY(attempted.wait(2000));
        QVERIFY(!attempted.first()[0].toBool());
        responding = true;
        QVERIFY(found.wait(4000));
        QCOMPARE(found.count(), 1);
        QVERIFY(requests >= 2);
    }
    void cancellingLookupStopsRetriesAndAllowsReconnect() {
        responding = false;
        ComputerManager manager(StreamingPreferences::get());
        {
            ComputerSeeker seeker(&manager, address);
            QSignalSpy attempted(&manager, &ComputerManager::computerAddCompleted);
            seeker.start(5000, true); QVERIFY(attempted.wait(2000));
        }
        const int before = requests;
        QTest::qWait(2300);
        QCOMPARE(requests, before);
        responding = true;
        ComputerSeeker next(&manager, address);
        QSignalSpy found(&next, &ComputerSeeker::computerFound);
        next.start(2500, true); QVERIFY(found.wait(3000));
    }
};
QTEST_GUILESS_MAIN(SeekerTest)
#include "seeker_test.moc"
