#pragma once
#include "microphoneaudio.h"
#include <functional>
#include <memory>

namespace AntCloud::MicrophoneAudio {
struct CaptureFormat { int channels = 0, rate = 0; };
class Capture {
public:
    virtual ~Capture() = default;
    virtual bool start(CaptureQueue& queue, const std::atomic<bool>& cancelled, CaptureFormat& format) = 0;
    virtual bool healthy() const = 0;
    virtual void stop() = 0;
};
void requestPermission(std::function<void(bool)> completion);
std::unique_ptr<Capture> makeCapture();
}
