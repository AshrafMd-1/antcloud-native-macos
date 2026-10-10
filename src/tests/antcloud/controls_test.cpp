#include "antcloudcontrols.h"
#include "streaming/video/overlaymanager.h"
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
    int requests = 0, stops = 0, microphoneToggles = 0, pairingRequests = 0;
    int settingUpdates = 0, microphonePreferences = 0, lastSetting = -1;
    bool lastSettingValue = false;
    qulonglong microphoneGeneration = 0, settingsGeneration = 0;
public slots:
    void shutdown() { ++requests; }
    void stop() { ++stops; }
    void pair(QString, qulonglong generation) {
        if (generation == AntCloud::StreamBridge::pairingGeneration()) ++pairingRequests;
    }
    void persistMicrophonePreference(bool value, qulonglong generation, qulonglong) {
        if (generation == settingsGeneration) {
            ++microphoneToggles; microphonePreferences += value ? 1 : -1;
        }
    }
    void persistStreamSetting(int setting, bool value, qulonglong generation) {
        if (generation == settingsGeneration) {
            ++settingUpdates; lastSetting = setting; lastSettingValue = value;
        }
    }
};
class OverlayReceiver : public Overlay::IOverlayRenderer {
public:
    int updates[Overlay::OverlayMax] = {};
    void notifyOverlayUpdated(Overlay::OverlayType type) override { ++updates[type]; }
};
class ControlsTest : public QObject {
    Q_OBJECT
private slots:
    void pairingDispatchRejectsStaleSessions() {
        using Bridge = AntCloud::StreamBridge;
        Receiver receiver;
        Bridge::install(&receiver); Bridge::setHostRequestsAllowed(true);
        Bridge::setPairingGeneration(1);
        Bridge::pair(QStringLiteral("1234"), 1);
        Bridge::setPairingGeneration(2);
        Bridge::pair(QStringLiteral("1234"), 1);
        QCoreApplication::processEvents(); QCOMPARE(receiver.pairingRequests, 0);
        Bridge::pair(QStringLiteral("1234"), 2);
        QTRY_COMPARE(receiver.pairingRequests, 1);
        Bridge::setHostRequestsAllowed(false);
        Bridge::pair(QStringLiteral("1234"), 2);
        QCoreApplication::processEvents(); QCOMPARE(receiver.pairingRequests, 1);
        Bridge::setPairingGeneration(0); Bridge::install(nullptr);
    }
    void controlsHintStateAndPlacement() {
        using namespace Overlay;
        using Bridge = AntCloud::StreamBridge;
        QVERIFY(SDL_Init(SDL_INIT_VIDEO) == 0);
        Receiver receiver; Bridge::install(&receiver); Bridge::setControlsHint(true);
        {
            OverlayManager manager;
            OverlayReceiver renderer;
            QVERIFY(manager.isOverlayEnabled(OverlayControlsHint));
            QVERIFY(!manager.isOverlayEnabled(OverlayDebug));
            QCOMPARE(manager.getOverlayFontSize(OverlayControlsHint), manager.getOverlayFontSize(OverlayDebug));
            QCOMPARE(QByteArray(manager.getOverlayText(OverlayControlsHint)), QByteArray("Ctrl + Cmd + Shift + P"));
            manager.setOverlayRenderer(&renderer);
            manager.setOverlayTextUpdated(OverlayControlsHint);
            QCOMPARE(renderer.updates[OverlayControlsHint], 1);
            auto hint = manager.getUpdatedOverlaySurface(OverlayControlsHint); QVERIFY(hint);
            QVERIFY(hint->w < 1280); QVERIFY(hint->h > 0);
            SDL_Rect top = OverlayManager::overlayRect(OverlayControlsHint, 1280, 720, hint->w, hint->h);
            QCOMPARE(top.x + top.w, 1280); QCOMPARE(top.y, 0);
            SDL_Rect bottomOrigin = OverlayManager::overlayRect(OverlayControlsHint, 1280, 720, hint->w, hint->h, true);
            QCOMPARE(bottomOrigin.x + bottomOrigin.w, 1280); QCOMPARE(bottomOrigin.y + bottomOrigin.h, 720);
            SDL_Rect stats = OverlayManager::overlayRect(OverlayDebug, 1280, 720, 300, 120);
            QCOMPARE(stats.x, 0); QCOMPARE(stats.y, 0);
            SDL_Rect status = OverlayManager::overlayRect(OverlayStatusUpdate, 1280, 720, 500, 50);
            QCOMPARE(status.x, 0); QCOMPARE(status.y, 670);
            SDL_Rect compact = OverlayManager::overlayRect(OverlayControlsHint, 600, 400, hint->w, hint->h);
            QCOMPARE(compact.x + compact.w, 600); QCOMPARE(compact.y, 0);
            auto captureDir = qEnvironmentVariable("ANTCLOUD_SCREENSHOT_DIR");
            if (!captureDir.isEmpty()) {
                QVERIFY(QDir().mkpath(captureDir));
                auto canvas = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ARGB8888); QVERIFY(canvas);
                SDL_FillRect(canvas, nullptr, SDL_MapRGB(canvas->format, 24, 28, 36));
                SDL_BlitSurface(hint, nullptr, canvas, &top);
                QCOMPARE(SDL_SaveBMP(canvas, QFile::encodeName(QDir(captureDir).filePath("stream-controls-hint.bmp"))), 0);
                SDL_FreeSurface(canvas);
            }
            SDL_FreeSurface(hint);
            manager.setControlsHint(false);
            QVERIFY(!manager.isOverlayEnabled(OverlayControlsHint));
            QVERIFY(!manager.getUpdatedOverlaySurface(OverlayControlsHint));
            QCOMPARE(renderer.updates[OverlayControlsHint], 2);
            manager.setControlsHint(false); QCOMPARE(renderer.updates[OverlayControlsHint], 2);
            manager.setControlsHint(true); QCOMPARE(renderer.updates[OverlayControlsHint], 3);
            auto next = manager.getUpdatedOverlaySurface(OverlayControlsHint); QVERIFY(next); SDL_FreeSurface(next);
            QCOMPARE(renderer.updates[OverlayDebug], 0); QCOMPARE(renderer.updates[OverlayStatusUpdate], 0);
        }
        Bridge::setControlsHint(false);
        { OverlayManager disabled; QVERIFY(!disabled.isOverlayEnabled(OverlayControlsHint)); }
        Bridge::install(nullptr); Bridge::setControlsHint(true);
        { OverlayManager retainedCli; QVERIFY(!retainedCli.isOverlayEnabled(OverlayControlsHint)); }
        SDL_Quit();
    }
    void microphoneMuteAndSessionGuards() {
        using Bridge = AntCloud::StreamBridge;
        Receiver receiver;
        Bridge::install(&receiver); Bridge::installSettingsController(&receiver);
        Bridge::setExpectedEnd(false); Bridge::setHostRequestsAllowed(true); Bridge::setActive(true);
        receiver.settingsGeneration = Bridge::beginSettingsSession();
        receiver.microphoneGeneration = Bridge::beginMicrophoneSession();
        Bridge::setMicrophonePreference(true);
        QVERIFY(!Bridge::microphoneAllowed());
        Bridge::setMicrophoneAllowed(true); QVERIFY(Bridge::microphoneAllowed());
        QVERIFY(Bridge::toggleMicrophone());
        QVERIFY(!Bridge::microphoneAllowed()); // Gate closes before the queued GUI callback.
        QTRY_COMPARE(receiver.microphoneToggles,1);
        QVERIFY(Bridge::toggleMicrophone());
        receiver.microphoneGeneration = Bridge::beginMicrophoneSession();
        QCoreApplication::processEvents();
        QCOMPARE(receiver.microphoneToggles,2); // Preference persistence is independent from capture generation.
        QVERIFY(Bridge::toggleMicrophone());
        Bridge::setMicrophoneAllowed(true); Bridge::invalidateMicrophoneSession();
        QVERIFY(!Bridge::microphoneAllowed());
        QVERIFY(Bridge::microphoneSessionGeneration()!=receiver.microphoneGeneration);
        Bridge::setMicrophoneAllowed(true,receiver.microphoneGeneration);
        QVERIFY(!Bridge::microphoneAllowed()); // A stale enable cannot reopen a terminated session.
        // The controller rejects an ended stream's generation before its GUI phase changes.
        receiver.microphoneGeneration=Bridge::microphoneSessionGeneration();
        QCoreApplication::processEvents(); QCOMPARE(receiver.microphoneToggles,3);
        Bridge::setMicrophoneAllowed(true); Bridge::setExpectedEnd(true);
        QVERIFY(!Bridge::microphoneAllowed()); QVERIFY(!Bridge::toggleMicrophone());
        Bridge::setExpectedEnd(false); QVERIFY(!Bridge::microphoneAllowed());
        Bridge::setMicrophoneAllowed(true); Bridge::setHostRequestsAllowed(false);
        QVERIFY(!Bridge::microphoneAllowed());
        Bridge::setHostRequestsAllowed(true); QVERIFY(!Bridge::microphoneAllowed());
        Bridge::setMicrophoneAllowed(true); Bridge::setActive(false);
        QVERIFY(!Bridge::microphoneAllowed()); QVERIFY(!Bridge::toggleMicrophone());
        Bridge::installSettingsController(nullptr); Bridge::install(nullptr);
    }
    void shortcutSettingsRejectRetiredSessions() {
        using Bridge = AntCloud::StreamBridge;
        Receiver receiver;
        Bridge::install(&receiver); Bridge::installSettingsController(&receiver);
        Bridge::setExpectedEnd(false); Bridge::setHostRequestsAllowed(true); Bridge::setActive(true);
        receiver.settingsGeneration = Bridge::beginSettingsSession();
        Bridge::persistStreamSetting(Bridge::Fullscreen, false);
        QTRY_COMPARE(receiver.settingUpdates, 1);
        QCOMPARE(receiver.lastSetting, int(Bridge::Fullscreen)); QVERIFY(!receiver.lastSettingValue);
        Bridge::persistStreamSetting(Bridge::RelativeMouse, true);
        Bridge::setActive(false);
        QTRY_COMPARE(receiver.settingUpdates, 2); // A change made before Stop is still retained.
        Bridge::setActive(true);
        Bridge::persistStreamSetting(Bridge::RelativeMouse, false);
        receiver.settingsGeneration = Bridge::beginSettingsSession();
        QCoreApplication::processEvents();
        QCOMPARE(receiver.settingUpdates, 2);
        Bridge::persistStreamSetting(Bridge::Stats, false);
        QTRY_COMPARE(receiver.settingUpdates, 3);
        QCOMPARE(receiver.lastSetting, int(Bridge::Stats)); QVERIFY(!receiver.lastSettingValue);
        Bridge::setActive(false); Bridge::installSettingsController(nullptr); Bridge::install(nullptr);
    }
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
            int panelWidth, panelHeight;
            SDL_GetWindowSize(panel, &panelWidth, &panelHeight);
            QCOMPARE(panelWidth, 660); QVERIFY(panelHeight >= 400);
            auto captureDir = qEnvironmentVariable("ANTCLOUD_SCREENSHOT_DIR");
            if (!captureDir.isEmpty()) {
                QVERIFY(QDir().mkpath(captureDir));
                auto renderer = SDL_GetRenderer(panel); QVERIFY(renderer);
                int width, height; QCOMPARE(SDL_GetRendererOutputSize(renderer, &width, &height), 0);
                auto surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888); QVERIFY(surface);
                QCOMPARE(SDL_RenderReadPixels(renderer, nullptr, surface->format->format, surface->pixels, surface->pitch), 0);
                QCOMPARE(SDL_SaveBMP(surface, QFile::encodeName(QDir(captureDir).filePath("stream-controls.bmp"))), 0);
                SDL_FreeSurface(surface);
            }
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
            event = {}; event.type=SDL_KEYDOWN; event.key.windowID=SDL_GetWindowID(panel); event.key.keysym.sym=SDLK_ESCAPE;
            QVERIFY(controls.handles(event)); QCOMPARE(controls.handle(event),AntCloudControls::Resume);
            QVERIFY(controls.show(stream));
            event.key.keysym.sym=SDLK_TAB; QCOMPARE(controls.handle(event),AntCloudControls::None);
            event.key.keysym.sym=SDLK_RETURN; QCOMPARE(controls.handle(event),AntCloudControls::Stop);
            event.key.keysym.sym=SDLK_TAB; controls.handle(event);
            event.key.keysym.sym=SDLK_SPACE; QCOMPARE(controls.handle(event),AntCloudControls::Shutdown);
            event.key.keysym.sym=SDLK_TAB; event.key.keysym.mod=KMOD_SHIFT; controls.handle(event);
            event.key.keysym.sym=SDLK_RETURN; QCOMPARE(controls.handle(event),AntCloudControls::Stop);
            event.key.keysym.sym=SDLK_RIGHT; event.key.keysym.mod=KMOD_NONE; controls.handle(event);
            event.key.keysym.sym=SDLK_RETURN; QCOMPARE(controls.handle(event),AntCloudControls::Shutdown);
            QVERIFY(controls.show(stream));
            event.key.keysym.sym=SDLK_RETURN; QCOMPARE(controls.handle(event),AntCloudControls::Resume);
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
