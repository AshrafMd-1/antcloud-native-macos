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
    Q_PROPERTY(int resolutionIndex MEMBER m_ResolutionIndex NOTIFY settingsChanged)
    Q_PROPERTY(int fpsIndex MEMBER m_FpsIndex NOTIFY settingsChanged)
    Q_PROPERTY(int bitrate MEMBER m_Bitrate NOTIFY settingsChanged)
    Q_PROPERTY(bool fullscreen MEMBER m_Fullscreen NOTIFY settingsChanged)
    Q_PROPERTY(bool relativeMouse MEMBER m_RelativeMouse NOTIFY settingsChanged)
    Q_PROPERTY(bool showStats MEMBER m_ShowStats NOTIFY settingsChanged)
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
    Q_INVOKABLE void initialize(QQuickWindow* window);
    Q_INVOKABLE void login(QString email, QString password, bool remember);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void start();
    Q_INVOKABLE void disconnectStream();
    Q_INVOKABLE void shutdown();
    Q_INVOKABLE void exportDiagnostics(QUrl file);
    Q_INVOKABLE void clearDiagnostics();
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
    ComputerManager* m_Manager = nullptr;
    QPointer<ComputerSeeker> m_Seeker;
    QPointer<Session> m_Session;
    QPointer<QQuickWindow> m_Window;
    StreamingPreferences* m_Preferences = nullptr;
    NvComputer* m_Computer = nullptr;
    bool m_SignedIn = false, m_Active = false, m_ProviderActive = false, m_Pairing = false, m_Polling = false;
    bool m_Fullscreen = true, m_RelativeMouse = true, m_ShowStats = true;
    int m_Limit = 0, m_ResolutionIndex = 0, m_FpsIndex = 1, m_Bitrate = 6000;
    QString m_Phase = "idle", m_Detail = "Sign in to connect to your Windows VM.", m_Error, m_Notice, m_StreamWarning, m_ShutdownStatus;
    QStringList m_Diagnostics;
    QElapsedTimer m_Clock;
    QTimer m_ElapsedTimer, m_AppTimeout;
};
}
