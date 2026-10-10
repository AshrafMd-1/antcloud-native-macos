#include "controller.h"
#include "streambridge.h"
#include "microphone.h"
#include "remainingtime.h"
#include "backend/computermanager.h"
#include "backend/computerseeker.h"
#include "settings/streamingpreferences.h"
#include "streaming/session.h"
#include <QSettings>
#include <QSaveFile>
#include <QReadLocker>

namespace AntCloud {
namespace {
const QSize resolutions[] = {
    {1280,720}, {1920,1080}, {2560,1440}, {3840,2160},
    {2560,1600}, {2048,1280}, {1920,1200}, {1680,1050},
    {1440,900}, {1280,800}, {1024,640}, {960,600}
};
const int frameRates[] = {30,60,120};
}
Controller::Controller(QObject* parent) : QObject(parent), m_Worker(new ProviderWorker), m_Microphone(new Microphone(this)) {
    StreamBridge::installSettingsController(this);
    QSettings settings;
    const int resolutionSchema = settings.value("native/resolutionSchema", 1).toInt();
    m_ResolutionIndex = savedResolutionIndex(settings.value("native/resolutionIndex", DefaultResolutionIndex).toInt(), resolutionSchema);
    if (resolutionSchema < ResolutionSchema) {
        settings.setValue("native/resolutionIndex", m_ResolutionIndex);
        settings.setValue("native/resolutionSchema", ResolutionSchema);
    }
    m_FpsIndex = qBound(0, settings.value("native/fpsIndex", 1).toInt(), 2);
    m_Bitrate = savedBitrate(settings.value("native/bitrate", 10000).toInt());
    m_CodecIndex = savedCodecIndex(settings.value("native/codecIndex", 0).toInt());
    m_Fullscreen = settings.value("native/fullscreen", true).toBool();
    m_RelativeMouse = settings.value("native/relativeMouse", true).toBool();
    m_ShowStats = settings.value("native/showStats", true).toBool();
    m_ShowControlsHint = settings.value("native/showControlsHint", true).toBool();
    m_MicrophonePreference = settings.value("native/microphoneEnabled", false).toBool();
    StreamBridge::setMicrophonePreference(m_MicrophonePreference);
    m_Microphone->setEnabled(m_MicrophonePreference);
    connect(m_Microphone, &Microphone::changed, this, [this] {
        if (!m_Microphone->enabled()) StreamBridge::setMicrophoneAllowed(false);
        emit changed();
    });
    m_Modifiers = savedModifierMapping(settings.value("native/modifierMapping").toList());
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
        m_ShutdownStatus = "Shutdown requested.";
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
    StreamBridge::installSettingsController(nullptr);
    m_Microphone->stopSession();
    // The seeker borrows the manager; destroy it before QObject deletes child objects.
    if (m_Seeker) { delete m_Seeker; m_Seeker = nullptr; }
    stopPolling();
    StreamBridge::install(nullptr);
    QMetaObject::invokeMethod(m_Worker, "stop", Qt::BlockingQueuedConnection);
    m_Thread.quit(); m_Thread.wait();
}

QString Controller::remainingTime() const {
    char text[32]{};
    formatRemainingTime(text, sizeof(text), m_RemainingMinutes);
    return QString::fromLatin1(text);
}

#ifdef ANTCLOUD_TESTING
void Controller::beginSettingsSessionForTest() {
    StreamBridge::setMicrophonePreference(m_MicrophonePreference);
    m_SettingsGeneration = StreamBridge::beginSettingsSession();
    m_MicrophoneGeneration = StreamBridge::beginMicrophoneSession();
}
#endif

void Controller::setResolutionIndex(int value) {
    const int normalized = savedResolutionIndex(value, ResolutionSchema);
    if (busy() || normalized != value || m_ResolutionIndex == value) return;
    m_ResolutionIndex = value; QSettings().setValue("native/resolutionIndex", value); emit settingsChanged();
}
void Controller::setFpsIndex(int value) {
    if (busy() || value < 0 || value > 2 || m_FpsIndex == value) return;
    m_FpsIndex = value; QSettings().setValue("native/fpsIndex", value); emit settingsChanged();
}
void Controller::setBitrate(int value) {
    if (busy() || savedBitrate(value) != value || m_Bitrate == value) return;
    m_Bitrate = value; QSettings().setValue("native/bitrate", value); emit settingsChanged();
}
void Controller::setCodecIndex(int value) {
    if (busy() || savedCodecIndex(value) != value || m_CodecIndex == value) return;
    m_CodecIndex = value; QSettings().setValue("native/codecIndex", value); emit settingsChanged();
}
void Controller::setFullscreen(bool value) {
    if (busy() || m_Fullscreen == value) return;
    m_Fullscreen = value; QSettings().setValue("native/fullscreen", value); emit settingsChanged();
}
void Controller::setRelativeMouse(bool value) {
    if (busy() || m_RelativeMouse == value) return;
    m_RelativeMouse = value; QSettings().setValue("native/relativeMouse", value); emit settingsChanged();
}
void Controller::setShowStats(bool value) {
    if (busy() || m_ShowStats == value) return;
    m_ShowStats = value; QSettings().setValue("native/showStats", value); emit settingsChanged();
}
void Controller::setShowControlsHint(bool value) {
    if (busy() || m_ShowControlsHint == value) return;
    m_ShowControlsHint = value; QSettings().setValue("native/showControlsHint", value); emit settingsChanged();
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
    m_Microphone->stopSession();
    m_Error.clear(); m_Notice.clear(); m_ShutdownStatus.clear();
    setStage("authenticating", "Removing your saved login…");
    QMetaObject::invokeMethod(m_Worker, "signOut", Qt::QueuedConnection);
}

void Controller::start() {
    if (!m_SignedIn || busy() || !m_Window) return;
    m_Error.clear(); m_Notice.clear(); m_StreamWarning.clear(); m_ShutdownStatus.clear();
    StreamBridge::setExpectedEnd(false);
    if (m_CodecIndex != savedCodecIndex(m_CodecIndex) || m_ResolutionIndex < 0 || m_ResolutionIndex > 11 || m_FpsIndex < 0 || m_FpsIndex > 2) { emit changed(); return; }
    const QSize size = resolutions[m_ResolutionIndex];
    StreamSettings spec{size.width(), size.height(), frameRates[m_FpsIndex], m_Bitrate};
    if (!spec.isValid() || spec.preset() > m_Limit) {
        m_Error = "Choose a resolution within your account’s limit and a bitrate from 2 to 50 Mbps."; emit changed(); return;
    }
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
    StreamBridge::setModifierMapping(m_Modifiers);
    StreamBridge::setControlsHint(m_ShowControlsHint);
    StreamBridge::setMicrophonePreference(m_MicrophonePreference);
    m_SettingsGeneration = StreamBridge::beginSettingsSession();
    m_MicrophoneHost.clear();
    m_MicrophoneGeneration = StreamBridge::beginMicrophoneSession();
    m_Active = true; m_ProviderActive = true; m_Computer = nullptr;
    m_Clock.start(); emit elapsedChanged(); m_ElapsedTimer.start(); setStage("connecting", "Opening your provider session…");
    StreamBridge::setHostRequestsAllowed(true);
    record(QString("Requested %1×%2, %3 FPS, %4 Kbps; provider preset %5.").arg(spec.width).arg(spec.height).arg(spec.fps).arg(spec.bitrate).arg(spec.preset()));
    QMetaObject::invokeMethod(m_Worker, "start", Qt::QueuedConnection, Q_ARG(int, spec.width), Q_ARG(int, spec.height), Q_ARG(int, spec.fps), Q_ARG(int, spec.bitrate));
}

void Controller::setModifierMapping(int row, int target) {
    if (busy() || !m_Modifiers.assign(row, target)) return;
    QSettings().setValue("native/modifierMapping", modifierMappingValues(m_Modifiers));
    emit settingsChanged();
}

bool Controller::microphoneEnabled() const { return m_MicrophonePreference; }
QString Controller::microphoneStatus() const { return m_Microphone->status(); }
void Controller::setMicrophoneEnabled(bool enabled) {
    if (busy() && (m_Phase != "connected" || !m_Active || !StreamBridge::active()
        || !StreamBridge::hostRequestsAllowed() || StreamBridge::expectedEnd()
        || m_MicrophoneGeneration != StreamBridge::microphoneSessionGeneration())) return;
    const bool preferenceChanged = m_MicrophonePreference != enabled;
    if (preferenceChanged) {
        m_MicrophonePreference = enabled;
        QSettings().setValue("native/microphoneEnabled", enabled);
    }
    StreamBridge::setMicrophonePreference(enabled);
    StreamBridge::setMicrophoneAllowed(enabled && m_Phase == "connected" && m_Active, m_MicrophoneGeneration);
    const bool microphoneWillEmit = m_Microphone->enabled() != enabled;
    m_Microphone->setEnabled(enabled);
    if (preferenceChanged && !microphoneWillEmit) emit changed();
}
void Controller::persistMicrophonePreference(bool enabled, qulonglong settingsGeneration,
                                             qulonglong microphoneGeneration) {
    if (settingsGeneration != m_SettingsGeneration
        || settingsGeneration != StreamBridge::settingsSessionGeneration()) return;
    // Multiple rapid B presses may already have superseded this queued value.
    // Only the latest atomic choice may alter persistence or reopen capture.
    if (enabled != StreamBridge::microphonePreference()) return;
    if (m_MicrophonePreference != enabled) {
        m_MicrophonePreference = enabled;
        QSettings().setValue("native/microphoneEnabled", enabled);
        emit changed();
    }
    if (!enabled) {
        StreamBridge::setMicrophoneAllowed(false);
        m_Microphone->setEnabled(false);
    } else if (m_Active && m_Phase == "connected" && !StreamBridge::expectedEnd()
               && microphoneGeneration == m_MicrophoneGeneration
               && microphoneGeneration == StreamBridge::microphoneSessionGeneration()) {
        // A superseded mute may already have closed the send gate and caused
        // the worker to stop. Retire that attempt before reopening the gate.
        m_Microphone->setEnabled(false);
        StreamBridge::setMicrophoneAllowed(true, m_MicrophoneGeneration);
        m_Microphone->setEnabled(true);
    }
}

void Controller::persistStreamSetting(int setting, bool value, qulonglong generation) {
    if (generation != m_SettingsGeneration
        || generation != StreamBridge::settingsSessionGeneration()) return;
    const char* key = nullptr;
    bool* target = nullptr;
    switch (StreamBridge::StreamSetting(setting)) {
    case StreamBridge::Fullscreen: key = "native/fullscreen"; target = &m_Fullscreen; break;
    case StreamBridge::RelativeMouse: key = "native/relativeMouse"; target = &m_RelativeMouse; break;
    case StreamBridge::Stats: key = "native/showStats"; target = &m_ShowStats; break;
    case StreamBridge::ControlsHint: key = "native/showControlsHint"; target = &m_ShowControlsHint; break;
    default: return;
    }
    if (*target == value) return;
    *target = value; QSettings().setValue(key, value); emit settingsChanged();
}

void Controller::findHost(const QString& address) {
    if (!m_Active) return;
    m_MicrophoneHost = vmAddress(QJsonObject{{"vmip", address}});
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
    const QPointer<Session> session = m_Session;
    connect(m_Session, &Session::stageStarting, this, [this, session](const QString&) {
        if (session && session == m_Session && m_Active && !StreamBridge::expectedEnd()) setStage("video", "Starting the encrypted video and audio connection…");
    });
    connect(m_Session, &Session::stageFailed, this, [this, session](const QString&, int code, const QString&) {
        if (!session || session != m_Session || !m_Active || StreamBridge::expectedEnd()) return;
        m_Error = QString("Native streaming failed (error %1). Export diagnostics and check the dashboard.").arg(code); record(m_Error); emit changed();
    });
    connect(m_Session, &Session::nativeHostRequestFailed, this, [this, session](int code, bool transport) {
        if (!session || session != m_Session || !m_Active || StreamBridge::expectedEnd()) return;
        m_Error = transport
            ? QString("Could not reach the native host for Desktop launch/resume (network error %1). Check your connection and the dashboard.").arg(code)
            : QString("The native host rejected Desktop launch/resume (host error %1). Check the dashboard and export diagnostics before retrying.").arg(code);
        record(m_Error); emit changed();
    });
    connect(m_Session, &Session::displayLaunchError, this, [this, session](const QString&) {
        if (!session || session != m_Session || !m_Active || StreamBridge::expectedEnd()) return;
        if (!m_Error.isEmpty()) return;
        m_Error = "The native stream could not start. Check your settings and the dashboard."; record(m_Error); emit changed();
    });
    connect(m_Session, &Session::connectionStarted, this, [this, session] {
        if (!session || session != m_Session || !m_Active || StreamBridge::expectedEnd()
            || m_MicrophoneGeneration != StreamBridge::microphoneSessionGeneration()) return;
        m_Error.clear(); m_ElapsedTimer.stop();
        setStage("connected", "Video connected. Use Control + Command + Shift + P for local controls.");
        m_Microphone->setEnabled(m_MicrophonePreference);
        StreamBridge::setMicrophoneAllowed(m_MicrophonePreference, m_MicrophoneGeneration);
        m_Microphone->startSession(m_MicrophoneHost);
        if (m_Window) m_Window->hide();
    });
    connect(m_Session, &Session::sessionFinished, this, [this, session](int) {
        if (!session || session != m_Session) return;
        StreamBridge::setActive(false);
        m_Microphone->stopSession(); m_MicrophoneHost.clear();
        if (m_Window) { m_Window->show(); m_Window->requestActivate(); }
        QMetaObject::invokeMethod(m_Worker, "stopAfterStream", Qt::QueuedConnection);
        m_Active = false; m_StreamWarning.clear(); m_ElapsedTimer.stop();
        setStage("idle", "Disconnected.");
    });
    connect(m_Session, &Session::readyForDeletion, this, [this, session] {
        if (!session || session != m_Session) return;
        if (session) session->deleteLater();
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
    if (m_Session->property("legacy4KFallback").toBool()) {
        m_StreamWarning = "Stream advisory: this legacy NVIDIA host reduced the requested 4K setting to 1920×1080.";
        record(m_StreamWarning);
    } else if (!m_Session->property("launchWarnings").toStringList().isEmpty())
        m_StreamWarning = "Stream advisory: the engine reported a configuration warning. If video or input fails, try 720p / 60 FPS.";
    StreamBridge::setActive(true);
    m_Session->start();
}

void Controller::finish(const QString& message) {
    StreamBridge::setMicrophoneAllowed(false);
    m_Microphone->stopSession(); m_MicrophoneHost.clear();
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
