#include "antcloud/controller.h"
#include "antcloud/microphone.h"
#include "antcloud/streambridge.h"
#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>

QAtomicInt g_AsyncLoggingEnabled;

class SettingsTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_Storage;
private slots:
    void initTestCase() {
        QVERIFY(m_Storage.isValid());
        QCoreApplication::setOrganizationName("AntCloudSettingsFixture");
        QCoreApplication::setApplicationName("NativePreferences");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_Storage.path());
    }
    void defaultsAndImmediatePersistence() {
        QSettings().clear();
        {
            AntCloud::Controller controller;
            QCOMPARE(controller.resolutionIndex(), 1);
            QCOMPARE(controller.fpsIndex(), 1);
            QCOMPARE(controller.bitrate(), 10000);
            QCOMPARE(controller.codecIndex(), 0);
            QVERIFY(controller.fullscreen());
            QVERIFY(controller.relativeMouse());
            QVERIFY(controller.showStats());
            QVERIFY(controller.showControlsHint());
            QVERIFY(!controller.microphoneEnabled());

            controller.setResolutionIndex(4);
            controller.setFpsIndex(2);
            controller.setBitrate(45000);
            controller.setCodecIndex(1);
            controller.setFullscreen(false);
            controller.setRelativeMouse(false);
            controller.setShowStats(false);
            controller.setShowControlsHint(false);
            controller.setMicrophoneEnabled(true);
            controller.setModifierMapping(0, 3);

            QSettings stored;
            stored.sync(); QCOMPARE(stored.status(), QSettings::NoError);
            QCOMPARE(stored.value("native/resolutionIndex").toInt(), 4);
            QCOMPARE(stored.value("native/resolutionSchema").toInt(), AntCloud::ResolutionSchema);
            QCOMPARE(stored.value("native/fpsIndex").toInt(), 2);
            QCOMPARE(stored.value("native/bitrate").toInt(), 45000);
            QCOMPARE(stored.value("native/codecIndex").toInt(), 1);
            QVERIFY(!stored.value("native/fullscreen").toBool());
            QVERIFY(!stored.value("native/relativeMouse").toBool());
            QVERIFY(!stored.value("native/showStats").toBool());
            QVERIFY(!stored.value("native/showControlsHint").toBool());
            QVERIFY(stored.value("native/microphoneEnabled").toBool());

            auto microphone = controller.findChild<AntCloud::Microphone*>();
            QVERIFY(microphone);
            microphone->startSession("not-an-address");
            QVERIFY(!microphone->enabled());
            QVERIFY(controller.microphoneEnabled());
            QVERIFY(controller.microphoneStatus().contains("unavailable"));

            QSignalSpy preferenceChanged(&controller, &AntCloud::Controller::changed);
            controller.setMicrophoneEnabled(false);
            QVERIFY(!controller.microphoneEnabled());
            QCOMPARE(preferenceChanged.count(), 1);
            controller.beginSettingsSessionForTest();
            AntCloud::StreamBridge::setExpectedEnd(false);
            AntCloud::StreamBridge::setHostRequestsAllowed(true);
            AntCloud::StreamBridge::setActive(true);
            QVERIFY(AntCloud::StreamBridge::toggleMicrophone());
            AntCloud::StreamBridge::setExpectedEnd(true);
            AntCloud::StreamBridge::setActive(false);
            AntCloud::StreamBridge::invalidateMicrophoneSession();
            QTRY_VERIFY(controller.microphoneEnabled());
            QSettings afterStop; afterStop.sync();
            QVERIFY(afterStop.value("native/microphoneEnabled").toBool());

            controller.beginSettingsSessionForTest();
            AntCloud::StreamBridge::setExpectedEnd(false);
            AntCloud::StreamBridge::setHostRequestsAllowed(true);
            AntCloud::StreamBridge::setActive(true);
            QVERIFY(AntCloud::StreamBridge::toggleMicrophone());
            QVERIFY(AntCloud::StreamBridge::toggleMicrophone());
            QVERIFY(!AntCloud::StreamBridge::microphoneAllowed());
            QCoreApplication::processEvents();
            QVERIFY(controller.microphoneEnabled());
            QSettings rapidToggle; rapidToggle.sync();
            QVERIFY(rapidToggle.value("native/microphoneEnabled").toBool());
            AntCloud::StreamBridge::setActive(false);
            AntCloud::StreamBridge::setExpectedEnd(false);
            AntCloud::StreamBridge::setHostRequestsAllowed(false);
        }
        {
            AntCloud::Controller restored;
            QCOMPARE(restored.resolutionIndex(), 4);
            QCOMPARE(restored.fpsIndex(), 2);
            QCOMPARE(restored.bitrate(), 45000);
            QCOMPARE(restored.codecIndex(), 1);
            QVERIFY(!restored.fullscreen());
            QVERIFY(!restored.relativeMouse());
            QVERIFY(!restored.showStats());
            QVERIFY(!restored.showControlsHint());
            QVERIFY(restored.microphoneEnabled());
            QCOMPARE(restored.commandModifier(), 3);
            QCOMPARE(restored.optionModifier(), 0);
            QCOMPARE(restored.controlModifier(), 2);
        }
    }
    void schemaTwoCorruptionDoesNotAdoptNewMode() {
        QSettings stored;
        stored.clear();
        stored.setValue("native/resolutionSchema", 2);
        stored.setValue("native/resolutionIndex", 4);
        stored.sync();
        AntCloud::Controller controller;
        QCOMPARE(controller.resolutionIndex(), AntCloud::DefaultResolutionIndex);
        QSettings migrated; migrated.sync();
        QCOMPARE(migrated.value("native/resolutionSchema").toInt(), AntCloud::ResolutionSchema);
        QCOMPARE(migrated.value("native/resolutionIndex").toInt(), AntCloud::DefaultResolutionIndex);
    }
    void shortcutPersistenceAndGenerationGuards() {
        using Bridge = AntCloud::StreamBridge;
        QSettings().clear();
        {
            AntCloud::Controller controller;
            controller.beginSettingsSessionForTest();
            Bridge::setExpectedEnd(false);
            Bridge::setHostRequestsAllowed(true);
            Bridge::setActive(true);

            QVERIFY(Bridge::persistStreamSetting(Bridge::Fullscreen, false));
            QVERIFY(Bridge::persistStreamSetting(Bridge::RelativeMouse, false));
            QVERIFY(Bridge::persistStreamSetting(Bridge::Stats, false));
            QVERIFY(Bridge::persistStreamSetting(Bridge::ControlsHint, false));
            QVERIFY(Bridge::toggleMicrophone());

            // Stop can close capture before the GUI receives the accepted
            // setting callbacks, but the user's choices must still be saved.
            Bridge::setExpectedEnd(true);
            Bridge::setActive(false);
            Bridge::invalidateMicrophoneSession();
            QTRY_VERIFY(!controller.fullscreen());
            QTRY_VERIFY(!controller.relativeMouse());
            QTRY_VERIFY(!controller.showStats());
            QTRY_VERIFY(!controller.showControlsHint());
            QTRY_VERIFY(controller.microphoneEnabled());
            QVERIFY(!Bridge::microphoneAllowed());

            QSettings stopped;
            stopped.sync();
            QVERIFY(!stopped.value("native/fullscreen").toBool());
            QVERIFY(!stopped.value("native/relativeMouse").toBool());
            QVERIFY(!stopped.value("native/showStats").toBool());
            QVERIFY(!stopped.value("native/showControlsHint").toBool());
            QVERIFY(stopped.value("native/microphoneEnabled").toBool());

            Bridge::setExpectedEnd(false);
            Bridge::setHostRequestsAllowed(true);
            Bridge::setActive(true);
            controller.beginSettingsSessionForTest();
            QVERIFY(Bridge::persistStreamSetting(Bridge::Fullscreen, true));
            QVERIFY(Bridge::persistStreamSetting(Bridge::RelativeMouse, true));
            QVERIFY(Bridge::persistStreamSetting(Bridge::Stats, true));
            QVERIFY(Bridge::persistStreamSetting(Bridge::ControlsHint, true));
            QVERIFY(Bridge::toggleMicrophone());

            // A new stream reseeds the bridge from the saved microphone
            // preference and rejects every callback queued by the old stream.
            controller.beginSettingsSessionForTest();
            QCoreApplication::processEvents();
            QVERIFY(!controller.fullscreen());
            QVERIFY(!controller.relativeMouse());
            QVERIFY(!controller.showStats());
            QVERIFY(!controller.showControlsHint());
            QVERIFY(controller.microphoneEnabled());
            QVERIFY(Bridge::microphonePreference());

            // The first B press in the new stream must toggle the restored
            // value rather than the stale atomic value from the old stream.
            QVERIFY(Bridge::toggleMicrophone());
            QVERIFY(!Bridge::microphoneAllowed());
            QTRY_VERIFY(!controller.microphoneEnabled());
            QSettings current;
            current.sync();
            QVERIFY(!current.value("native/microphoneEnabled").toBool());
            Bridge::setActive(false);
            Bridge::setHostRequestsAllowed(false);
            Bridge::setExpectedEnd(false);
        }
        {
            AntCloud::Controller restored;
            QVERIFY(!restored.fullscreen());
            QVERIFY(!restored.relativeMouse());
            QVERIFY(!restored.showStats());
            QVERIFY(!restored.showControlsHint());
            QVERIFY(!restored.microphoneEnabled());
        }
    }
};

QTEST_GUILESS_MAIN(SettingsTest)
#include "settings_test.moc"
