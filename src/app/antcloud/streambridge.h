// Process-local control channel. Account tokens and passwords stay on the worker.
#pragma once
#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <atomic>
#include "SDL_compat.h"
#include "modifiermapping.h"

namespace AntCloud {
class StreamBridge {
public:
    static void install(QObject* worker) { QMutexLocker lock(&mutex()); target() = worker; enabledFlag() = worker != nullptr; if (!worker) setMicrophoneAllowed(false); }
    static bool enabled() { return enabledFlag().load(); }
    // Set before queuing Stop so provider events cannot race the local SDL exit.
    static void requestStop() {
        QMutexLocker lock(&mutex());
        if (!target()) return;
        setMicrophoneAllowed(false);
        expectedEndFlag() = true;
        QMetaObject::invokeMethod(target(), "stop", Qt::QueuedConnection);
    }
    static void setExpectedEnd(bool value) { if (value) setMicrophoneAllowed(false); expectedEndFlag() = value; }
    static bool expectedEnd() { return expectedEndFlag().load(); }
    static void setHostRequestsAllowed(bool value) { if (!value) setMicrophoneAllowed(false); allowedFlag() = value; }
    static void setClosing() { setMicrophoneAllowed(false); closingFlag() = true; }
    static bool hostRequestsAllowed() { return !closingFlag().load() && (!enabled() || allowedFlag().load()); }
    static bool shutdown() {
        QMutexLocker lock(&mutex());
        return target() && QMetaObject::invokeMethod(target(), "shutdown", Qt::QueuedConnection);
    }
    static void setPairingGeneration(qulonglong value) { pairingGenerationFlag() = value; }
    static qulonglong pairingGeneration() { return pairingGenerationFlag().load(); }
    static void pair(const QString& pin, qulonglong generation) {
        QMutexLocker lock(&mutex());
        if (target() && generation != 0 && generation == pairingGeneration() && hostRequestsAllowed())
            QMetaObject::invokeMethod(target(), "pair", Qt::QueuedConnection,
                Q_ARG(QString, pin), Q_ARG(qulonglong, generation));
    }
    // Only validated provider minutes cross into the decoder; no credentials or GUI access.
    static void setRemainingMinutes(int value) { remainingMinutesFlag() = value >= 0 ? value : -1; }
    static int remainingMinutes() { return remainingMinutesFlag().load(); }
    static void setModifierMapping(const ModifierMapping& mapping) { modifierMappingFlag() = mapping.packed(); }
    static ModifierMapping modifierMapping() { return ModifierMapping::fromPacked(modifierMappingFlag().load()); }
    static void setControlsHint(bool value) { controlsHintFlag() = value; }
    static bool controlsHint() { return controlsHintFlag().load(); }
    static void setActive(bool value) { if (!value) setMicrophoneAllowed(false); activeFlag() = value; }
    static bool active() { return activeFlag().load(); }
    static void installMicrophoneController(QObject* controller) {
        QMutexLocker lock(&mutex()); microphoneTarget() = controller;
        if (!controller) setMicrophoneAllowed(false);
    }
    static qulonglong beginMicrophoneSession() { setMicrophoneAllowed(false); return ++microphoneGeneration(); }
    static qulonglong microphoneSessionGeneration() { return microphoneGeneration().load(); }
    static void invalidateMicrophoneSession() { setMicrophoneAllowed(false); ++microphoneGeneration(); }
    static void setMicrophoneAllowed(bool value, qulonglong generation) { microphoneStamp() = value ? generation : 0; }
    static void setMicrophoneAllowed(bool value) { setMicrophoneAllowed(value, microphoneSessionGeneration()); }
    static bool microphoneAllowed() {
        const auto stamp = microphoneStamp().load();
        return stamp != 0 && stamp == microphoneSessionGeneration()
            && enabled() && active() && hostRequestsAllowed() && !expectedEnd();
    }
    static bool toggleMicrophone() {
        QMutexLocker lock(&mutex());
        if (!microphoneTarget() || !enabled() || !active() || expectedEnd() || !hostRequestsAllowed()) return false;
        // Muting blocks sends immediately, before the GUI processes its queued toggle.
        if (microphoneAllowed()) setMicrophoneAllowed(false);
        return QMetaObject::invokeMethod(microphoneTarget(), "toggleMicrophone", Qt::QueuedConnection,
            Q_ARG(qulonglong, microphoneGeneration().load()));
    }
    static void interrupt() {
        if (!active()) return;
        SDL_Event event{}; event.type = SDL_QUIT; SDL_PushEvent(&event);
    }
private:
    static QObject*& target() { static QObject* value = nullptr; return value; }
    static QObject*& microphoneTarget() { static QObject* value = nullptr; return value; }
    static QMutex& mutex() { static QMutex value; return value; }
    static std::atomic<bool>& enabledFlag() { static std::atomic<bool> value{false}; return value; }
    static std::atomic<int>& remainingMinutesFlag() { static std::atomic<int> value{-1}; return value; }
    static std::atomic<unsigned>& modifierMappingFlag() { static std::atomic<unsigned> value{ModifierMapping{}.packed()}; return value; }
    static std::atomic<bool>& controlsHintFlag() { static std::atomic<bool> value{true}; return value; }
    static std::atomic<qulonglong>& pairingGenerationFlag() { static std::atomic<qulonglong> value{0}; return value; }
    static std::atomic<bool>& activeFlag() { static std::atomic<bool> value{false}; return value; }
    static std::atomic<bool>& allowedFlag() { static std::atomic<bool> value{false}; return value; }
    static std::atomic<bool>& closingFlag() { static std::atomic<bool> value{false}; return value; }
    static std::atomic<bool>& expectedEndFlag() { static std::atomic<bool> value{false}; return value; }
    static std::atomic<qulonglong>& microphoneStamp() { static std::atomic<qulonglong> value{0}; return value; }
    static std::atomic<qulonglong>& microphoneGeneration() { static std::atomic<qulonglong> value{1}; return value; }
};
}
