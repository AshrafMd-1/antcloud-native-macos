#include "microphonecapture.h"
#include "streambridge.h"
#include <QThread>
#include <algorithm>
#import <AVFoundation/AVFoundation.h>

namespace AntCloud::MicrophoneAudio {
void requestPermission(std::function<void(bool)> completion) {
    const AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if (status == AVAuthorizationStatusAuthorized) { completion(true); return; }
    if (status != AVAuthorizationStatusNotDetermined) { completion(false); return; }
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
        completion(granted == YES);
    }];
}

struct TapState {
    std::atomic<bool> running{false}, valid{true};
    std::atomic<unsigned> callbacks{0};
    CaptureQueue* queue = nullptr;
    const std::atomic<bool>* cancelled = nullptr;
    int rate = 0, channels = 0;
};

class MacCapture : public Capture {
public:
    ~MacCapture() override { stop(); }
    bool start(CaptureQueue& queue, const std::atomic<bool>& cancelled, CaptureFormat& format) override {
        @autoreleasepool {
            @try {
                m_Engine = [[AVAudioEngine alloc] init];
                AVAudioInputNode* input = [m_Engine inputNode];
                AVAudioFormat* hardware = [input outputFormatForBus:0];
                const int rate = int(hardware.sampleRate), channels = int(hardware.channelCount);
                if (rate < 8000 || rate > 192000 || channels < 1 || channels > MaxChannels) { stop(); return false; }
                AVAudioFormat* tapFormat = [[[AVAudioFormat alloc] initStandardFormatWithSampleRate:hardware.sampleRate channels:hardware.channelCount] autorelease];
                const auto state = m_State;
                state->queue = &queue; state->cancelled = &cancelled;
                state->rate = rate; state->channels = channels;
                state->running.store(true, std::memory_order_release);
                [input installTapOnBus:0 bufferSize:512 format:tapFormat block:^(AVAudioPCMBuffer* buffer, AVAudioTime*) {
                    state->callbacks.fetch_add(1, std::memory_order_acq_rel);
                    if (state->running.load(std::memory_order_acquire) && state->valid.load(std::memory_order_acquire)
                        && !state->cancelled->load(std::memory_order_acquire) && StreamBridge::microphoneAllowed()) {
                        float* const* planes = buffer.floatChannelData;
                        if (!planes || int(buffer.format.channelCount) != state->channels
                            || int(buffer.format.sampleRate) != state->rate) state->valid.store(false);
                        else {
                            const int stride = int(buffer.stride);
                            const bool interleaved = buffer.format.interleaved;
                            const uint64_t capturedAt = nowMs();
                            const unsigned frames = std::min<unsigned>(buffer.frameLength, BlockFrames * QueueSlots);
                            for (unsigned offset = 0; offset < frames; offset += BlockFrames) {
                                if (state->cancelled->load(std::memory_order_acquire) || !StreamBridge::microphoneAllowed()) break;
                                std::array<float*, MaxChannels> chunk{};
                                for (int channel = 0; channel < state->channels; ++channel)
                                    chunk[channel] = (interleaved ? planes[0] + channel : planes[channel]) + offset * stride;
                                const uint64_t age = uint64_t(buffer.frameLength - offset) * 1000 / state->rate;
                                state->queue->push(chunk.data(), stride, state->channels,
                                    int(std::min<unsigned>(BlockFrames, frames - offset)), capturedAt > age ? capturedAt - age : 0);
                            }
                        }
                    }
                    state->callbacks.fetch_sub(1, std::memory_order_release);
                }];
                m_TapInstalled = true;
                m_Observer = [[[NSNotificationCenter defaultCenter] addObserverForName:AVAudioEngineConfigurationChangeNotification object:m_Engine queue:nil usingBlock:^(NSNotification*) {
                    state->valid.store(false, std::memory_order_release);
                }] retain];
                NSError* error = nil;
                [m_Engine prepare];
                if (![m_Engine startAndReturnError:&error]) { stop(); return false; }
                format = {channels, rate};
                return true;
            } @catch (NSException*) { stop(); return false; }
        }
    }
    bool healthy() const override {
        return m_State->valid.load(std::memory_order_acquire) && m_Engine && [m_Engine isRunning]
            && [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio] == AVAuthorizationStatusAuthorized;
    }
    void stop() override {
        m_State->running.store(false, std::memory_order_release);
        @autoreleasepool {
            if (m_Observer) { [[NSNotificationCenter defaultCenter] removeObserver:m_Observer]; [m_Observer release]; m_Observer = nil; }
            if (m_Engine) {
                @try {
                    [m_Engine stop];
                    if (m_TapInstalled) [[m_Engine inputNode] removeTapOnBus:0];
                } @catch (NSException*) {}
                m_TapInstalled = false;
            }
            // A late tap block retains TapState and sees running=false. Blocks
            // already using the queue finish before its worker can be destroyed.
            while (m_State->callbacks.load(std::memory_order_acquire) != 0) QThread::yieldCurrentThread();
            if (m_Engine) { [m_Engine release]; m_Engine = nil; }
        }
    }
private:
    AVAudioEngine* m_Engine = nil;
    id m_Observer = nil;
    bool m_TapInstalled = false;
    std::shared_ptr<TapState> m_State = std::make_shared<TapState>();
};
std::unique_ptr<Capture> makeCapture() { return std::make_unique<MacCapture>(); }
}
