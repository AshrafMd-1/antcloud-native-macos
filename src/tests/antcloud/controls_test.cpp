#include "antcloudcontrols.h"
#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QDir>

QByteArray Path::readDataFile(QString) {
    QFile file(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("../../app/ModeSeven.ttf"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
class Receiver : public QObject {
    Q_OBJECT
public:
    int requests = 0, stops = 0;
public slots:
    void shutdown() { ++requests; }
    void stop() { ++stops; }
};
class ControlsTest : public QObject {
    Q_OBJECT
private slots:
    void localEventsAndExplicitShutdown() {
        AntCloud::StreamBridge::setExpectedEnd(false);
        QVERIFY(SDL_Init(SDL_INIT_VIDEO) == 0); QVERIFY(TTF_Init() == 0);
        auto stream = SDL_CreateWindow("test",0,0,1280,720,SDL_WINDOW_HIDDEN); QVERIFY(stream);
        Receiver receiver;
        {
            AntCloudControls controls;
            QVERIFY(!controls.enabled()); QVERIFY(!controls.show(stream));
            AntCloud::StreamBridge::install(&receiver);
            QVERIFY(controls.enabled()); QVERIFY(controls.show(stream));
            SDL_Window* panel = nullptr;
            for (Uint32 id=1;id<32;++id) { auto item=SDL_GetWindowFromID(id); if(item && item!=stream) panel=item; }
            QVERIFY(panel);
            SDL_Event event{}; event.type=SDL_MOUSEBUTTONDOWN;
            event.button.windowID=SDL_GetWindowID(stream); QVERIFY(!controls.handles(event));
            event.button.windowID=SDL_GetWindowID(panel); event.button.button=SDL_BUTTON_LEFT;
            QVERIFY(controls.handles(event));
            // A click is only a candidate action; confirmation is required by the input handler.
            auto hit=[&](int x,int end) {
                event.type=SDL_MOUSEBUTTONDOWN; event.button.x=x; event.button.y=70; controls.handle(event);
                event.type=SDL_MOUSEBUTTONUP; event.button.x=end; return controls.handle(event);
            };
            QCOMPARE(hit(80,80),AntCloudControls::Resume);
            QCOMPARE(hit(300,300),AntCloudControls::Stop);
            QCOMPARE(hit(520,520),AntCloudControls::Shutdown);
            QCOMPARE(hit(300,520),AntCloudControls::None);
            QCOMPARE(receiver.requests,0);
            event.type=SDL_KEYDOWN; event.key.windowID=SDL_GetWindowID(panel); event.key.keysym.sym=SDLK_ESCAPE;
            QVERIFY(controls.handles(event)); QCOMPARE(controls.handle(event),AntCloudControls::Resume);
            QVERIFY(controls.requestShutdown());
            QTRY_COMPARE(receiver.requests,1);
            QVERIFY(!AntCloud::StreamBridge::expectedEnd()); // Queueing is not write acceptance.
            AntCloud::StreamBridge::requestStop();
            QVERIFY(AntCloud::StreamBridge::expectedEnd());
            QTRY_COMPARE(receiver.stops,1);
            QCOMPARE(receiver.requests,1); // Stop never requests shutdown.
            AntCloud::StreamBridge::install(nullptr);
            AntCloud::StreamBridge::setExpectedEnd(false);
        }
        SDL_DestroyWindow(stream); TTF_Quit(); SDL_Quit();
    }
};
QTEST_GUILESS_MAIN(ControlsTest)
#include "controls_test.moc"
