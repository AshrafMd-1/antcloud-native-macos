// Process-local control channel. Account tokens and passwords stay on the worker.
#pragma once
#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <atomic>
#include "SDL_compat.h"

namespace AntCloud {
class StreamBridge {
public:
    static void install(QObject* worker) { QMutexLocker lock(&mutex()); target() = worker; enabledFlag() = worker != nullptr; }
    static bool enabled() { return enabledFlag().load(); }
    // Set before queuing Stop so provider events cannot race the local SDL exit.
    static void requestStop() {
        QMutexLocker lock(&mutex());
        if (!target()) return;
        expectedEndFlag() = true;
        QMetaObject::invokeMethod(target(), "stop", Qt::QueuedConnection);
    }
    static void setExpectedEnd(bool value) { expectedEndFlag() = value; }
    static bool expectedEnd() { return expectedEndFlag().load(); }
    static void setHostRequestsAllowed(bool value) { allowedFlag() = value; }
    static void setClosing() { closingFlag() = true; }
    static bool hostRequestsAllowed() { return !closingFlag().load() && (!enabled() || allowedFlag().load()); }
    static bool shutdown() {
        QMutexLocker lock(&mutex());
        return target() && QMetaObject::invokeMethod(target(), "shutdown", Qt::QueuedConnection);
    }
    static void pair(const QString& pin) {
        QMutexLocker lock(&mutex());
        if (target()) QMetaObject::invokeMethod(target(), "pair", Qt::QueuedConnection, Q_ARG(QString, pin));
    }
    // Only validated provider minutes cross into the decoder; no credentials or GUI access.
    static void setRemainingMinutes(int value) { remainingMinutesFlag() = value >= 0 ? value : -1; }
    static int remainingMinutes() { return remainingMinutesFlag().load(); }
    static void setActive(bool value) { activeFlag() = value; }
    static bool active() { return activeFlag().load(); }
    static void interrupt() {
        if (!active()) return;
        SDL_Event event{}; event.type = SDL_QUIT; SDL_PushEvent(&event);
    }
private:
    static QObject*& target() { static QObject* value = nullptr; return value; }
    static QMutex& mutex() { static QMutex value; return value; }
    static std::atomic<bool>& enabledFlag() { static std::atomic<bool> value{false}; return value; }
    static std::atomic<int>& remainingMinutesFlag() { static std::atomic<int> value{-1}; return value; }
    static std::atomic<bool>& activeFlag() { static std::atomic<bool> value{false}; return value; }
    static std::atomic<bool>& allowedFlag() { static std::atomic<bool> value{false}; return value; }
    static std::atomic<bool>& closingFlag() { static std::atomic<bool> value{false}; return value; }
    static std::atomic<bool>& expectedEndFlag() { static std::atomic<bool> value{false}; return value; }
};
}
