#include "computerseeker.h"
#include "computermanager.h"
#include <QTimer>

ComputerSeeker::ComputerSeeker(ComputerManager *manager, QString computerName, QObject *parent)
    : QObject(parent), m_ComputerManager(manager), m_ComputerName(computerName),
      m_TimeoutTimer(new QTimer(this)), m_RetryTimer(new QTimer(this))
{
    // If we know this computer, send a WOL packet to wake it up in case it is asleep.
    NvComputer* matchingComputer = findMatchingComputer();
    if (matchingComputer) {
        matchingComputer->wake();
    }

    m_TimeoutTimer->setSingleShot(true);
    connect(m_TimeoutTimer, &QTimer::timeout,
            this, &ComputerSeeker::onTimeout);
    connect(m_ComputerManager, &ComputerManager::computerStateChanged,
            this, &ComputerSeeker::onComputerUpdated);
    connect(m_ComputerManager, &ComputerManager::computerPolled,
            this, &ComputerSeeker::onComputerUpdated);
    m_RetryTimer->setSingleShot(true);
    m_RetryTimer->setInterval(2000);
    connect(m_RetryTimer, &QTimer::timeout, this, &ComputerSeeker::tryAddHost);
    connect(m_ComputerManager, &ComputerManager::computerAddCompleted, this,
            [this](QVariant success, QVariant) {
        if (!m_AddPending) return;
        m_AddPending = false;
        if (!success.toBool() && m_RetryFailedAdds && m_TimeoutTimer->isActive())
            m_RetryTimer->start();
    });
}

void ComputerSeeker::start(int timeout, bool retryFailedAdds)
{
    if (m_Polling) return;
    m_RetryFailedAdds = retryFailedAdds;
    m_TimeoutTimer->start(timeout);
    m_ComputerManager->startPolling();
    m_Polling = true;

    // If we don't know this computer by name, address, or UUID, try adding it
    // manually and see if we can find it by address, hostname, or mDNS.
    //
    // NB: We don't do this unconditionally because it will wipe out the user's
    // manual address if they pass another reachable hostname/address.
    if (!findMatchingComputer()) {
        tryAddHost();
    }
}

ComputerSeeker::~ComputerSeeker()
{
    stop();
}

void ComputerSeeker::tryAddHost()
{
    if (!m_TimeoutTimer->isActive() || m_AddPending || findMatchingComputer()) return;
    m_AddPending = true;
    m_ComputerManager->addNewHostManually(m_ComputerName);
}

void ComputerSeeker::stop()
{
    m_TimeoutTimer->stop();
    m_RetryTimer->stop();
    if (m_Polling) {
        m_ComputerManager->stopPollingAsync();
        m_Polling = false;
    }
}

void ComputerSeeker::onComputerUpdated(NvComputer *computer)
{
    if (!m_TimeoutTimer->isActive()) {
        return;
    }
    if (matchComputer(computer) && isOnline(computer)) {
        stop();
        emit computerFound(computer);
    }
}

bool ComputerSeeker::matchComputer(NvComputer *computer) const
{
    QString value = m_ComputerName.toLower();

    if (computer->name.toLower() == value || computer->uuid.toLower() == value) {
        return true;
    }

    const auto uniqueAddresses = computer->uniqueAddresses();
    for (const NvAddress& addr : uniqueAddresses) {
        if (addr.address().toLower() == value || addr.toString().toLower() == value) {
            return true;
        }
    }

    return false;
}

NvComputer* ComputerSeeker::findMatchingComputer() const
{
    const auto computers = m_ComputerManager->getComputers();
    for (NvComputer* computer : computers) {
        if (this->matchComputer(computer)) {
            return computer;
        }
    }

    return nullptr;
}

bool ComputerSeeker::isOnline(NvComputer *computer) const
{
    return computer->state == NvComputer::CS_ONLINE;
}

void ComputerSeeker::onTimeout()
{
    stop();
    emit errorTimeout();
}
