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
// UI fixtures never contact AntCloud and are not linked into the app.
class UiController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool signedIn MEMBER signedIn NOTIFY changed)
    Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
    Q_PROPERTY(bool active MEMBER active NOTIFY changed)
    Q_PROPERTY(bool fullscreen MEMBER fullscreen NOTIFY changed)
    Q_PROPERTY(bool relativeMouse MEMBER relativeMouse NOTIFY changed)
    Q_PROPERTY(bool showStats MEMBER showStats NOTIFY changed)
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
public:
    bool signedIn=false, busy=false, active=false, fullscreen=true, relativeMouse=true, showStats=true;
    QString phase="idle", detail="Sign in to connect to your Windows VM.", error, notice, streamWarning, shutdownStatus;
    int elapsed=83, resolutionLimit=1080, resolutionIndex=0, fpsIndex=1, bitrate=6000, loginCalls=0, shutdownCalls=0;
    Q_INVOKABLE void initialize(QQuickWindow*) {}
    Q_INVOKABLE void login(QString, QString, bool) { ++loginCalls; signedIn=true; detail="Your Windows VM is ready to connect."; emit changed(); }
    Q_INVOKABLE void signOut() { signedIn=false; emit changed(); }
    Q_INVOKABLE void start() { active=busy=true; phase="booting"; detail="Waiting for the VM to boot. This can take a minute or two."; emit changed(); }
    Q_INVOKABLE void disconnectStream() { active=busy=false; phase="idle"; emit changed(); }
    Q_INVOKABLE void shutdown() { ++shutdownCalls; disconnectStream(); }
    Q_INVOKABLE void exportDiagnostics(QUrl) {}
    Q_INVOKABLE void clearDiagnostics() {}
    void reset() { signedIn=busy=active=false; resolutionIndex=0; phase="idle"; detail="Sign in to connect to your Windows VM."; error.clear(); notice.clear(); streamWarning.clear(); shutdownStatus.clear(); loginCalls=shutdownCalls=0; emit changed(); }
signals:
    void changed();
};
class UiTest : public QObject {
    Q_OBJECT
    UiController controller;
    QQmlApplicationEngine engine;
    QQuickWindow* window=nullptr;
private slots:
    void initTestCase() {
        QTest::failOnWarning(QRegularExpression(".*Binding loop.*"));
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
        controller.reset(); window->setWidth(860); window->setHeight(740); window->show(); }
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
        QCOMPARE(window->activeFocusItem(),cancel);
        QTest::keyClick(window,Qt::Key_Tab);
        auto confirm=window->findChild<QQuickItem*>("confirmShutdown"); QVERIFY(confirm);
        QCOMPARE(window->activeFocusItem(),confirm);
        QTest::keyClick(window,Qt::Key_Return);
        QCOMPARE(controller.shutdownCalls,1);
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
        QTest::newRow("login-600-light") << 600 << false << "login";
        QTest::newRow("login-860-light") << 860 << false << "login";
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
        QPalette palette;
        palette.setColor(QPalette::Window,dark?QColor("#1c1c1e"):QColor("#f6f6f7"));
        palette.setColor(QPalette::WindowText,dark?QColor("#f2f2f7"):QColor("#202124"));
        palette.setColor(QPalette::Base,dark?QColor("#2c2c2e"):QColor("#ffffff"));
        palette.setColor(QPalette::Text,palette.color(QPalette::WindowText));
        palette.setColor(QPalette::Button,palette.color(QPalette::Base));
        palette.setColor(QPalette::ButtonText,palette.color(QPalette::WindowText));
        palette.setColor(QPalette::Mid,dark?QColor("#72727a"):QColor("#77777d"));
        QGuiApplication::setPalette(palette);
        window->setWidth(width); window->setHeight(state=="booting"?840:740);
        if(state!="login") { controller.signedIn=true; controller.detail="Your Windows VM is ready to connect."; }
        if(state=="experimental") controller.resolutionIndex=2;
        if(state=="4k") controller.resolutionIndex=3;
        if(state=="booting") controller.start();
        if(state=="shutdown") { controller.detail="Disconnected."; controller.shutdownStatus="Shutdown requested. Check the AntCloud dashboard for the final VM power state."; }
        if(state=="error") { controller.detail="Disconnected. You can try connecting again."; controller.error="The provider ended the session because video did not start. Check the dashboard, then retry."; }
        emit controller.changed(); QTest::qWait(200);
        if (state=="experimental" || state=="4k") {
            for (const char* name : {"resolutionPicker", "frameRatePicker", "bitratePicker"}) {
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
