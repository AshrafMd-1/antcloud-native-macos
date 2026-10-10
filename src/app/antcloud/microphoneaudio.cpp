#include "microphoneaudio.h"
#include "streambridge.h"
#include <algorithm>
#include <chrono>

namespace AntCloud::MicrophoneAudio {
uint64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool CaptureQueue::push(float* const* planes, int stride, int channels, int frames, uint64_t capturedAt) {
    if (!planes || channels < 1 || channels > MaxChannels || stride < 1
        || frames < 1 || frames > BlockFrames) return false;
    for (int channel = 0; channel < channels; ++channel) if (!planes[channel]) return false;
    const unsigned write = m_Write.load(std::memory_order_relaxed);
    const unsigned next = (write + 1) % QueueSlots;
    if (next == m_Read.load(std::memory_order_acquire)) {
        m_Dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    Block& block = m_Blocks[write];
    for (int frame = 0; frame < frames; ++frame)
        for (int channel = 0; channel < channels; ++channel)
            block.samples[frame * channels + channel] = planes[channel][frame * stride];
    block.frames = frames; block.channels = channels; block.capturedAt = capturedAt;
    m_Write.store(next, std::memory_order_release);
    return true;
}

bool CaptureQueue::pop(Block& block) {
    const unsigned read = m_Read.load(std::memory_order_relaxed);
    if (read == m_Write.load(std::memory_order_acquire)) return false;
    const Block& source = m_Blocks[read];
    block.frames = source.frames; block.channels = source.channels; block.capturedAt = source.capturedAt;
    // Tap blocks usually contain mono/stereo audio, not the eight-channel
    // capacity. Leave unused storage untouched on each consumer tick.
    std::copy_n(source.samples.begin(), source.frames * source.channels, block.samples.begin());
    m_Read.store((read + 1) % QueueSlots, std::memory_order_release);
    return true;
}

Converter::~Converter() { if (m_Stream) SDL_FreeAudioStream(m_Stream); }
bool Converter::configure(SDL_AudioFormat format, int channels, int rate) {
    if (m_Stream) { SDL_FreeAudioStream(m_Stream); m_Stream = nullptr; }
    m_HasCapturedTime = false;
    if (channels < 1 || channels > MaxChannels || rate < 8000 || rate > 192000) return false;
    m_Stream = SDL_NewAudioStream(format, channels, rate, AUDIO_S16LSB, 1, SampleRate);
    return m_Stream != nullptr;
}
bool Converter::put(const void* data, int bytes) { return m_Stream && bytes > 0 && SDL_AudioStreamPut(m_Stream, data, bytes) == 0; }
bool Converter::putCaptured(const Block& block, uint64_t now) {
    if (block.frames < 1 || block.frames > BlockFrames || block.channels < 1 || block.channels > MaxChannels) return false;
    expire(now);
    if (!fresh(block, now)) { clear(); return true; }
    if (m_HasCapturedTime && (block.capturedAt < m_CapturedAt || block.capturedAt - m_CapturedAt > MaxAgeMs)) clear();
    if (!put(block.samples.data(), block.frames * block.channels * int(sizeof(float)))) return false;
    m_CapturedAt = block.capturedAt; m_HasCapturedTime = true;
    return true;
}
void Converter::expire(uint64_t now) {
    if (m_HasCapturedTime && (now < m_CapturedAt || now - m_CapturedAt > MaxAgeMs)) clear();
}
int Converter::available() const { return m_Stream ? SDL_AudioStreamAvailable(m_Stream) : -1; }
int Converter::read(void* data, int bytes) { return m_Stream ? SDL_AudioStreamGet(m_Stream, data, bytes) : -1; }
bool Converter::flush() { return m_Stream && SDL_AudioStreamFlush(m_Stream) == 0; }
void Converter::clear() { if (m_Stream) SDL_AudioStreamClear(m_Stream); m_HasCapturedTime = false; }

Sender::Sender(const QHostAddress& host) : m_Host(host) {}
bool Sender::open() {
    if (m_Host.isNull() || m_Host.isMulticast() || m_Host == QHostAddress::AnyIPv4 || m_Host == QHostAddress::AnyIPv6) return false;
    return m_Socket.bind(m_Host.protocol() == QAbstractSocket::IPv6Protocol ? QHostAddress::AnyIPv6 : QHostAddress::AnyIPv4, 0);
}
bool Sender::send(const void* pcm, int bytes, const std::atomic<bool>& cancelled) {
    if (!pcm || bytes <= 0 || bytes > PacketBytes || bytes % 2 != 0
        || cancelled.load(std::memory_order_acquire) || !StreamBridge::microphoneAllowed()) return false;
    return m_Socket.writeDatagram(static_cast<const char*>(pcm), bytes, m_Host, Port) == bytes;
}
}
