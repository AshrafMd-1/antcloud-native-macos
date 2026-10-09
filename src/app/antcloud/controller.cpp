#include "controller.h"
#include "streambridge.h"
#include "backend/computermanager.h"
#include "backend/computerseeker.h"
#include "settings/streamingpreferences.h"
#include "streaming/session.h"
#include <QSettings>
#include <QSaveFile>
#include <QReadLocker>

namespace AntCloud {
namespace {
const QSize resolutions[] = {{1280,720}, {1920,1080}, {2560,1440}, {3840,2160}};
const int frameRates[] = {30,60,120};
}
Controller::Controller(QObject* parent) : QObject(parent), m_Worker(new ProviderWorker) {
    QSettings settings;
    const int resolutionSchema = settings.value("native/resolutionSchema", 1).toInt();
    m_ResolutionIndex = savedResolutionIndex(settings.value("native/resolutionIndex", 0).toInt(), resolutionSchema);
    if (resolutionSchema < 2) {
        settings.setValue("native/resolutionIndex", m_ResolutionIndex);
        settings.setValue("native/resolutionSchema", 2);
    }
    m_FpsIndex = qBound(0, settings.value("native/fpsIndex", 1).toInt(), 2);
    m_Bitrate = savedBitrate(settings.value("native/bitrate", 6000).toInt());
    m_CodecIndex = savedCodecIndex(settings.value("native/codecIndex", 0).toInt());
    m_Fullscreen = settings.value("native/fullscreen", true).toBool();
    m_RelativeMouse = settings.value("native/relativeMouse", true).toBool();
    m_ShowStats = settings.value("native/showStats", true).toBool();
    m_Worker->moveToThread(&m_Thread);
    connect(&m_Thread, &QThread::started, m_Worker, &ProviderWorker::initialize);
    connect(&m_Thread, &QThread::finished, m_Worker, &QObject::deleteLater);
    connect(m_Worker, &ProviderWorker::account, this, [this](bool signedIn, int limit, const QString& message) {
        m_SignedIn = signedIn; m_Limit = limit; m_Notice = message;
        setStage("idle", signedIn ? "Your Windows VM is ready to connect." : "Sign in to connect to your Windows VM.");
    });
    connect(m_Worker, &ProviderWorker::remainingMinutesChanged, this, [this](int minutes) {
        m_RemainingMinutes = minutes; emit changed();
    });
    connect(m_Worker, &ProviderWorker::authenticationFailed, this, [this](const QString& message) {
        m_Error = message; setStage("idle", "Sign in to connect to your Windows VM.");
    });
    connect(m_Worker, &ProviderWorker::stage, this, [this](const QString& phase, const QString& detail) {
        if (phase == "authenticating" || (m_Active && !m_Computer)) setStage(phase, detail);
    });
    connect(m_Worker, &ProviderWorker::hostAvailable, this, &Controller::findHost);
    connect(m_Worker, &ProviderWorker::nativeReady, this, [this] { record("Provider confirmed native connection; video control sent."); });
    connect(m_Worker, &ProviderWorker::ended, this, [this](const QString& message) {
        m_ProviderActive = false; finish(message);
    });
    connect(m_Worker, &ProviderWorker::notice, this, [this](const QString& message) { m_Notice = message; record(message); emit changed(); });
    connect(m_Worker, &ProviderWorker::shutdownRequested, this, [this] {
        m_ShutdownStatus = "Shutdown requested. Check the AntCloud dashboard for the final VM power state.";
        record("Shutdown request sent once; final power state unverified.");
        setStage("shuttingDown", "Disconnecting after the shutdown request…");
    });
    m_ElapsedTimer.setInterval(1000);
    connect(&m_ElapsedTimer, &QTimer::timeout, this, &Controller::elapsedChanged);
    m_AppTimeout.setSingleShot(true); m_AppTimeout.setInterval(30000);
    connect(&m_AppTimeout, &QTimer::timeout, this, [this] { finish("The native host did not offer Desktop in time. Check the dashboard before retrying."); });
    m_Thread.setObjectName("AntCloud provider"); m_Thread.start();
    StreamBridge::install(m_Worker);
}

Controller::~Controller() {
    // The seeker borrows the manager; destroy it before QObject deletes child objects.
    if (m_Seeker) { delete m_Seeker; m_Seeker = nullptr; }
    stopPolling();
    StreamBridge::install(nullptr);
    QMetaObject::invokeMethod(m_Worker, "stop", Qt::BlockingQueuedConnection);
    m_Thread.quit(); m_Thread.wait();
}

void Controller::initialize(QQuickWindow* window) {
    if (m_Window) return;
    m_Window = window;
    QMetaObject::invokeMethod(m_Worker, "restore", Qt::QueuedConnection);
}

void Controller::login(QString email, QString password, bool remember) {
    if (busy()) return;
    m_Error.clear(); m_Notice.clear(); setStage("authenticating", "Signing in to AntCloud…");
    QMetaObject::invokeMethod(m_Worker, "login", Qt::QueuedConnection,
        Q_ARG(QString, email), Q_ARG(QString, password), Q_ARG(bool, remember));
    password.fill(QChar('\0'));
}

void Controller::signOut() {
    if (busy()) return;
    m_Error.clear(); m_Notice.clear(); m_ShutdownStatus.clear();
    setStage("authenticating", "Removing your saved login…");
    QMetaObject::invokeMethod(m_Worker, "signOut", Qt::QueuedConnection);
}

void Controller::start() {
    if (!m_SignedIn || busy() || !m_Window) return;
    m_Error.clear(); m_Notice.clear(); m_StreamWarning.clear(); m_ShutdownStatus.clear();
    StreamBridge::setExpectedEnd(false);
    if (m_CodecIndex != savedCodecIndex(m_CodecIndex) || m_ResolutionIndex < 0 || m_ResolutionIndex > 3 || m_FpsIndex < 0 || m_FpsIndex > 2) { emit changed(); return; }
    const QSize size = resolutions[m_ResolutionIndex];
    StreamSettings spec{size.width(), size.height(), frameRates[m_FpsIndex], m_Bitrate};
    if (!spec.isValid() || spec.preset() > m_Limit) {
        m_Error = "Choose a resolution within your account’s limit and a bitrate from 2 to 50 Mbps."; emit changed(); return;
    }
    QSettings settings;
    settings.setValue("native/resolutionIndex", m_ResolutionIndex); settings.setValue("native/fpsIndex", m_FpsIndex);
    settings.setValue("native/bitrate", m_Bitrate); settings.setValue("native/codecIndex", m_CodecIndex); settings.setValue("native/fullscreen", m_Fullscreen);
    settings.setValue("native/relativeMouse", m_RelativeMouse); settings.setValue("native/showStats", m_ShowStats);
    m_Preferences = StreamingPreferences::get();
    m_Preferences->width = spec.width; m_Preferences->height = spec.height; m_Preferences->fps = spec.fps;
    m_Preferences->bitrateKbps = spec.bitrate;
    m_Preferences->windowMode = m_Fullscreen ? StreamingPreferences::WM_FULLSCREEN_DESKTOP : StreamingPreferences::WM_WINDOWED;
    m_Preferences->absoluteMouseMode = !m_RelativeMouse; m_Preferences->captureSysKeysMode = StreamingPreferences::CSK_ALWAYS;
    const StreamingPreferences::VideoCodecConfig codecs[] = {
        StreamingPreferences::VCC_FORCE_H264, StreamingPreferences::VCC_FORCE_HEVC, StreamingPreferences::VCC_FORCE_AV1
    };
    m_Preferences->videoCodecConfig = codecs[m_CodecIndex]; m_Preferences->enableHdr = false;
    m_Preferences->gameOptimizations = false; m_Preferences->quitAppAfter = false;
    m_Preferences->showPerformanceOverlay = m_ShowStats; m_Preferences->richPresence = false; m_Preferences->enableMdns = false;
    m_Active = true; m_ProviderActive = true; m_Computer = nullptr;
    m_Clock.start(); emit elapsedChanged(); m_ElapsedTimer.start(); setStage("connecting", "Opening your provider session…");
    StreamBridge::setHostRequestsAllowed(true);
    record(QString("Requested %1×%2, %3 FPS, %4 Kbps; provider preset %5.").arg(spec.width).arg(spec.height).arg(spec.fps).arg(spec.bitrate).arg(spec.preset()));
    QMetaObject::invokeMethod(m_Worker, "start", Qt::QueuedConnection, Q_ARG(int, spec.width), Q_ARG(int, spec.height), Q_ARG(int, spec.fps), Q_ARG(int, spec.bitrate));
}

void Controller::findHost(const QString& address) {
    if (!m_Active) return;
    setStage("host", "VM address available. Contacting the native streaming host…");
    if (!m_Manager) {
        m_Manager = new ComputerManager(m_Preferences); m_Manager->setParent(this);
        connect(m_Manager, &ComputerManager::pairingCompleted, this, [this](NvComputer* computer, const QString& error) {
            if (computer != m_Computer || !m_Pairing) return;
            m_Pairing = false;
            if (!m_Active) { setStage("idle", "Disconnected. You can connect again."); return; }
            if (!error.isEmpty()) { finish("Native pairing did not complete. Check the dashboard before retrying."); return; }
            record("Native pairing completed; certificate pinned.");
            setStage("launching", "Paired. Finding the Windows desktop…");
            m_AppTimeout.start(); launchIfReady();
        });
        connect(m_Manager, &ComputerManager::computerStateChanged, this, [this](NvComputer* computer) {
            if (m_Active && computer == m_Computer && !m_Pairing) launchIfReady();
        });
    }
    m_Manager->startPolling(); m_Polling = true;
    m_Seeker = new ComputerSeeker(m_Manager, address, this);
    connect(m_Seeker, &ComputerSeeker::computerFound, this, &Controller::hostFound);
    connect(m_Seeker, &ComputerSeeker::errorTimeout, this, [this] { finish("Could not reach the native streaming host. Check the dashboard and your connection."); });
    m_Seeker->start(qMax(1, StartupTimeoutMs - int(m_Clock.elapsed())), true);
}

void Controller::hostFound(NvComputer* computer) {
    if (!m_Active) return;
    m_Computer = computer; m_Seeker->deleteLater(); m_Seeker = nullptr;
    bool paired;
    { QReadLocker lock(&computer->lock); paired = computer->pairState == NvComputer::PS_PAIRED; }
    if (paired) { m_AppTimeout.start(); setStage("launching", "This Mac is paired. Finding the Windows desktop…"); launchIfReady(); }
    else {
        m_Pairing = true;
        setStage("pairing", "Pairing this Mac with your VM. No PIN entry is needed.");
        m_Manager->pairHost(computer, m_Manager->generatePinString());
    }
}

void Controller::launchIfReady() {
    if (!m_Active || !m_Computer || m_Pairing || m_Session) return;
    NvApp desktop;
    bool found = false, otherApp = false;
    {
        QReadLocker lock(&m_Computer->lock);
        for (const auto& app : m_Computer->appList) {
            if (app.name.compare("Desktop", Qt::CaseInsensitive) == 0) { desktop = app; found = true; break; }
        }
        otherApp = found && m_Computer->currentGameId != 0 && m_Computer->currentGameId != desktop.id;
    }
    if (!found) return;
    if (otherApp) { finish("Another host application is running. Stop that stream in the dashboard before connecting Desktop; it was not force-quit."); return; }
    m_AppTimeout.stop(); stopPolling();
    m_Session = new Session(m_Computer, desktop, m_Preferences);
    QQmlEngine::setObjectOwnership(m_Session, QQmlEngine::CppOwnership);
    connect(m_Session, &Session::stageStarting, this, [this](const QString&) {
        if (m_Active && !StreamBridge::expectedEnd()) setStage("video", "Starting the encrypted video and audio connection…");
    });
    connect(m_Session, &Session::stageFailed, this, [this](const QString&, int code, const QString&) {
        if (!m_Active || StreamBridge::expectedEnd()) return;
        m_Error = QString("Native streaming failed (error %1). Export diagnostics and check the dashboard.").arg(code); record(m_Error); emit changed();
    });
    connect(m_Session, &Session::displayLaunchError, this, [this](const QString&) {
        if (!m_Active || StreamBridge::expectedEnd()) return;
        m_Error = "The native stream could not start. Check your settings and the dashboard."; record(m_Error); emit changed();
    });
    connect(m_Session, &Session::connectionStarted, this, [this] {
        if (!m_Active || StreamBridge::expectedEnd()) return;
        m_Error.clear(); m_ElapsedTimer.stop();
        setStage("connected", "Video connected. Use Control + Command + Shift + P for local controls.");
        if (m_Window) m_Window->hide();
    });
    connect(m_Session, &Session::sessionFinished, this, [this](int) {
        StreamBridge::setActive(false);
        if (m_Window) { m_Window->show(); m_Window->requestActivate(); }
        QMetaObject::invokeMethod(m_Worker, "stopAfterStream", Qt::QueuedConnection);
        m_Active = false; m_StreamWarning.clear(); m_ElapsedTimer.stop();
        setStage("idle", "Disconnected.");
    });
    connect(m_Session, &Session::readyForDeletion, this, [this] {
        if (m_Session) m_Session->deleteLater();
        m_Session = nullptr; emit changed();
    });
    setStage("video", "Preparing the native decoder and input capture…");
    if (!m_Session->initialize(m_Window)) {
        m_Session->deleteLater(); m_Session = nullptr;
        finish("The native decoder or display could not be initialized. Check the stream settings."); return;
    }
    if (!m_Active) {
        m_Session->deleteLater(); m_Session = nullptr; emit changed(); return;
    }
    if (!m_Session->property("launchWarnings").toStringList().isEmpty())
        m_StreamWarning = "Stream advisory: the engine reported a configuration warning. If video or input fails, try 720p / 60 FPS.";
    StreamBridge::setActive(true);
    m_Session->start();
}

void Controller::finish(const QString& message) {
    if (!message.isEmpty()) { m_Error = message; record(message); }
    m_Active = false; m_StreamWarning.clear(); m_ElapsedTimer.stop(); m_AppTimeout.stop();
    if (m_Seeker) { delete m_Seeker; m_Seeker = nullptr; }
    stopPolling();
    QMetaObject::invokeMethod(m_Worker, "stop", Qt::QueuedConnection);
    if (m_Session) m_Session->interrupt();
    setStage(m_Pairing ? "cancelling" : "idle", m_Pairing ? "Disconnecting. Waiting for the pairing request to finish…" : (m_Error.isEmpty() ? "Disconnected." : "Disconnected. You can try connecting again."));
}

void Controller::disconnectStream() {
    if (m_Active) {
        // finish() queues cancellation; record intent before that single queue.
        StreamBridge::setExpectedEnd(true);
        finish({});
    }
}
void Controller::shutdown() { if (m_Active) QMetaObject::invokeMethod(m_Worker, "shutdown", Qt::QueuedConnection); }
void Controller::stopPolling() { if (m_Polling) { m_Manager->stopPollingAsync(); m_Polling = false; } }

void Controller::setStage(const QString& phase, const QString& detail) {
    if (phase != m_Phase || detail != m_Detail) record(phase + ": " + detail);
    m_Phase = phase; m_Detail = detail; emit changed();
}
void Controller::record(const QString& value) {
    // Fixed local messages only. No account responses, host addresses or decoder dumps.
    m_Diagnostics.append(QString("%1s %2").arg(elapsed()).arg(value).left(512));
    while (m_Diagnostics.size() > 64) m_Diagnostics.removeFirst();
}
void Controller::clearDiagnostics() { m_Diagnostics.clear(); m_Notice = "Diagnostic history cleared."; emit changed(); }
void Controller::exportDiagnostics(QUrl url) {
    if (!url.isLocalFile()) return;
    QSaveFile file(url.toLocalFile()); file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) { m_Notice = "Could not write the diagnostic report."; emit changed(); return; }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    const QByteArray bytes = ("AntCloud Native — redacted session summary\nNo raw account or transport payloads are recorded.\n\n" + m_Diagnostics.join('\n') + '\n').toUtf8();
    m_Notice = file.write(bytes) == bytes.size() && file.commit() ? "Diagnostic report exported." : "Could not write the diagnostic report.";
    emit changed();
}
}
