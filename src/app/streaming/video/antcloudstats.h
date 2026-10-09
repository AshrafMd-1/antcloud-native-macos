// AntCloud Native additions, GPL-3.0-or-later, 2026.
#pragma once
#include <cstdio>
#include <cstdint>
#include <cmath>

namespace AntCloudStats {
struct Snapshot {
    uint32_t rttMs;
    uint32_t totalFrames;
    uint32_t networkDroppedFrames;
    double receivedFps;
    double videoMbps;
    int width;
    int height;
};

inline void format(const Snapshot& value, char* output, int length)
{
    if (length <= 0) return;
    char rtt[32], loss[32], fps[32], bitrate[32];
    if (value.rttMs) std::snprintf(rtt, sizeof(rtt), "%u ms", value.rttMs);
    else std::snprintf(rtt, sizeof(rtt), "N/A");
    if (value.totalFrames) std::snprintf(loss, sizeof(loss), "%.1f%%",
        100.0 * value.networkDroppedFrames / value.totalFrames);
    else std::snprintf(loss, sizeof(loss), "N/A");
    if (value.totalFrames && std::isfinite(value.receivedFps))
        std::snprintf(fps, sizeof(fps), "%.1f", value.receivedFps);
    else std::snprintf(fps, sizeof(fps), "N/A");
    if (value.totalFrames && std::isfinite(value.videoMbps) && value.videoMbps >= 0)
        std::snprintf(bitrate, sizeof(bitrate), "%.1f Mbps", value.videoMbps);
    else std::snprintf(bitrate, sizeof(bitrate), "N/A");
    std::snprintf(output, static_cast<size_t>(length),
        "RTT %s | Loss %s | %s FPS | %s | %dx%d",
        rtt, loss, fps, bitrate, value.width, value.height);
}
}
