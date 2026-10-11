#include "internal/true_peak_full_resource.hpp"
#include "internal/true_peak_itu4x_channel_stream.hpp"

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace rgsml::analysis::internal {
namespace {

[[nodiscard]] bool supported_rate(std::int64_t hz) noexcept
{
    switch (hz) {
    case 44100:
    case 48000:
    case 88200:
    case 96000:
    case 176400:
    case 192000:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] core::Result<std::optional<double>>
db_tp_of_peak(double linearPeak)
{
    if (!std::isfinite(linearPeak) || linearPeak < 0.0) {
        return core::Result<std::optional<double>>::failure(
            core::Error{core::ErrorCode::InvalidAudioSample,
                        "True Peak result is not finite and nonnegative."});
    }
    if (linearPeak == 0.0) {
        return core::Result<std::optional<double>>::success(std::nullopt);
    }
    const double db = 20.0 * std::log10(linearPeak);
    if (!std::isfinite(db)) {
        return core::Result<std::optional<double>>::failure(
            core::Error{core::ErrorCode::InvalidAudioSample,
                        "True Peak dBTP conversion was non-finite."});
    }
    return core::Result<std::optional<double>>::success(db);
}

}  // namespace

core::Result<UnqualifiedTruePeakResourceEvidence>
measure_unqualified_true_peak_full_resource(const audio::AudioBuffer& resource)
{
    const auto view = resource.view();
    const auto& fmt = view.format();
    const auto hz = fmt.sample_rate().value();
    if (hz <= 0) {
        return core::Result<UnqualifiedTruePeakResourceEvidence>::failure(
            core::Error{core::ErrorCode::InvalidArgument,
                        "True Peak sample rate must be positive."});
    }
    if (!supported_rate(hz)) {
        return core::Result<UnqualifiedTruePeakResourceEvidence>::failure(
            core::Error{core::ErrorCode::UnsupportedOperation,
                        "True Peak sample rate is outside the frozen v1 support matrix."});
    }
    const auto layout = fmt.channel_layout();
    if (layout != audio::ChannelLayout::MONO_C
        && layout != audio::ChannelLayout::STEREO_LR) {
        return core::Result<UnqualifiedTruePeakResourceEvidence>::failure(
            core::Error{core::ErrorCode::UnsupportedAudioLayout,
                        "Only canonical mono and stereo are supported."});
    }
    const auto frames = view.frame_count().value();
    // Empty owned buffers exist in the current audio API; their future public
    // result/lifecycle policy is not frozen. Refuse to invent it here.
    if (frames <= 0) {
        return core::Result<UnqualifiedTruePeakResourceEvidence>::failure(
            core::Error{core::ErrorCode::InvalidState,
                        "Empty-resource True Peak semantics not yet reconciled."});
    }
    const std::size_t count = fmt.channel_count();
    std::array<std::span<const double>, 2> channels{};
    for (std::size_t ch = 0; ch < count; ++ch) {
        auto span = view.channel(ch);
        if (!span) {
            return core::Result<UnqualifiedTruePeakResourceEvidence>::failure(
                *span.error());
        }
        channels[ch] = *span.value();
    }

    std::array<TruePeakItu4xChannelStream, 2> fir{};
    // Chronological source frame order: L/R independent, never summed.
    for (std::size_t i = 0; i < static_cast<std::size_t>(frames); ++i) {
        for (std::size_t ch = 0; ch < count; ++ch) {
            auto step = fir[ch].push(channels[ch][i]);
            if (!step) {
                return core::Result<UnqualifiedTruePeakResourceEvidence>::failure(
                    *step.error());
            }
        }
    }

    UnqualifiedTruePeakResourceEvidence result{};
    result.sample_rate_hz = hz;
    result.channel_count = count;
    for (std::size_t ch = 0; ch < count; ++ch) {
        auto peak = fir[ch].finalize();
        if (!peak) {
            return core::Result<UnqualifiedTruePeakResourceEvidence>::failure(
                *peak.error());
        }
        result.per_channel_linear_peak[ch] = *peak.value();
        auto db = db_tp_of_peak(*peak.value());
        if (!db) {
            return core::Result<UnqualifiedTruePeakResourceEvidence>::failure(
                *db.error());
        }
        result.per_channel_db_tp[ch] = *db.value();
        result.global_linear_peak = std::max(result.global_linear_peak,
                                              *peak.value());
    }
    result.is_exact_silence = (result.global_linear_peak == 0.0);
    auto globalDb = db_tp_of_peak(result.global_linear_peak);
    if (!globalDb) {
        return core::Result<UnqualifiedTruePeakResourceEvidence>::failure(
            *globalDb.error());
    }
    result.global_db_tp = *globalDb.value();
    return core::Result<UnqualifiedTruePeakResourceEvidence>::success(result);
}

}  // namespace rgsml::analysis::internal
