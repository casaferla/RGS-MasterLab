#pragma once
// L3 P2 optional exact-prefix caching RAM budget. This is an
// IMPLEMENTATION policy, NOT a frozen M10 solver cap. The full render
// must continue cold when a prefix cannot fit or availability is unknown.
// No hardcoded 256 MiB cap and no attempt to cache multiple full tracks.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <fstream>
#include <string>
#include <unistd.h>
#endif

namespace rgsml::app::l3_automatch::m10_p2_memory {
inline constexpr std::uint64_t kReserveForSystemAndProcessing =
    512ULL*1024ULL*1024ULL;
inline constexpr std::uint64_t kSingleSnapshotSafetyCeiling =
    2ULL*1024ULL*1024ULL*1024ULL;
// Limit cache to 25% of memory free AFTER reserving 512 MiB for
// ongoing rendering/UI/analysis. Only ONE exact PCM snapshot is held.
// This percentage and safety ceiling are adjustable implementation
// choices and have no authority over numerical AutoMatch policy.
[[nodiscard]] inline std::size_t budget_from_available_ram(
    std::uint64_t available_bytes,
    std::optional<std::size_t> requested_maximum=std::nullopt) noexcept
{
    if(available_bytes<=kReserveForSystemAndProcessing)
        return 0U;
    const auto quarter=
        (available_bytes-kReserveForSystemAndProcessing)/4U;
    auto permitted=std::min(quarter,kSingleSnapshotSafetyCeiling);
    if(requested_maximum)
        permitted=std::min(permitted,
            static_cast<std::uint64_t>(*requested_maximum));
    return static_cast<std::size_t>(std::min(
        permitted,
        static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max())));
}
[[nodiscard]] inline std::uint64_t available_ram_bytes() noexcept
{
#if defined(_WIN32)
    MEMORYSTATUSEX memory{};
    memory.dwLength=sizeof(memory);
    if(!GlobalMemoryStatusEx(&memory))return 0U;
    return static_cast<std::uint64_t>(memory.ullAvailPhys);
#elif defined(__linux__)
    // Linux MemAvailable includes reclaimable page cache and is a
    // better indicator of safe headroom than the free page count.
    std::ifstream input("/proc/meminfo");
    std::string name;
    unsigned long long amount=0;
    std::string unit;
    while(input>>name>>amount>>unit) {
        if(name=="MemAvailable:"&&unit=="kB") {
            if(amount>std::numeric_limits<std::uint64_t>::max()/1024ULL)
                return 0U;
            return static_cast<std::uint64_t>(amount)*1024ULL;
        }
    }
    // Fail closed to uncached rendering if the source is unavailable.
    return 0U;
#else
    // Unknown OS: do not guess a memory budget.
    return 0U;
#endif
}
[[nodiscard]] inline std::size_t current_budget(
    std::optional<std::size_t> requested_maximum=std::nullopt) noexcept
{
    return budget_from_available_ram(
        available_ram_bytes(),requested_maximum);
}
} // namespace rgsml::app::l3_automatch::m10_p2_memory
