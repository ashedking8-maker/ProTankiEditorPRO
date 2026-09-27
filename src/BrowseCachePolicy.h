#pragma once
// Byte budgets rather than count-based eviction: entries may have different dimensions.
#include <cstddef>
#include <cstdint>
#include <algorithm>
namespace BrowseCachePolicy {
inline constexpr size_t MiB=1024u*1024u;
inline constexpr size_t MaxGpuBytes=256u*MiB;
inline constexpr size_t MinGpuBytes=64u*MiB;
inline constexpr size_t MaxCpuBytes=128u*MiB;
inline size_t GpuBudget(size_t dedicatedVideoMemory) {
    return dedicatedVideoMemory ? std::clamp(dedicatedVideoMemory/16u,MinGpuBytes,MaxGpuBytes) : MinGpuBytes;
}
inline size_t RawBytes(unsigned width,unsigned height) {
    return static_cast<size_t>(width)*height*4u;
}
}
