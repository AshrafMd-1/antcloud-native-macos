#pragma once
#include "SDL_compat.h"
#include <QHostAddress>
#include <QUdpSocket>
#include <array>
#include <atomic>
#include <cstdint>

namespace AntCloud::MicrophoneAudio {
constexpr int SampleRate = 44100;
constexpr int PacketBytes = 882;
constexpr quint16 Port = 9002;
constexpr int MaxChannels = 8;
constexpr int BlockFrames = 1024;
constexpr unsigned QueueSlots = 8;
constexpr uint64_t MaxAgeMs = 100;

struct Block {
    std::array<float, BlockFrames * MaxChannels> samples{};
    int frames = 0;
    int channels = 0;
    uint64_t capturedAt = 0;
};

// One capture callback produces; the microphone thread consumes. Neither side
// changes the other's index, including when a full queue drops a new block.
class CaptureQueue {
public:
    bool push(float* const* planes, int stride, int channels, int frames, uint64_t capturedAt);
    bool pop(Block& block);
    uint64_t dropped() const { return m_Dropped.load(std::memory_order_relaxed); }
private:
    std::array<Block, QueueSlots> m_Blocks{};
    std::atomic<unsigned> m_Write{0}, m_Read{0};
    std::atomic<uint64_t> m_Dropped{0};
};

uint64_t nowMs();
inline bool fresh(const Block& block, uint64_t now) { return now >= block.capturedAt && now - block.capturedAt <= MaxAgeMs; }

class Converter {
public:
    ~Converter();
    bool configure(SDL_AudioFormat format, int channels, int rate);
    bool put(const void* data, int bytes);
    bool putCaptured(const Block& block, uint64_t now);
    void expire(uint64_t now);
    int available() const;
    int read(void* data, int bytes);
    bool flush();
    void clear();
private:
    SDL_AudioStream* m_Stream = nullptr;
    uint64_t m_CapturedAt = 0;
    bool m_HasCapturedTime = false;
};

class Sender {
public:
    explicit Sender(const QHostAddress& host);
    bool open();
    bool send(const void* pcm, int bytes, const std::atomic<bool>& cancelled);
private:
    QHostAddress m_Host;
    QUdpSocket m_Socket;
};
}
