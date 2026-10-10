#include "microphone.h"
#include "microphonecapture.h"
#include "protocol.h"
#include "streambridge.h"
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

namespace AntCloud {
using namespace MicrophoneAudio;
class MicrophoneWorker;
struct PermissionTicket {
    QMutex mutex;
    MicrophoneWorker* receiver = nullptr;
};

class MicrophoneWorker : public QObject {
    Q_OBJECT
public:
    MicrophoneWorker(const QString& host, std::shared_ptr<std::atomic<bool>> cancelled)
        : m_Host(host), m_Cancelled(std::move(cancelled)), m_Permission(std::make_shared<PermissionTicket>()) {
        m_Permission->receiver = this;
    }
    void start() {
        if (!allowed()) return;
        const auto ticket = m_Permission;
        requestPermission([ticket](bool granted) {
            // The system prompt may complete after disconnect or object deletion.
            QMutexLocker lock(&ticket->mutex);
            if (ticket->receiver)
                QMetaObject::invokeMethod(ticket->receiver, [receiver = ticket->receiver, granted] {
                    receiver->permissionReady(granted);
                }, Qt::QueuedConnection);
        });
    }
    void stop() {
        m_Cancelled->store(true, std::memory_order_release);
        { QMutexLocker lock(&m_Permission->mutex); m_Permission->receiver = nullptr; }
        if (m_Timer) m_Timer->stop();
        if (m_Capture) { m_Capture->stop(); m_Capture.reset(); }
        m_Sender.reset(); m_Converter.reset();
        while (m_Queue.pop(m_Block)) {}
    }
signals:
    void capturing();
    void sending();
    void failed(const QString& message);
private:
    bool allowed() const { return !m_Cancelled->load(std::memory_order_acquire) && StreamBridge::microphoneAllowed(); }
    void fail(const QString& message) { stop(); emit failed(message); }
    void permissionReady(bool granted) {
        if (!allowed()) { stop(); return; }
        if (!granted) { fail("Microphone access denied. Enable it in macOS Privacy & Security → Microphone, then try again."); return; }
        m_Capture = makeCapture();
        CaptureFormat format;
        if (!m_Capture || !m_Capture->start(m_Queue, *m_Cancelled, format)) {
            fail("Microphone capture unavailable. Check the Mac input device, then try again."); return;
        }
        if (!allowed()) { stop(); return; }
        m_Converter = std::make_unique<Converter>();
        if (!m_Converter->configure(AUDIO_F32SYS, format.channels, format.rate)) {
            fail("Microphone audio conversion failed. Try another Mac input device."); return;
        }
        m_Sender = std::make_unique<Sender>(QHostAddress(m_Host));
        if (!m_Sender->open()) { fail("Microphone network setup failed. Try again."); return; }
        m_Timer = new QTimer(this);
        // Each wire packet contains 10 ms of audio. Poll device/permission state
        // separately so system queries do not compete with every packet.
        m_Timer->setTimerType(Qt::PreciseTimer); m_Timer->setInterval(10);
        connect(m_Timer, &QTimer::timeout, this, &MicrophoneWorker::pump);
        m_Timer->start();
        emit capturing();
    }
    void pump() {
        if (!allowed()) { stop(); return; }
        const uint64_t pumpTime = nowMs();
        if (pumpTime >= m_NextHealthCheck) {
            if (!m_Capture->healthy()) { fail("Microphone input changed or became unavailable. Select a Mac input device and enable microphone again."); return; }
            m_NextHealthCheck = pumpTime + 250;
        }
        // Partial packets and resampler history age with capture, not just the
        // queue. A delayed device/worker must not prepend an old voice fragment.
        m_Converter->expire(pumpTime);
        if (m_Dropped != m_Queue.dropped()) { m_Dropped = m_Queue.dropped(); m_Converter->clear(); }
        for (unsigned count = 0; count < QueueSlots && m_Queue.pop(m_Block); ++count) {
            if (!allowed()) { stop(); return; }
            const uint64_t now = nowMs();
            if (!fresh(m_Block, now)) { m_Converter->clear(); continue; }
            if (!m_Converter->putCaptured(m_Block, now)) {
                fail("Microphone audio conversion failed. Try another Mac input device."); return;
            }
            int available = m_Converter->available();
            if (available < 0) { fail("Microphone audio conversion failed. Try another Mac input device."); return; }
            while (available >= PacketBytes) {
                if (!allowed()) { stop(); return; }
                if (m_Converter->read(m_Packet.data(), PacketBytes) != PacketBytes) {
                    fail("Microphone audio conversion failed. Try another Mac input device."); return;
                }
                if (!m_Sender->send(m_Packet.data(), PacketBytes, *m_Cancelled)) {
                    if (!allowed()) { stop(); return; }
                    fail("Microphone UDP send failed. Check the connection and try again."); return;
                }
                if (!m_Sent) { m_Sent = true; emit sending(); }
                available = m_Converter->available();
                if (available < 0) { fail("Microphone audio conversion failed. Try another Mac input device."); return; }
            }
        }
    }
    QString m_Host;
    std::shared_ptr<std::atomic<bool>> m_Cancelled;
    std::shared_ptr<PermissionTicket> m_Permission;
    CaptureQueue m_Queue;
    Block m_Block;
    std::array<char, PacketBytes> m_Packet{};
    std::unique_ptr<Capture> m_Capture;
    std::unique_ptr<Converter> m_Converter;
    std::unique_ptr<Sender> m_Sender;
    QTimer* m_Timer = nullptr;
    uint64_t m_Dropped = 0, m_NextHealthCheck = 0;
    bool m_Sent = false;
};

class Microphone::Private {
public:
    explicit Private(Microphone* owner) : q(owner) {}
    void stopWorker() {
        ++generation;
        if (cancelled) cancelled->store(true, std::memory_order_release);
        if (thread && thread->isRunning()) {
            QMetaObject::invokeMethod(worker, [receiver = worker] { receiver->stop(); }, Qt::BlockingQueuedConnection);
            thread->quit(); thread->wait();
        }
        thread.reset(); worker = nullptr; cancelled.reset();
    }
    void begin() {
        if (thread || host.isEmpty() || !enabled) return;
        const quint64 current = ++generation;
        cancelled = std::make_shared<std::atomic<bool>>(false);
        thread = std::make_unique<QThread>(q);
        worker = new MicrophoneWorker(host, cancelled);
        worker->moveToThread(thread.get());
        QObject::connect(thread.get(), &QThread::started, worker, &MicrophoneWorker::start);
        QObject::connect(thread.get(), &QThread::finished, worker, &QObject::deleteLater);
        QObject::connect(worker, &MicrophoneWorker::capturing, q, [this, current] {
            if (current != generation || !enabled) return;
            status = "Capturing microphone"; emit q->changed();
        });
        QObject::connect(worker, &MicrophoneWorker::sending, q, [this, current] {
            if (current != generation || !enabled) return;
            status = "Sending microphone"; emit q->changed();
        });
        QObject::connect(worker, &MicrophoneWorker::failed, q, [this, current](const QString& message) {
            if (current != generation) return;
            stopWorker(); enabled = false; status = message; emit q->changed();
        });
        status = "Requesting microphone permission…";
        thread->start();
    }
    Microphone* q;
    bool enabled = false;
    QString status = "Microphone off", host;
    quint64 generation = 0;
    std::unique_ptr<QThread> thread;
    MicrophoneWorker* worker = nullptr;
    std::shared_ptr<std::atomic<bool>> cancelled;
};

Microphone::Microphone(QObject* parent) : QObject(parent), d(std::make_unique<Private>(this)) {}
Microphone::~Microphone() { d->stopWorker(); }
bool Microphone::enabled() const { return d->enabled; }
QString Microphone::status() const { return d->status; }
void Microphone::setEnabled(bool value) {
    if (d->enabled == value) return;
    d->enabled = value;
    if (value) {
        d->status.clear(); d->begin();
    } else { d->stopWorker(); d->status = "Microphone off"; }
    emit changed();
}
void Microphone::startSession(const QString& host) {
    d->stopWorker();
    d->host = vmAddress(QJsonObject{{"vmip", host}});
    if (d->host.isEmpty()) {
        d->enabled = false; d->status = "Microphone destination unavailable.";
    } else if (d->enabled) d->begin();
    emit changed();
}
void Microphone::stopSession() {
    d->stopWorker(); d->host.clear(); d->enabled = false; d->status = "Microphone off";
    emit changed();
}
}
#include "microphone.moc"
