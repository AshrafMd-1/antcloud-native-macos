#include "antcloud/microphone.h"
#include "antcloud/microphonecapture.h"
#include "antcloud/streambridge.h"
#include <QtTest>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <cmath>
#include <thread>

namespace {
QMutex permissionMutex;
std::function<void(bool)> permissionCompletion;
std::atomic<int> permissionRequests{0}, captureStarts{0}, captureStops{0}, captureHealthChecks{0};
std::atomic<bool> captureAvailable{true}, captureHealthy{true};
void finishPermission(bool granted) {
    std::function<void(bool)> completion;
    { QMutexLocker lock(&permissionMutex); completion = std::move(permissionCompletion); }
    QVERIFY(bool(completion)); completion(granted);
}
}
namespace AntCloud::MicrophoneAudio {
void requestPermission(std::function<void(bool)> completion) {
    QMutexLocker lock(&permissionMutex); permissionCompletion = std::move(completion); ++permissionRequests;
}
class FakeCapture : public Capture {
public:
    bool start(CaptureQueue&, const std::atomic<bool>& cancelled, CaptureFormat& format) override {
        ++captureStarts;
        if (!captureAvailable || cancelled) return false;
        running = true; format = {1, 48000}; return true;
    }
    bool healthy() const override { ++captureHealthChecks; return captureHealthy; }
    void stop() override { if (running) { running = false; ++captureStops; } }
private:
    bool running = false;
};
std::unique_ptr<Capture> makeCapture() { return std::make_unique<FakeCapture>(); }
}
using namespace AntCloud;
using namespace AntCloud::MicrophoneAudio;
class MicrophoneTest : public QObject {
    Q_OBJECT
    QObject bridgeTarget;
private slots:
    void init() {
        permissionRequests = 0; captureStarts = 0; captureStops = 0; captureHealthChecks = 0;
        captureAvailable = true; captureHealthy = true;
        { QMutexLocker lock(&permissionMutex); permissionCompletion = {}; }
        StreamBridge::install(&bridgeTarget); StreamBridge::setActive(true);
        StreamBridge::setExpectedEnd(false); StreamBridge::setHostRequestsAllowed(true); StreamBridge::setMicrophoneAllowed(true);
    }
    void cleanup() {
        StreamBridge::setMicrophoneAllowed(false); StreamBridge::setActive(false); StreamBridge::install(nullptr);
        QMutexLocker lock(&permissionMutex); permissionCompletion = {};
    }
    void offByDefaultAndDeferredPermission() {
        Microphone mic;
        QVERIFY(!mic.enabled()); QCOMPARE(mic.status(), QString("Microphone off"));
        mic.setEnabled(true);
        QTest::qWait(20); QCOMPARE(permissionRequests.load(), 0); QCOMPARE(captureStarts.load(), 0);
        mic.startSession("192.0.2.1");
        QVERIFY(mic.enabled()); QTRY_COMPARE(permissionRequests.load(), 1);
        mic.stopSession(); QVERIFY(!mic.enabled());
        finishPermission(true); QTest::qWait(20); QCOMPARE(captureStarts.load(), 0);
    }
    void workerResourcesAreLazyAndHealthChecksAreBounded() {
        Microphone mic;
        mic.startSession("192.0.2.1");
        QTest::qWait(30);
        QCOMPARE(mic.findChildren<QThread*>().size(), 0);
        QCOMPARE(permissionRequests.load(), 0);
        QCOMPARE(captureHealthChecks.load(), 0);
        mic.setEnabled(true);
        QTRY_COMPARE(permissionRequests.load(), 1);
        QCOMPARE(mic.findChildren<QThread*>().size(), 1);
        finishPermission(true);
        QTRY_COMPARE(captureStarts.load(), 1);
        QTRY_VERIFY(mic.status().startsWith("Capturing"));
        captureHealthChecks = 0;
        QTest::qWait(350);
        QVERIFY2(captureHealthChecks.load() >= 1 && captureHealthChecks.load() <= 2,
                 "Device/permission checks must not run for every audio packet");
        mic.setEnabled(false);
        QCOMPARE(captureStops.load(), 1);
        QCOMPARE(mic.findChildren<QThread*>().size(), 0);
        const int healthChecks = captureHealthChecks.load();
        QTest::qWait(40);
        QCOMPARE(captureHealthChecks.load(), healthChecks);
        QCOMPARE(captureStarts.load(), 1);
        QCOMPARE(permissionRequests.load(), 1);
    }
    void permissionDenialAndCaptureFailureAreLocal() {
        Microphone mic;
        mic.setEnabled(true); mic.startSession("192.0.2.1");
        QTRY_COMPARE(permissionRequests.load(), 1); finishPermission(false);
        QTRY_VERIFY(!mic.enabled()); QVERIFY(mic.status().contains("denied"));
        QCOMPARE(captureStarts.load(), 0); QVERIFY(StreamBridge::active());
        captureAvailable = false; mic.setEnabled(true);
        QTRY_COMPARE(permissionRequests.load(), 2); finishPermission(true);
        QTRY_VERIFY(!mic.enabled()); QVERIFY(mic.status().contains("capture unavailable"));
        QVERIFY(StreamBridge::active());
    }
    void stopCancelsPendingPermissionAndRetainsOnlyCurrentSession() {
        Microphone mic;
        mic.setEnabled(true); mic.startSession("192.0.2.1");
        QTRY_COMPARE(permissionRequests.load(), 1);
        mic.setEnabled(false); finishPermission(true);
        QTest::qWait(20); QCOMPARE(captureStarts.load(), 0);
        mic.setEnabled(true); QTRY_COMPARE(permissionRequests.load(), 2); finishPermission(true);
        QTRY_COMPARE(captureStarts.load(), 1); QTRY_VERIFY(mic.status().startsWith("Capturing"));
        mic.setEnabled(false); QCOMPARE(captureStops.load(), 1);
        mic.setEnabled(true); QTRY_COMPARE(permissionRequests.load(), 3); finishPermission(true);
        QTRY_COMPARE(captureStarts.load(), 2);
        mic.stopSession(); QCOMPARE(captureStops.load(), 2); QVERIFY(!mic.enabled());
        mic.startSession("192.0.2.2"); QTest::qWait(20); QCOMPARE(permissionRequests.load(), 3);
    }
    void destinationValidationAndDeviceChange() {
        Microphone mic;
        mic.setEnabled(true); mic.startSession("example.com");
        QVERIFY(!mic.enabled()); QCOMPARE(permissionRequests.load(), 0);
        mic.setEnabled(true); mic.startSession("192.0.2.1");
        QTRY_COMPARE(permissionRequests.load(), 1); finishPermission(true);
        QTRY_COMPARE(captureStarts.load(), 1);
        captureHealthy = false; QTRY_VERIFY(!mic.enabled());
        QCOMPARE(captureStops.load(), 1); QVERIFY(mic.status().contains("input changed")); QVERIFY(StreamBridge::active());
    }
    void planarInterleavedAndBoundedQueue() {
        CaptureQueue queue;
        std::array<float, 3> left{1, 2, 3}, right{4, 5, 6};
        std::array<float*, 2> planes{left.data(), right.data()};
        for (unsigned index = 0; index < QueueSlots - 1; ++index) QVERIFY(queue.push(planes.data(), 1, 2, 3, 100 + index));
        QVERIFY(!queue.push(planes.data(), 1, 2, 3, 999)); QCOMPARE(queue.dropped(), uint64_t(1));
        Block block;
        for (unsigned index = 0; index < QueueSlots - 1; ++index) {
            QVERIFY(queue.pop(block)); QCOMPARE(block.capturedAt, uint64_t(100 + index));
            QCOMPARE(block.frames, 3); QCOMPARE(block.channels, 2);
            const std::array<float, 6> expected{1, 4, 2, 5, 3, 6};
            QVERIFY(std::equal(expected.begin(), expected.end(), block.samples.begin()));
        }
        QVERIFY(!queue.pop(block));
        std::array<float, 6> interleaved{1, 4, 2, 5, 3, 6};
        planes = {interleaved.data(), interleaved.data() + 1};
        QVERIFY(queue.push(planes.data(), 2, 2, 3, 200)); QVERIFY(queue.pop(block));
        QVERIFY(std::equal(interleaved.begin(), interleaved.end(), block.samples.begin()));
        QVERIFY(fresh(block, 300)); QVERIFY(!fresh(block, 301)); QVERIFY(!fresh(block, 199));
        QVERIFY(!queue.push(nullptr, 1, 2, 3, 0)); QVERIFY(!queue.push(planes.data(), 1, 2, BlockFrames + 1, 0));
    }
    void queueCopiesOnlyPopulatedAudio() {
        CaptureQueue queue;
        std::array<float, 3> samples{0.25f, -0.5f, 1.0f};
        float* plane = samples.data();
        QVERIFY(queue.push(&plane, 1, 1, samples.size(), 100));
        Block output;
        output.samples.fill(17.0f);
        QVERIFY(queue.pop(output));
        QCOMPARE(output.frames, 3); QCOMPARE(output.channels, 1); QCOMPARE(output.capturedAt, uint64_t(100));
        QVERIFY(std::equal(samples.begin(), samples.end(), output.samples.begin()));
        QVERIFY(std::all_of(output.samples.begin() + samples.size(), output.samples.end(),
                            [](float sample) { return sample == 17.0f; }));
    }
    void concurrentQueueKeepsWholeOrderedBlocks() {
        CaptureQueue queue;
        std::atomic<bool> done{false};
        std::thread producer([&] {
            std::array<float, 32> samples{}; float* plane = samples.data();
            for (int value = 1; value <= 5000; ++value) { samples.fill(float(value)); queue.push(&plane, 1, 1, samples.size(), value); }
            done = true;
        });
        Block block; uint64_t previous = 0; int received = 0; bool valid = true;
        while (true) {
            if (!queue.pop(block)) { if (done) break; std::this_thread::yield(); continue; }
            if (block.capturedAt <= previous || block.frames != 32 || block.channels != 1) valid = false;
            for (int index = 0; index < block.frames; ++index) if (block.samples[index] != float(block.capturedAt)) valid = false;
            previous = block.capturedAt; ++received;
        }
        producer.join(); QVERIFY(valid); QVERIFY(received > 0);
    }
    void littleEndianPcmAndResampleDownmix() {
        Converter converter;
        QVERIFY(converter.configure(AUDIO_S16LSB, 1, SampleRate));
        const QByteArray pcm = QByteArray::fromHex("00000100ffffff7f0080");
        QVERIFY(converter.put(pcm.constData(), pcm.size()));
        QCOMPARE(converter.available(), pcm.size()); QByteArray output(pcm.size(), '\0');
        QCOMPARE(converter.read(output.data(), output.size()), pcm.size()); QCOMPARE(output, pcm);
        QVERIFY(converter.configure(AUDIO_F32SYS, 2, 48000));
        std::array<float, 960> input{};
        for (int chunk = 0; chunk < 100; ++chunk) {
            for (int frame = 0; frame < 480; ++frame) {
                const float value = 0.5f * std::sin(2.0 * 3.141592653589793 * 1000 * (chunk * 480 + frame) / 48000);
                input[frame * 2] = value; input[frame * 2 + 1] = value;
            }
            QVERIFY(converter.put(input.data(), sizeof(input)));
        }
        QVERIFY(converter.flush()); const int bytes = converter.available();
        QVERIFY(std::abs(bytes - SampleRate * 2) <= 4); QVERIFY(bytes % 2 == 0);
        output.resize(bytes); QCOMPARE(converter.read(output.data(), bytes), bytes);
        int crossings = 0, peak = 0, previous = 0;
        for (int index = 0; index < bytes; index += 2) {
            const int sample = qint16(quint16(quint8(output[index])) | (quint16(quint8(output[index + 1])) << 8));
            if (previous <= 0 && sample > 0) ++crossings;
            peak = std::max(peak, std::abs(sample)); previous = sample;
        }
        QVERIFY(std::abs(crossings - 1000) <= 2); QVERIFY(peak > 14000 && peak < 18000);
        converter.clear(); QCOMPARE(converter.available(), 0);
    }
    void udpRawPacketsPortAndStopGuard() {
        QUdpSocket receiver;
        QVERIFY2(receiver.bind(QHostAddress::LocalHost, Port, QUdpSocket::DontShareAddress), "Loopback UDP9002 must be free for the isolated microphone fixture");
        Sender sender(QHostAddress::LocalHost); QVERIFY(sender.open());
        QByteArray pcm(PacketBytes, '\0'); for (int index = 0; index < pcm.size(); ++index) pcm[index] = char(index % 256);
        std::atomic<bool> cancelled{false}; QVERIFY(sender.send(pcm.constData(), pcm.size(), cancelled));
        QTRY_VERIFY(receiver.hasPendingDatagrams());
        QCOMPARE(receiver.pendingDatagramSize(), qint64(PacketBytes)); QByteArray received(PacketBytes, '\0');
        QHostAddress source; quint16 sourcePort = 0;
        QCOMPARE(receiver.readDatagram(received.data(), received.size(), &source, &sourcePort), qint64(PacketBytes));
        QCOMPARE(received, pcm); QVERIFY(source.isLoopback()); QVERIFY(sourcePort != 0 && sourcePort != Port);
        QVERIFY(!sender.send(pcm.constData(), 1, cancelled)); QVERIFY(!sender.send(pcm.constData(), PacketBytes + 2, cancelled));
        cancelled = true; QVERIFY(!sender.send(pcm.constData(), pcm.size(), cancelled));
        cancelled = false; StreamBridge::setMicrophoneAllowed(false); QVERIFY(!sender.send(pcm.constData(), pcm.size(), cancelled));
        QTest::qWait(20); QVERIFY(!receiver.hasPendingDatagrams());
    }
    void partialPcmCannotSurviveCaptureGap() {
        Converter converter;
        QVERIFY(converter.configure(AUDIO_F32SYS, 1, SampleRate));
        Block old;
        old.frames = 200; old.channels = 1; old.capturedAt = 100;
        old.samples.fill(0.25f);
        QVERIFY(converter.putCaptured(old, 100)); QCOMPARE(converter.available(), 400);
        converter.expire(200); QCOMPARE(converter.available(), 400);
        converter.expire(201); QCOMPARE(converter.available(), 0);
        // This path also covers a stalled worker with no intervening timer tick.
        QVERIFY(converter.putCaptured(old, 100)); QCOMPARE(converter.available(), 400);
        Block current;
        current.frames = PacketBytes / 2; current.channels = 1; current.capturedAt = 301;
        current.samples.fill(-0.5f);
        QVERIFY(converter.putCaptured(current, 301)); QCOMPARE(converter.available(), PacketBytes);
        QByteArray pcm(PacketBytes, '\0'); QCOMPARE(converter.read(pcm.data(), pcm.size()), PacketBytes);
        for (int index = 0; index < pcm.size(); index += 2) {
            const qint16 sample = qint16(quint16(quint8(pcm[index])) | (quint16(quint8(pcm[index + 1])) << 8));
            QVERIFY(sample < -15000 && sample > -18000);
        }
        QVERIFY(converter.putCaptured(old, 1000)); QCOMPARE(converter.available(), 0);
    }
};
QTEST_GUILESS_MAIN(MicrophoneTest)
#include "microphone_test.moc"
