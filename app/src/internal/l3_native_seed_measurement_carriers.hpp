#pragma once
// Immutable measurement carriers; no PCM, analyzer or render dependencies.
#include <cstdint>
#include <optional>
#include <vector>
namespace rgsml::app::l3_automatch::m05m06_real {
enum class NativeWindowProblem : std::uint8_t {
    none,invalid_full_source_or_rate,invalid_geometry,
    malformed_sample_or_arithmetic
};
struct NativeRmsCrestWindow final {
    std::int64_t absolute_start_frame{};
    std::optional<double> rms_dbfs;
    std::optional<double> crest_db;
};
struct D03D04NativeSeries final {
    std::vector<NativeRmsCrestWindow> complete_native_windows;
    std::uint64_t valid_rms_windows{};
    std::uint64_t valid_crest_windows{};
    NativeWindowProblem problem{NativeWindowProblem::invalid_geometry};
    [[nodiscard]] constexpr bool authorizes_solver_objective()
        const noexcept{return false;}
};
struct NativeFrameBand final {
    int exact_canonical_index{};
    double low_hz{},high_hz{};
};
} // namespace rgsml::app::l3_automatch::m05m06_real
namespace rgsml::app::l3_automatch {
struct NativeSpectralSeedCarrier final {
    std::vector<m05m06_real::NativeFrameBand> grid;
    std::vector<std::vector<std::optional<double>>> energy;
    std::vector<std::uint64_t> ordinals;
    std::uint64_t active_pcm_frames{};
    std::uint32_t sample_rate{};
};
} // namespace rgsml::app::l3_automatch
