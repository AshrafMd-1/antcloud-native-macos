#include <QtTest>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QPalette>
#include <QFontMetricsF>
#include <QFileInfo>
#include <QDir>
#include <cmath>
#include "antcloud/modifiermapping.h"
// UI fixtures never contact AntCloud and are not linked into the app.
class UiController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool signedIn MEMBER signedIn NOTIFY changed)
    Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
    Q_PROPERTY(bool active MEMBER active NOTIFY changed)
    Q_PROPERTY(bool fullscreen MEMBER fullscreen NOTIFY changed)
    Q_PROPERTY(bool relativeMouse MEMBER relativeMouse NOTIFY changed)
    Q_PROPERTY(bool showStats MEMBER showStats NOTIFY changed)
    Q_PROPERTY(bool showControlsHint MEMBER showControlsHint NOTIFY changed)
    Q_PROPERTY(bool microphoneEnabled MEMBER microphoneEnabled NOTIFY changed)
    Q_PROPERTY(QString microphoneStatus MEMBER microphoneStatus NOTIFY changed)
    Q_PROPERTY(QString phase MEMBER phase NOTIFY changed)
    Q_PROPERTY(QString detail MEMBER detail NOTIFY changed)
    Q_PROPERTY(QString error MEMBER error NOTIFY changed)
    Q_PROPERTY(QString notice MEMBER notice NOTIFY changed)
    Q_PROPERTY(QString streamWarning MEMBER streamWarning NOTIFY changed)
    Q_PROPERTY(QString shutdownStatus MEMBER shutdownStatus NOTIFY changed)
    Q_PROPERTY(int elapsed MEMBER elapsed NOTIFY changed)
    Q_PROPERTY(int resolutionLimit MEMBER resolutionLimit NOTIFY changed)
    Q_PROPERTY(int resolutionIndex MEMBER resolutionIndex NOTIFY changed)
    Q_PROPERTY(int fpsIndex MEMBER fpsIndex NOTIFY changed)
    Q_PROPERTY(int bitrate MEMBER bitrate NOTIFY changed)
    Q_PROPERTY(QVariantList bitrateChoices READ bitrateChoices CONSTANT)
    Q_PROPERTY(int codecIndex MEMBER codecIndex NOTIFY changed)
    Q_PROPERTY(int remainingMinutes MEMBER remainingMinutes NOTIFY changed)
    Q_PROPERTY(int commandModifier READ commandModifier NOTIFY changed)
    Q_PROPERTY(int optionModifier READ optionModifier NOTIFY changed)
    Q_PROPERTY(int controlModifier READ controlModifier NOTIFY changed)
public:
    AntCloud::ModifierMapping modifiers;
    int commandModifier() const { return modifiers.target(0); }
    int optionModifier() const { return modifiers.target(1); }
    int controlModifier() const { return modifiers.target(2); }
    Q_INVOKABLE void setModifierMapping(int row, int target) {
        if (!busy && modifiers.assign(row,target)) emit changed();
    }
    QVariantList bitrateChoices() const { return {2000, 3000, 4000, 6000, 8000, 10000, 12000, 15000, 20000, 25000, 30000, 35000, 40000, 45000, 50000}; }
    int codecIndex=0, remainingMinutes=-1;
    bool signedIn=false, busy=false, active=false, fullscreen=true, relativeMouse=true, showStats=true, showControlsHint=true, microphoneEnabled=false;
    QString microphoneStatus="Microphone off";
    Q_INVOKABLE void setMicrophoneEnabled(bool enabled) {
        if(busy && phase!="connected") return;
        microphoneEnabled=enabled; microphoneStatus=enabled?QString():QString("Microphone off"); emit changed();
    }
    QString phase="idle", detail="Sign in to connect to your Windows VM.", error, notice, streamWarning, shutdownStatus;
    int elapsed=83, resolutionLimit=1080, resolutionIndex=1, fpsIndex=1, bitrate=6000, loginCalls=0, shutdownCalls=0;
    Q_INVOKABLE void initialize(QQuickWindow*) {}
    Q_INVOKABLE void login(QString, QString, bool) { ++loginCalls; signedIn=true; detail="Your Windows VM is ready to connect."; emit changed(); }
    Q_INVOKABLE void signOut() { signedIn=false; emit changed(); }
    Q_INVOKABLE void start() { active=busy=true; phase="booting"; detail="Waiting for the VM to boot. This can take a minute or two."; emit changed(); }
    Q_INVOKABLE void disconnectStream() { active=busy=false; microphoneEnabled=false; microphoneStatus="Microphone off"; phase="idle"; emit changed(); }
    Q_INVOKABLE void shutdown() { ++shutdownCalls; disconnectStream(); }
    Q_INVOKABLE void exportDiagnostics(QUrl) {}
    Q_INVOKABLE void clearDiagnostics() {}
    void reset() { showControlsHint=true; signedIn=busy=active=microphoneEnabled=false; microphoneStatus="Microphone off"; resolutionIndex=1; phase="idle"; detail="Sign in to connect to your Windows VM."; error.clear(); notice.clear(); streamWarning.clear(); shutdownStatus.clear(); loginCalls=shutdownCalls=0; bitrate=6000; fpsIndex=1; codecIndex=0; remainingMinutes=-1; modifiers=AntCloud::ModifierMapping{}; emit changed(); }
signals:
    void changed();
};
class UiTest : public QObject {
    Q_OBJECT
    UiController controller;
    QQmlApplicationEngine engine;
    QQuickWindow* window=nullptr;
    static qreal textContrast(const QColor& foreground, const QColor& background) {
        auto luminance=[](const QColor& color) {
            auto linear=[](qreal value) { return value<=0.04045 ? value/12.92 : std::pow((value+0.055)/1.055,2.4); };
            return 0.2126*linear(color.redF())+0.7152*linear(color.greenF())+0.0722*linear(color.blueF());
        };
        const qreal first=luminance(foreground), second=luminance(background);
        return (qMax(first,second)+0.05)/(qMin(first,second)+0.05);
    }
    void setAppearance(bool dark) {
        QPalette palette;
        palette.setColor(QPalette::Window,dark?QColor("#1c1c1e"):QColor("#f6f6f7"));
        palette.setColor(QPalette::WindowText,dark?QColor("#f2f2f7"):QColor("#202124"));
        palette.setColor(QPalette::Base,dark?QColor("#2c2c2e"):QColor("#ffffff"));
        palette.setColor(QPalette::Text,palette.color(QPalette::WindowText));
        palette.setColor(QPalette::Button,palette.color(QPalette::Base));
        palette.setColor(QPalette::ButtonText,palette.color(QPalette::WindowText));
        palette.setColor(QPalette::Mid,dark?QColor("#72727a"):QColor("#77777d"));
        QGuiApplication::setPalette(palette);
    }
private slots:
    void initTestCase() {
        QTest::failOnWarning(QRegularExpression(".*Binding loop.*"));
        QFile version(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("../../app/antcloud/version.txt"));
        QVERIFY(version.open(QIODevice::ReadOnly));
        QCoreApplication::setApplicationVersion(QString::fromUtf8(version.readAll()).trimmed());
#ifdef Q_OS_MACOS
        QQuickStyle::setStyle("macOS");
#else
        QQuickStyle::setStyle("Basic");
#endif
        engine.setInitialProperties({{"controller", QVariant::fromValue(static_cast<QObject*>(&controller))}});
        engine.load(QUrl::fromLocalFile(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("../../app/gui/AntCloudMain.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        window=qobject_cast<QQuickWindow*>(engine.rootObjects().first()); QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }
    void init() {
        auto dialog=window->findChild<QObject*>("shutdownDialog");
        if(dialog) QMetaObject::invokeMethod(dialog,"close");
        auto controls=window->findChild<QObject*>("controlsDialog");
        if(controls) QMetaObject::invokeMethod(controls,"close");
        auto tabs=window->findChild<QObject*>("settingsTabs");
        if(tabs) tabs->setProperty("currentIndex",0);
        controller.reset(); window->setWidth(860); window->setHeight(740); window->show(); window->requestActivate(); QVERIFY(QTest::qWaitForWindowActive(window)); }
    void settingsTabsPreserveChoicesAndBusyGuards() {
        controller.signedIn=true; emit controller.changed();
        auto tabs=window->findChild<QObject*>("settingsTabs");
        auto video=window->findChild<QQuickItem*>("videoSettingsTab");
        auto keyboard=window->findChild<QQuickItem*>("keyboardSettingsTab");
        auto resolution=window->findChild<QQuickItem*>("resolutionPicker");
        auto mapping=window->findChild<QQuickItem*>("modifierMappingSection");
        QVERIFY(tabs && video && keyboard && resolution && mapping);
        QCOMPARE(tabs->property("currentIndex").toInt(),0);
        QTRY_VERIFY(resolution->isVisible()); QVERIFY(!mapping->isVisible());
        controller.resolutionIndex=1; emit controller.changed();
        keyboard->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_COMPARE(tabs->property("currentIndex").toInt(),1);
        QTRY_VERIFY(mapping->isVisible()); QVERIFY(!resolution->isVisible());
        controller.setModifierMapping(0,3);
        video->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_COMPARE(tabs->property("currentIndex").toInt(),0);
        QCOMPARE(controller.resolutionIndex,1); QCOMPARE(controller.commandModifier(),3);
        controller.start();
        QTRY_VERIFY(!resolution->isEnabled());
        keyboard->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_COMPARE(tabs->property("currentIndex").toInt(),1);
        QVERIFY(!mapping->isEnabled());
        QCOMPARE(controller.commandModifier(),3);
    }
    void microphoneOptInBusyGuardsAndCurrentHelp() {
        controller.signedIn=true; emit controller.changed();
        auto checkbox=window->findChild<QQuickItem*>("microphoneCheckbox"); QVERIFY(checkbox);
        auto privacy=window->findChild<QObject*>("microphonePrivacyNote"); QVERIFY(privacy);
        auto status=window->findChild<QObject*>("microphoneStatusLabel"); QVERIFY(status);
        QTRY_VERIFY(checkbox->isVisible() && checkbox->isEnabled());
        QVERIFY(!checkbox->property("checked").toBool());
        QVERIFY(!checkbox->property("text").toString().contains("Experimental"));
        QVERIFY(!privacy->property("visible").toBool());
        QVERIFY(privacy->property("text").toString().contains("unencrypted",Qt::CaseInsensitive));
        checkbox->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_VERIFY(controller.microphoneEnabled);
        QTRY_VERIFY(privacy->property("visible").toBool());
        QTRY_VERIFY(!status->property("visible").toBool());
        controller.microphoneStatus="Sending microphone"; emit controller.changed();
        QTRY_VERIFY(status->property("visible").toBool());
        controller.start(); QTRY_VERIFY(!checkbox->isEnabled());
        controller.phase="connected"; emit controller.changed(); QTRY_VERIFY(checkbox->isEnabled());
        checkbox->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_VERIFY(!controller.microphoneEnabled);
        QTRY_VERIFY(!privacy->property("visible").toBool());
        QTRY_VERIFY(!status->property("visible").toBool());
        controller.setMicrophoneEnabled(true); controller.disconnectStream();
        QTRY_VERIFY(!checkbox->property("checked").toBool());
        auto help=window->findChild<QObject*>("microphoneHelp"); QVERIFY(help);
        QVERIFY(help->property("text").toString().contains("Control + Command + Shift + B"));
        QVERIFY(!help->property("text").toString().contains("unverified"));
        QVERIFY(!help->property("text").toString().contains("unavailable"));
    }
    void controlsHintDefaultAndSavedChoice() {
        controller.signedIn=true; emit controller.changed();
        auto hint=window->findChild<QQuickItem*>("controlsHintCheckbox"); QVERIFY(hint);
        QTRY_VERIFY(hint->isVisible() && hint->isEnabled());
        QVERIFY(hint->property("checked").toBool());
        hint->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_VERIFY(!controller.showControlsHint);
        auto tabs=window->findChild<QObject*>("settingsTabs"); QVERIFY(tabs);
        tabs->setProperty("currentIndex",1); tabs->setProperty("currentIndex",0);
        QVERIFY(!hint->property("checked").toBool());
        controller.start(); QTRY_VERIFY(!hint->isEnabled());
    }
    void controlsHelpAndFullButtonFill_data() {
        QTest::addColumn<int>("width"); QTest::addColumn<int>("height"); QTest::addColumn<bool>("dark");
        QTest::newRow("compact-light") << 600 << 740 << false;
        QTest::newRow("wide-dark") << 860 << 740 << true;
        QTest::newRow("short-light") << 600 << 540 << false;
    }
    void controlsHelpAndFullButtonFill() {
        QFETCH(int,width); QFETCH(int,height); QFETCH(bool,dark);
        setAppearance(dark); window->setWidth(width); window->setHeight(height);
        controller.signedIn=true; emit controller.changed();
        auto accountScroll=window->findChild<QQuickItem*>("accountScroll"); QVERIFY(accountScroll);
        auto accountViewport=accountScroll->property("contentItem").value<QQuickItem*>(); QVERIFY(accountViewport);
        QTest::qWait(100);
        for(const char* name : {"videoSettingsTab","keyboardSettingsTab"}) {
            auto tab=window->findChild<QQuickItem*>(name); QVERIFY(tab);
            auto label=tab->property("contentItem").value<QQuickItem*>(); QVERIFY(label);
            QVERIFY2(textContrast(label->property("color").value<QColor>(),window->color())>=4.5,name);
        }
        accountViewport->setProperty("contentY",qMax(0.0,accountViewport->property("contentHeight").toReal()-accountViewport->height()));
        QTest::qWait(100);
        auto button=window->findChild<QQuickItem*>("controlsButton"); QVERIFY(button && button->isVisible());
        const QImage image=window->grabWindow(); QVERIFY(!image.isNull());
        const qreal scale=qreal(image.width())/window->width();
        for(qreal y : {qreal(3),button->height()-3}) {
            const QPointF point=button->mapToScene(QPointF(button->width()/2,y))*scale;
            const QColor pixel=image.pixelColor(qRound(point.x()),qRound(point.y()));
            QVERIFY2(pixel.red()<60 && pixel.green()<60 && pixel.blue()<60,"Controls background must fill its top and bottom edges");
        }
        button->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        auto dialog=window->findChild<QObject*>("controlsDialog"); QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QCOMPARE(dialog->property("title").toString(),QString("Controls · AntCloud Native ")+QCoreApplication::applicationVersion());
        const QString snapshotDir=qEnvironmentVariable("ANTCLOUD_SCREENSHOT_DIR");
        if(height==540 && !snapshotDir.isEmpty())
            QVERIFY(window->grabWindow().save(snapshotDir+"/controls-short-600-light.png"));
        controller.setModifierMapping(0,3);
        auto mapping=window->findChild<QObject*>("modifierControlsHelp"); QVERIFY(mapping);
        QTRY_COMPARE(mapping->property("text").toString(),QString("Command → Windows key\nOption → None\nControl → Alt"));
        auto rules=window->findChild<QObject*>("mappingRulesHelp"); QVERIFY(rules);
        QVERIFY(rules->property("text").toString().contains("Keyboard tab"));
        auto helpScroll=window->findChild<QQuickItem*>("controlsHelpScroll"); QVERIFY(helpScroll);
        auto flickable=helpScroll->property("contentItem").value<QQuickItem*>(); QVERIFY(flickable);
        QTRY_VERIFY(flickable->property("contentHeight").toReal() > flickable->height());
        QTRY_VERIFY(helpScroll->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Down);
        QTRY_VERIFY(flickable->property("contentY").toReal() > 0);
        flickable->setProperty("contentY",flickable->property("contentHeight").toReal()-flickable->height());
        auto microphone=window->findChild<QQuickItem*>("microphoneHelp"); QVERIFY(microphone);
        QTRY_VERIFY(microphone->mapToScene(QPointF(0,microphone->height())).y() <= helpScroll->mapToScene(QPointF(0,helpScroll->height())).y()+1);
        QVERIFY(!microphone->property("text").toString().contains("Experimental"));
        auto footer=dialog->property("footer").value<QQuickItem*>(); QVERIFY(footer);
        QVERIFY(footer->mapToScene(QPointF(0,footer->height())).y() <= window->height());
        auto footerFocused=[&] {
            for(auto item=window->activeFocusItem();item;item=item->parentItem())
                if(item==footer) return true;
            return false;
        };
        for(int tab=0; tab<4 && !footerFocused(); ++tab) QTest::keyClick(window,Qt::Key_Tab);
        QVERIFY(footerFocused());
        QTest::keyClick(window,Qt::Key_Space);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog,"open"));
        QTRY_COMPARE(flickable->property("contentY").toReal(),0.0);
        QTest::keyClick(window,Qt::Key_Escape);
    }
    void keyboardLoginClearsPassword() {
        auto email=window->findChild<QQuickItem*>("emailField"), password=window->findChild<QQuickItem*>("passwordField");
        QVERIFY(email && password);
        email->forceActiveFocus(); email->setProperty("text", "test@example.invalid");
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(window->activeFocusItem(), password);
        password->setProperty("text", "fixture-password"); QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(controller.loginCalls,1); QCOMPARE(password->property("text").toString(),QString());
    }
    void busyBlocksConnect() {
        controller.signedIn=true; controller.busy=true; controller.active=true; controller.phase="booting"; emit controller.changed();
        auto button=window->findChild<QObject*>("connectButton"); QVERIFY(button);
        QTRY_COMPARE(button->property("enabled").toBool(),false);
    }
    void returnCancelsShutdown() {
        controller.signedIn=true; controller.active=true; controller.busy=true; controller.phase="booting"; emit controller.changed();
        auto dialog=window->findChild<QObject*>("shutdownDialog"); QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(dialog,"open"));
        QTest::qWait(200); QTest::keyClick(window,Qt::Key_Return);
        QTRY_COMPARE(dialog->property("visible").toBool(),false);
        QCOMPARE(controller.shutdownCalls,0);
    }
    void deliberateShutdownRequiresSelection() {
        controller.signedIn=true; controller.active=true; controller.busy=true; controller.phase="booting"; emit controller.changed();
        auto dialog=window->findChild<QObject*>("shutdownDialog");
        QVERIFY(QMetaObject::invokeMethod(dialog,"open")); QTest::qWait(200);
        auto cancel=window->findChild<QQuickItem*>("cancelShutdown"); QVERIFY(cancel);
        QTRY_COMPARE(window->activeFocusItem(),cancel);
        QTest::keyClick(window,Qt::Key_Tab);
        auto confirm=window->findChild<QQuickItem*>("confirmShutdown"); QVERIFY(confirm);
        QTRY_COMPARE(window->activeFocusItem(),confirm);
        QTest::keyClick(window,Qt::Key_Tab,Qt::ShiftModifier);
        QTRY_COMPARE(window->activeFocusItem(),cancel);
        QCOMPARE(controller.shutdownCalls,0);
        QTest::keyClick(window,Qt::Key_Tab);
        QTRY_COMPARE(window->activeFocusItem(),confirm);
        QTest::keyClick(window,Qt::Key_Return);
        QCOMPARE(controller.shutdownCalls,1);
        QVERIFY(QMetaObject::invokeMethod(dialog,"open")); QTest::qWait(200);
        QTRY_COMPARE(window->activeFocusItem(),cancel);
        QTest::keyClick(window,Qt::Key_Return);
        QCOMPARE(controller.shutdownCalls,1); // Reopen returns to the safe default.
    }
    void modifierChoicesAndCollision() {
        controller.signedIn=true; emit controller.changed();
        auto tabs=window->findChild<QObject*>("settingsTabs"); QVERIFY(tabs);
        tabs->setProperty("currentIndex",1);
        auto command=window->findChild<QObject*>("commandModifierPicker");
        auto option=window->findChild<QObject*>("optionModifierPicker");
        auto control=window->findChild<QObject*>("controlModifierPicker");
        QVERIFY(command && option && control);
        QCOMPARE(command->property("count").toInt(),4);
        QTRY_COMPARE(command->property("currentText").toString(),QString("Ctrl"));
        QTRY_COMPARE(option->property("currentText").toString(),QString("Windows key"));
        QTRY_COMPARE(control->property("currentText").toString(),QString("Alt"));
        command->setProperty("currentIndex",3);
        QVERIFY(QMetaObject::invokeMethod(command,"activated",Q_ARG(int,3)));
        QTRY_COMPARE(option->property("currentText").toString(),QString("None"));
        QCOMPARE(controller.commandModifier(),3);
        option->setProperty("currentIndex",2);
        QVERIFY(QMetaObject::invokeMethod(option,"activated",Q_ARG(int,2)));
        QTRY_COMPARE(control->property("currentText").toString(),QString("None"));
        command->setProperty("currentIndex",0);
        QVERIFY(QMetaObject::invokeMethod(command,"activated",Q_ARG(int,0)));
        QCOMPARE(controller.commandModifier(),0);
        auto help=window->findChild<QObject*>("modifierControlsHelp"); QVERIFY(help);
        QTRY_VERIFY(help->property("text").toString().contains("Command → None\nOption → Alt\nControl → None"));
        controller.start();
        QTRY_VERIFY(!command->property("enabled").toBool());
        QTRY_VERIFY(!option->property("enabled").toBool());
        QTRY_VERIFY(!control->property("enabled").toBool());
        const auto saved=controller.modifiers.packed();
        controller.setModifierMapping(2,1);
        QCOMPARE(controller.modifiers.packed(),saved);
        controller.disconnectStream();
        QTRY_VERIFY(command->property("enabled").toBool());
        QCOMPARE(controller.commandModifier(),0);
    }
    void codecBitrateAndReportedTime() {
        controller.signedIn=true; controller.codecIndex=1; controller.bitrate=25000;
        controller.remainingMinutes=750; emit controller.changed();
        auto codec=window->findChild<QObject*>("codecPicker"); QVERIFY(codec);
        QTRY_COMPARE(codec->property("currentIndex").toInt(),1);
        QCOMPARE(codec->property("count").toInt(),3);
        QTRY_COMPARE(codec->property("currentText").toString(),QString("H.265 - Experimental"));
        auto bitrate=window->findChild<QObject*>("bitratePicker"); QVERIFY(bitrate);
        QCOMPARE(bitrate->property("count").toInt(),15);
        QTRY_COMPARE(bitrate->property("currentText").toString(),QString("25 Mbps"));
        for (int index : {13,14}) {
            bitrate->setProperty("currentIndex",index);
            QVERIFY(QMetaObject::invokeMethod(bitrate,"activated",Q_ARG(int,index)));
            const int expected=index==13 ? 45000 : 50000;
            QCOMPARE(controller.bitrate,expected);
            QCOMPARE(bitrate->property("currentText").toString(),QString("%1 Mbps").arg(expected/1000));
        }
        auto remaining=window->findChild<QObject*>("remainingTimeLabel"); QVERIFY(remaining);
        QTRY_COMPARE(remaining->property("text").toString(),QString("Time left: 750 minutes"));
        controller.remainingMinutes=-1; emit controller.changed();
        QTRY_COMPARE(remaining->property("text").toString(),QString("Time left: unavailable"));
        codec->setProperty("currentIndex",2);
        QVERIFY(QMetaObject::invokeMethod(codec,"activated",Q_ARG(int,2)));
        QCOMPARE(controller.codecIndex,2);
        QCOMPARE(codec->property("currentText").toString(),QString("AV1 - Experimental"));
    }
    void retiredResolutionAndPowerStatus() {
        controller.signedIn=true; controller.detail="Disconnected.";
        controller.shutdownStatus="Shutdown requested. Check the dashboard for the final power state.";
        emit controller.changed();
        auto picker=window->findChild<QObject*>("resolutionPicker"); QVERIFY(picker);
        QCOMPARE(picker->property("count").toInt(),4);
        for (int index=0; index<4; ++index) {
            QString label;
            QVERIFY(QMetaObject::invokeMethod(picker, "textAt", Q_RETURN_ARG(QString,label), Q_ARG(int,index)));
            QCOMPARE(label.contains("Experimental"), index >= 2);
        }
        auto power=window->findChild<QQuickItem*>("powerStatusSection"); QVERIFY(power);
        QTRY_VERIFY(power->isVisible());
        auto error=window->findChild<QQuickItem*>("errorMessage"); QVERIFY(error);
        QVERIFY(!error->isVisible());
        controller.shutdownStatus.clear(); controller.error="Fixture connection failure"; emit controller.changed();
        QTRY_VERIFY(!power->isVisible()); QTRY_VERIFY(error->isVisible());
        QVERIFY(error->property("color").value<QColor>().red() > error->property("color").value<QColor>().green());
    }
    void streamAdvisoryEndsWithStream() {
        controller.signedIn=true; controller.active=true;
        controller.streamWarning="Fixture stream advisory"; emit controller.changed();
        auto advisory=window->findChild<QQuickItem*>("streamAdvisory"); QVERIFY(advisory);
        QTRY_VERIFY(advisory->isVisible());
        controller.disconnectStream();
        QTRY_VERIFY(!advisory->isVisible());
    }
    void compactCenteringAndTallContentScrolling() {
        controller.signedIn=true; emit controller.changed();
        window->setHeight(900);
        auto content=window->findChild<QQuickItem*>("accountContent");
        auto scroll=window->findChild<QQuickItem*>("accountScroll");
        QVERIFY(content && scroll);
        QTRY_VERIFY(content->height() < scroll->property("availableHeight").toReal());
        QTRY_VERIFY(qAbs(content->y() - (scroll->property("availableHeight").toReal()-content->height())/2) < 1);
        window->setHeight(540); controller.start();
        QTRY_VERIFY(scroll->property("contentHeight").toReal() > scroll->property("availableHeight").toReal());
        auto flickable=scroll->property("contentItem").value<QQuickItem*>(); QVERIFY(flickable);
        flickable->setProperty("contentY", flickable->property("contentHeight").toReal()-flickable->height());
        auto hint=window->findChild<QQuickItem*>("controlsHint"); QVERIFY(hint);
        QTRY_VERIFY(hint->mapToScene(QPointF(0,hint->height())).y() <= scroll->mapToScene(QPointF(0,scroll->height())).y()+1);
        QVERIFY(hint->mapToScene(QPointF(0,0)).y() >= scroll->mapToScene(QPointF(0,0)).y());
    }
    void snapshots_data() {
        QTest::addColumn<int>("width"); QTest::addColumn<bool>("dark"); QTest::addColumn<QString>("state");
        QTest::newRow("mapping-600-light") << 600 << false << "mapping";
        QTest::newRow("mapping-860-dark") << 860 << true << "mapping";
        QTest::newRow("controls-600-light") << 600 << false << "controls";
        QTest::newRow("controls-860-dark") << 860 << true << "controls";
        QTest::newRow("controls-bottom-600-light") << 600 << false << "controls-bottom";
        QTest::newRow("codec-600-light") << 600 << false << "hevc";
        QTest::newRow("codec-860-dark") << 860 << true << "av1";
        QTest::newRow("time-600-light") << 600 << false << "quota";
        QTest::newRow("login-600-light") << 600 << false << "login";
        QTest::newRow("login-860-light") << 860 << false << "login";
        QTest::newRow("microphone-600-light") << 600 << false << "microphone";
        QTest::newRow("microphone-860-dark") << 860 << true << "microphone";
        QTest::newRow("ready-860-light") << 860 << false << "ready";
        QTest::newRow("experimental-600-light") << 600 << false << "experimental";
        QTest::newRow("experimental-860-dark") << 860 << true << "experimental";
        QTest::newRow("4k-600-light") << 600 << false << "4k";
        QTest::newRow("boot-600-light") << 600 << false << "booting";
        QTest::newRow("boot-860-dark") << 860 << true << "booting";
        QTest::newRow("shutdown-600-light") << 600 << false << "shutdown";
        QTest::newRow("shutdown-860-dark") << 860 << true << "shutdown";
        QTest::newRow("error-1100-dark") << 1100 << true << "error";
    }
    void snapshots() {
        QFETCH(int,width); QFETCH(bool,dark); QFETCH(QString,state);
        setAppearance(dark);
        window->setWidth(width); window->setHeight(state=="booting"?840:740);
        if(state!="login") { controller.signedIn=true; controller.detail="Your Windows VM is ready to connect."; }
        if(state=="experimental") controller.resolutionIndex=2;
        if(state=="4k") controller.resolutionIndex=3;
        if(state=="hevc") controller.codecIndex=1;
        if(state=="av1") controller.codecIndex=2;
        if(state=="quota") controller.remainingMinutes=750;
        if(state=="microphone") controller.setMicrophoneEnabled(true);
        if(state=="mapping") {
            controller.modifiers.assign(0,3); controller.modifiers.assign(2,0);
            window->findChild<QObject*>("settingsTabs")->setProperty("currentIndex",1);
        }
        if(state=="booting") controller.start();
        if(state=="shutdown") { controller.detail="Disconnected."; controller.shutdownStatus="Shutdown requested. Check the AntCloud dashboard for the final VM power state."; }
        if(state=="error") { controller.detail="Disconnected. You can try connecting again."; controller.error="The provider ended the session because video did not start. Check the dashboard, then retry."; }
        emit controller.changed(); QTest::qWait(200);
        if(state.startsWith("controls")) {
            auto dialog=window->findChild<QObject*>("controlsDialog"); QVERIFY(dialog);
            QVERIFY(QMetaObject::invokeMethod(dialog,"open")); QTest::qWait(100);
            if(state=="controls-bottom") {
                auto scroll=window->findChild<QQuickItem*>("controlsHelpScroll"); QVERIFY(scroll);
                auto flickable=scroll->property("contentItem").value<QQuickItem*>(); QVERIFY(flickable);
                flickable->setProperty("contentY",flickable->property("contentHeight").toReal()-flickable->height());
                QTest::qWait(100);
            }
        }
        if (state=="experimental" || state=="4k" || state=="hevc" || state=="av1") {
            for (const char* name : {"resolutionPicker", "frameRatePicker", "bitratePicker", "codecPicker"}) {
                auto picker=window->findChild<QQuickItem*>(name); QVERIFY(picker);
                auto text=picker->property("contentItem").value<QQuickItem*>(); QVERIFY(text);
                const QFontMetricsF metrics(picker->property("font").value<QFont>());
                QVERIFY2(text->width() >= metrics.horizontalAdvance(picker->property("currentText").toString()), name);
            }
        }
        auto image=window->grabWindow(); QVERIFY(!image.isNull());
        const QString path=qEnvironmentVariable("ANTCLOUD_SCREENSHOT_DIR");
        if(!path.isEmpty()) QVERIFY(image.save(path+"/"+QString::fromUtf8(QTest::currentDataTag())+".png"));
        for(auto item: window->findChildren<QQuickItem*>()) {
            if(!item->isVisible() || item->width()<=0 || item->objectName().isEmpty()) continue;
            const QPointF point=item->mapToScene(QPointF(0,0));
            QVERIFY2(point.x()>=-1 && point.x()+item->width()<=width+1,qPrintable(item->objectName()));
        }
    }
};
QTEST_MAIN(UiTest)
#include "ui_test.moc"
