// Strictly PRIVATE M22 output-rate timeline metadata. Not a renderer.
// All rendered frame counts are EXTERNALLY DECLARED by a real full-Master
// witness; this metadata-only candidate cannot certify FIR/TP/PCM output.
#pragma once
#include "m22_request_plan_candidate.hpp"
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::m22_private {

enum class OutputClockFailure : std::uint8_t {
    NONE, INVALID_CLOCK, MISSING_IDENTITY, INVALID_LENGTH,
    UNATTESTED_FINAL_SRC, WRONG_BYPASS_LENGTH, INVALID_WINDOW
};
struct OutputClockDeclaration final {
    std::uint32_t source_processing_hz{};
    std::uint32_t output_hz{};
    std::uint64_t source_processing_frames{};
    // Must come from an external verified full-programme renderer,
    // never Math.round(source_frames * output_hz / source_hz).
    std::uint64_t declared_full_output_frames{};
    std::string exact_checked_full_master_identity{};
    // For cross-rate metadata only. A nonempty value is NOT a verification
    // receipt and cannot authorize actual production use by itself.
    std::string selected_final_src_asset_id{};
};
struct OutputWindowCandidate final {
    std::uint64_t absolute_output_first{};
    std::uint64_t absolute_output_end{};
    std::uint64_t replay_source_first{0};
    std::uint64_t required_source_true_eos{};
    std::uint64_t required_output_true_eos{};
    std::uint64_t absolute_dither_first{};
    [[nodiscard]] std::uint64_t output_frame_count() const noexcept {
        return absolute_output_end-absolute_output_first;
    }
};

class CheckedOutputClockCandidate final {
public:
    [[nodiscard]] static std::optional<CheckedOutputClockCandidate> freeze(
        const OutputClockDeclaration& d,OutputClockFailure& failure)
    {
        failure=OutputClockFailure::NONE;
        if(!supported_clock(d.source_processing_hz)
           ||!supported_clock(d.output_hz)) {
            failure=OutputClockFailure::INVALID_CLOCK;
            return std::nullopt;
        }
        if(d.exact_checked_full_master_identity.empty()) {
            failure=OutputClockFailure::MISSING_IDENTITY;
            return std::nullopt;
        }
        // Canonical FrameCount/FrameIndex use signed 64-bit storage:
        // reject non-representable values before any cast or QML publish.
        constexpr auto limit=static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max());
        if(d.source_processing_frames==0
           ||d.declared_full_output_frames==0
           ||d.source_processing_frames>limit
           ||d.declared_full_output_frames>limit) {
            failure=OutputClockFailure::INVALID_LENGTH;
            return std::nullopt;
        }
        if(d.source_processing_hz==d.output_hz) {
            if(d.declared_full_output_frames!=d.source_processing_frames) {
                failure=OutputClockFailure::WRONG_BYPASS_LENGTH;
                return std::nullopt;
            }
            if(!d.selected_final_src_asset_id.empty()) {
                failure=OutputClockFailure::UNATTESTED_FINAL_SRC;
                return std::nullopt;
            }
        }else if(d.selected_final_src_asset_id.empty()) {
            // Cross-rate is never silently replaced by a guessed bypass.
            failure=OutputClockFailure::UNATTESTED_FINAL_SRC;
            return std::nullopt;
        }
        return CheckedOutputClockCandidate(d);
    }
    [[nodiscard]] std::optional<OutputWindowCandidate> plan_window(
        Mode mode,Window outputWindow,OutputClockFailure& failure) const noexcept
    {
        failure=OutputClockFailure::NONE;
        if((mode!=Mode::FULL&&mode!=Mode::PREVIEW)
           ||outputWindow.first>=outputWindow.end
           ||outputWindow.end>d_.declared_full_output_frames
           ||(mode==Mode::FULL&&(outputWindow.first!=0
               ||outputWindow.end!=d_.declared_full_output_frames))) {
            failure=OutputClockFailure::INVALID_WINDOW;
            return std::nullopt;
        }
        // Real stateful DSP and M17 final TP must be computed from exact
        // SOURCE origin to complete program EOS, never the selected region.
        return OutputWindowCandidate{
            outputWindow.first,outputWindow.end,0,
            d_.source_processing_frames,d_.declared_full_output_frames,
            outputWindow.first};
    }
    [[nodiscard]] bool uses_final_src() const noexcept {
        return d_.source_processing_hz!=d_.output_hz;
    }
    [[nodiscard]] std::pair<std::uint32_t,std::uint32_t>
    reduced_rate_ratio() const noexcept {
        const auto g=std::gcd(d_.source_processing_hz,d_.output_hz);
        return {d_.output_hz/g,d_.source_processing_hz/g};
    }
    [[nodiscard]] bool authorizes_release() const noexcept {
        return false; // only actual FIR + full-M17 Product may release
    }
    [[nodiscard]] const OutputClockDeclaration&
    declaration() const noexcept {return d_;}
private:
    explicit CheckedOutputClockCandidate(const OutputClockDeclaration& d)
       :d_(d){}
    const OutputClockDeclaration d_;
};
} // namespace rgsml::m22_private
