#pragma once

#include <QObject>

class ComputerManager;
class NvComputer;
class QTimer;

class ComputerSeeker : public QObject
{
    Q_OBJECT
public:
    explicit ComputerSeeker(ComputerManager *manager, QString computerName, QObject *parent = nullptr);
    ~ComputerSeeker() override;

    void start(int timeout, bool retryFailedAdds = false);

signals:
    void computerFound(NvComputer *computer);
    void errorTimeout();

private slots:
    void onComputerUpdated(NvComputer *computer);
    void onTimeout();
    void tryAddHost();
    void stop();

private:
    bool matchComputer(NvComputer *computer) const;
    NvComputer* findMatchingComputer() const;
    bool isOnline(NvComputer *computer) const;

private:
    ComputerManager *m_ComputerManager;
    QString m_ComputerName;
    QTimer *m_TimeoutTimer;
    QTimer *m_RetryTimer;
    bool m_Polling = false, m_AddPending = false, m_RetryFailedAdds = false;
};
