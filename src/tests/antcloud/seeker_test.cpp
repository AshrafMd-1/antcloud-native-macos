#include "backend/computerseeker.h"
#include "backend/computermanager.h"
#include "backend/identitymanager.h"
#include "antcloud/streambridge.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThreadPool>

// Real host HTTP parsing, polling and discovery; only unrelated engine services
// (preferences/compatibility/downloads) are isolated in host_test_services.cpp.
class SeekerTest : public QObject {
    Q_OBJECT
    QTemporaryDir storage;
    QTcpServer server;
    QString address;
    int requests = 0;
    bool responding = true;
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
                    const QByteArray xml = "<root status_code=\"200\"><hostname>Fixture</hostname>"
                        "<uniqueid>fixture-host</uniqueid><appversion>7.1.0.0</appversion>"
                        "<GfeVersion>3.24.0.0</GfeVersion><state>SUNSHINE_SERVER_FREE</state>"
                        "<PairStatus>0</PairStatus><currentgame>0</currentgame>"
                        "<HttpsPort>47984</HttpsPort></root>";
                    socket->write("HTTP/1.1 " + QByteArray(responding ? "200 OK" : "404 Not Ready")
                        + "\r\nContent-Type: text/xml\r\nContent-Length: " + QByteArray::number(xml.size())
                        + "\r\nConnection: close\r\n\r\n" + xml);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    void init() {
        QSettings().clear(); requests = 0; responding = true;
        AntCloud::StreamBridge::install(nullptr);
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
