// AntCloud Native additions, GPL-3.0-or-later, 2026.
#pragma once
#include <cstddef>
#include <cstdio>

namespace AntCloud {
inline void formatRemainingTime(char* output, std::size_t capacity, int minutes)
{
    if (capacity == 0) return;
    if (minutes < 0) std::snprintf(output, capacity, "unavailable");
    else std::snprintf(output, capacity, "%dh %dm", minutes / 60, minutes % 60);
}
}
