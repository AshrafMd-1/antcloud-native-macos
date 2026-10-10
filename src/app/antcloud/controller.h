#pragma once
#include "providerworker.h"
#include <QThread>
#include <QElapsedTimer>
#include <QPointer>
#include <QQuickWindow>

class ComputerManager;
class ComputerSeeker;
class NvComputer;
class Session;
class StreamingPreferences;

namespace AntCloud {
class Microphone;
class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(QString streamWarning READ streamWarning NOTIFY changed)
    Q_PROPERTY(QString shutdownStatus READ shutdownStatus NOTIFY changed)
    Q_PROPERTY(int elapsed READ elapsed NOTIFY elapsedChanged)
    Q_PROPERTY(int resolutionLimit READ resolutionLimit NOTIFY changed)
    Q_PROPERTY(int resolutionIndex READ resolutionIndex WRITE setResolutionIndex NOTIFY settingsChanged)
    Q_PROPERTY(int fpsIndex READ fpsIndex WRITE setFpsIndex NOTIFY settingsChanged)
    Q_PROPERTY(int bitrate READ bitrate WRITE setBitrate NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList bitrateChoices READ bitrateChoices CONSTANT)
    Q_PROPERTY(int codecIndex READ codecIndex WRITE setCodecIndex NOTIFY settingsChanged)
    Q_PROPERTY(int remainingMinutes READ remainingMinutes NOTIFY changed)
    Q_PROPERTY(QString remainingTime READ remainingTime NOTIFY changed)
    Q_PROPERTY(int commandModifier READ commandModifier NOTIFY settingsChanged)
    Q_PROPERTY(int optionModifier READ optionModifier NOTIFY settingsChanged)
    Q_PROPERTY(int controlModifier READ controlModifier NOTIFY settingsChanged)
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY settingsChanged)
    Q_PROPERTY(bool relativeMouse READ relativeMouse WRITE setRelativeMouse NOTIFY settingsChanged)
    Q_PROPERTY(bool showStats READ showStats WRITE setShowStats NOTIFY settingsChanged)
    Q_PROPERTY(bool showControlsHint READ showControlsHint WRITE setShowControlsHint NOTIFY settingsChanged)
    Q_PROPERTY(bool microphoneEnabled READ microphoneEnabled NOTIFY changed)
    Q_PROPERTY(QString microphoneStatus READ microphoneStatus NOTIFY changed)
public:
    explicit Controller(QObject* parent = nullptr);
    ~Controller() override;
    bool signedIn() const { return m_SignedIn; }
    bool active() const { return m_Active; }
    bool busy() const { return m_Active || m_ProviderActive || m_Pairing || !m_Session.isNull() || m_Phase == "authenticating"; }
    QString phase() const { return m_Phase; }
    QString detail() const { return m_Detail; }
    QString error() const { return m_Error; }
    QString notice() const { return m_Notice; }
    QString streamWarning() const { return m_StreamWarning; }
    QString shutdownStatus() const { return m_ShutdownStatus; }
    int elapsed() const { return m_Clock.isValid() ? int(m_Clock.elapsed() / 1000) : 0; }
    int resolutionLimit() const { return m_Limit; }
    int remainingMinutes() const { return m_RemainingMinutes; }
    QString remainingTime() const;
    int resolutionIndex() const { return m_ResolutionIndex; }
    int fpsIndex() const { return m_FpsIndex; }
    int bitrate() const { return m_Bitrate; }
    int codecIndex() const { return m_CodecIndex; }
    bool fullscreen() const { return m_Fullscreen; }
    bool relativeMouse() const { return m_RelativeMouse; }
    bool showStats() const { return m_ShowStats; }
    bool showControlsHint() const { return m_ShowControlsHint; }
    int commandModifier() const { return m_Modifiers.target(0); }
    int optionModifier() const { return m_Modifiers.target(1); }
    int controlModifier() const { return m_Modifiers.target(2); }
    bool microphoneEnabled() const;
    QString microphoneStatus() const;
    Q_INVOKABLE void setMicrophoneEnabled(bool enabled);
    Q_INVOKABLE void persistMicrophonePreference(bool enabled, qulonglong settingsGeneration,
                                                 qulonglong microphoneGeneration);
    Q_INVOKABLE void persistStreamSetting(int setting, bool value, qulonglong generation);
    void setResolutionIndex(int value);
    void setFpsIndex(int value);
    void setBitrate(int value);
    void setCodecIndex(int value);
    void setFullscreen(bool value);
    void setRelativeMouse(bool value);
    void setShowStats(bool value);
    void setShowControlsHint(bool value);
    Q_INVOKABLE void setModifierMapping(int row, int target);
    QVariantList bitrateChoices() const {
        QVariantList values;
        for (int value : BitrateChoices) values.append(value);
        return values;
    }
    Q_INVOKABLE void initialize(QQuickWindow* window);
    Q_INVOKABLE void login(QString email, QString password, bool remember);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void start();
    Q_INVOKABLE void disconnectStream();
    Q_INVOKABLE void shutdown();
    Q_INVOKABLE void exportDiagnostics(QUrl file);
    Q_INVOKABLE void clearDiagnostics();
#ifdef ANTCLOUD_TESTING
    void beginSettingsSessionForTest();
#endif
signals:
    void changed();
    void settingsChanged();
    void elapsedChanged();
private:
    void setStage(const QString& phase, const QString& detail);
    void findHost(const QString& address);
    void hostFound(NvComputer* computer);
    void launchIfReady();
    void finish(const QString& message);
    void stopPolling();
    void record(const QString& value);
    QThread m_Thread;
    ProviderWorker* m_Worker;
    Microphone* m_Microphone;
    QString m_MicrophoneHost;
    qulonglong m_MicrophoneGeneration = 0;
    qulonglong m_SettingsGeneration = 0;
    ComputerManager* m_Manager = nullptr;
    QPointer<ComputerSeeker> m_Seeker;
    QPointer<Session> m_Session;
    QPointer<QQuickWindow> m_Window;
    StreamingPreferences* m_Preferences = nullptr;
    NvComputer* m_Computer = nullptr;
    bool m_SignedIn = false, m_Active = false, m_ProviderActive = false, m_Pairing = false, m_Polling = false;
    bool m_Fullscreen = true, m_RelativeMouse = true, m_ShowStats = true, m_ShowControlsHint = true;
    int m_Limit = 0, m_ResolutionIndex = DefaultResolutionIndex, m_FpsIndex = 1, m_Bitrate = 10000, m_CodecIndex = 0, m_RemainingMinutes = -1;
    bool m_MicrophonePreference = false;
    ModifierMapping m_Modifiers;
    QString m_Phase = "idle", m_Detail = "Sign in to connect to your Windows VM.", m_Error, m_Notice, m_StreamWarning, m_ShutdownStatus;
    QStringList m_Diagnostics;
    QElapsedTimer m_Clock;
    QTimer m_ElapsedTimer, m_AppTimeout;
};
}
