#pragma once
// L3-M06 D03/D04 real PCM native 400ms/100ms observations.
// The accepted Phase-6 C11 and M06 authority govern power floor,
// per-channel C*N normalization, complete window population and Type7.
// The same native RMS population feeds D03 and same-window sample-peak
// minus RMS feeds D04. No M21 nine-metric surrogate, no gated LUFS,
// event-aligned crest or new artistic Mass is introduced here.
#include "l3_m06_real_short_term_shape.hpp"
#include <rgsml/audio/audio_buffer_view.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
inline constexpr double kWindowedDigitalPowerFloor=1e-12;
inline constexpr std::size_t kNativeRmsGridBlocksPerWindow=4U;

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
struct NativeHundredMsBlock final {
    double sum_of_channel_sample_squares{};
    double max_abs_sample{};
    bool all_finite{true};
};
[[nodiscard]] inline std::optional<std::size_t>
exact_100ms_frames_for_native_rms(const audio::AudioBufferView& view) {
    const auto fs=view.format().sample_rate().value();
    // All accepted RGS L1 native processing rates are integer 100-ms
    // grids. Do not approximate unsupported rates or reanchor the grid.
    if(fs<=0||fs%10!=0||fs>192000)return std::nullopt;
    return static_cast<std::size_t>(fs/10);
}
[[nodiscard]] inline D03D04NativeSeries observe_d03_d04_real_pcm(
    const audio::AudioBufferView& full_source)
{
    D03D04NativeSeries out;
    if(full_source.absolute_start_frame().value()!=0||
       full_source.frame_count().value()<=0||
       full_source.timebase().frame_domain_id()!=
            audio::FrameDomainId::SOURCE_PROCESSING_RATE) {
        out.problem=NativeWindowProblem::invalid_full_source_or_rate;
        return out;
    }
    const auto block_frames=exact_100ms_frames_for_native_rms(full_source);
    const auto channels=full_source.format().channel_count();
    if(!block_frames||channels<1U||channels>2U) {
        out.problem=NativeWindowProblem::invalid_full_source_or_rate;
        return out;
    }
    const auto frames=static_cast<std::size_t>(
        full_source.frame_count().value());
    const auto number_of_complete_blocks=frames/(*block_frames);
    std::array<std::span<const double>,2> pcm{};
    for(std::size_t c=0;c<channels;++c) {
        const auto channel=full_source.channel(c);
        if(!channel||channel.value()->size()!=frames) {
            out.problem=NativeWindowProblem::invalid_geometry;
            return out;
        }
        pcm[c]=*channel.value();
    }
    // O(N) reading without rescanning 75% overlapped samples each
    // window. Exactly four native 100-ms blocks create one 400-ms
    // complete window; trailing incomplete material never enters
    // the denominator, and invalid completed blocks stay present.
    std::array<NativeHundredMsBlock,kNativeRmsGridBlocksPerWindow> ring{};
    if(number_of_complete_blocks>=kNativeRmsGridBlocksPerWindow)
        out.complete_native_windows.reserve(
            number_of_complete_blocks-kNativeRmsGridBlocksPerWindow+1U);
    const double denominator=static_cast<double>(
        channels*kNativeRmsGridBlocksPerWindow*(*block_frames));
    if(!std::isfinite(denominator)||denominator<=0.0) {
        out.problem=NativeWindowProblem::invalid_geometry;
        return out;
    }
    const volatile double checked_positive_denominator=denominator;
    for(std::size_t b=0;b<number_of_complete_blocks;++b) {
        NativeHundredMsBlock cell;
        const auto first=b*(*block_frames);
        for(std::size_t n=first;n<first+(*block_frames);++n) {
            for(std::size_t c=0;c<channels;++c) {
                const auto value=pcm[c][n];
                if(!std::isfinite(value)) {
                    cell.all_finite=false;
                    continue;
                }
                const auto magnitude=std::abs(value);
                cell.sum_of_channel_sample_squares+=value*value;
                cell.max_abs_sample=std::max(cell.max_abs_sample,magnitude);
            }
        }
        if(!std::isfinite(cell.sum_of_channel_sample_squares)||
           !std::isfinite(cell.max_abs_sample))
            cell.all_finite=false;
        ring[b%kNativeRmsGridBlocksPerWindow]=cell;
        if(b+1U<kNativeRmsGridBlocksPerWindow)continue;
        const auto window_index=b+1U-kNativeRmsGridBlocksPerWindow;
        const auto absolute_start=static_cast<std::int64_t>(
            window_index*(*block_frames));
        double sum=0.0,max_abs=0.0;
        bool valid=true;
        for(const auto& native:ring) {
            valid=valid&&native.all_finite;
            sum+=native.sum_of_channel_sample_squares;
            max_abs=std::max(max_abs,native.max_abs_sample);
        }
        NativeRmsCrestWindow measured;
        measured.absolute_start_frame=absolute_start;
        if(valid&&std::isfinite(sum)&&std::isfinite(max_abs)) {
            const double power=sum/checked_positive_denominator;
            if(std::isfinite(power)&&power>=kWindowedDigitalPowerFloor) {
                const double rms_dbfs=10.0*std::log10(power);
                if(std::isfinite(rms_dbfs)) {
                    measured.rms_dbfs=rms_dbfs;
                    ++out.valid_rms_windows;
                    if(max_abs>0.0) {
                        const double crest=20.0*std::log10(max_abs)-
                                           rms_dbfs;
                        if(std::isfinite(crest)) {
                            measured.crest_db=crest;
                            ++out.valid_crest_windows;
                        }
                    }
                }
            }
        }
        out.complete_native_windows.push_back(measured);
    }
    out.problem=NativeWindowProblem::none;
    return out;
}
struct D03D04Selectors final {
    std::optional<double> d03_type7_rms_p95_minus_p10_db;
    std::array<std::optional<double>,3> d04_type7_crest_p10_p50_p90_db{};
    std::uint64_t complete_native_windows{};
    std::uint64_t valid_rms_windows{};
    std::uint64_t valid_crest_windows{};
    std::optional<double> rms_native_support;
    std::optional<double> crest_native_support;
    [[nodiscard]] constexpr bool authorizes_m10() const noexcept {
        return false;
    }
};
[[nodiscard]] inline D03D04Selectors extract_d03_d04_type7(
    const D03D04NativeSeries& native)
{
    D03D04Selectors result;
    if(native.problem!=NativeWindowProblem::none)return result;
    result.complete_native_windows=native.complete_native_windows.size();
    std::vector<double> sorted_rms,sorted_crest;
    sorted_rms.reserve(result.complete_native_windows);
    sorted_crest.reserve(result.complete_native_windows);
    for(const auto& win:native.complete_native_windows) {
        if(win.rms_dbfs)sorted_rms.push_back(*win.rms_dbfs);
        if(win.crest_db)sorted_crest.push_back(*win.crest_db);
    }
    if(sorted_rms.size()!=native.valid_rms_windows||
       sorted_crest.size()!=native.valid_crest_windows||
       sorted_crest.size()>sorted_rms.size())return {};
    result.valid_rms_windows=sorted_rms.size();
    result.valid_crest_windows=sorted_crest.size();
    if(!result.complete_native_windows)return result;
    const double denominator=static_cast<double>(
        result.complete_native_windows);
    result.rms_native_support=
        static_cast<double>(result.valid_rms_windows)/denominator;
    result.crest_native_support=
        static_cast<double>(result.valid_crest_windows)/denominator;
    std::sort(sorted_rms.begin(),sorted_rms.end());
    std::sort(sorted_crest.begin(),sorted_crest.end());
    if(!sorted_rms.empty()) {
        const auto p10=type7_percentile(sorted_rms,0.10);
        const auto p95=type7_percentile(sorted_rms,0.95);
        if(p10&&p95)
            result.d03_type7_rms_p95_minus_p10_db=*p95-*p10;
    }
    if(!sorted_crest.empty()) {
        constexpr std::array<double,3> p{{0.10,0.50,0.90}};
        for(std::size_t i=0;i<p.size();++i)
            result.d04_type7_crest_p10_p50_p90_db[i]=
                type7_percentile(sorted_crest,p[i]);
    }
    return result;
}
// Prepared/REFERENCE pair use exact independent full-resource owner
// stamps. Native support uses MIN once, not a new M07 artistic weight.
// Eligibility and reference-grade validity remain external Phase-6.
struct D03D04PairedNative final {
    D03D04Selectors prepared;
    D03D04Selectors reference;
    std::optional<double> paired_d03_support;
    std::optional<double> paired_d04_support;
    [[nodiscard]] constexpr bool authorizes_solver_cost() const noexcept {
        return false;
    }
};
[[nodiscard]] inline std::optional<D03D04PairedNative>
pair_d03_d04_native(
    const D03D04NativeSeries& prepared,
    const D03D04NativeSeries& reference,
    const SourceIdentity& prepared_owner,
    const SourceIdentity& reference_owner)
{
    if(prepared.problem!=NativeWindowProblem::none||
       reference.problem!=NativeWindowProblem::none||
       !prepared_owner.full_track_owner_confirmed||
       !reference_owner.full_track_owner_confirmed||
       prepared_owner.generation==0||reference_owner.generation==0||
       prepared_owner.resource_identity.empty()||
       reference_owner.resource_identity.empty()||
       prepared_owner.resource_identity==reference_owner.resource_identity)
        return std::nullopt;
    D03D04PairedNative out{
        extract_d03_d04_type7(prepared),
        extract_d03_d04_type7(reference),{},{}};
    if(out.prepared.d03_type7_rms_p95_minus_p10_db&&
       out.reference.d03_type7_rms_p95_minus_p10_db&&
       out.prepared.rms_native_support&&
       out.reference.rms_native_support)
        out.paired_d03_support=std::min(
            *out.prepared.rms_native_support,
            *out.reference.rms_native_support);
    bool crest_ready=true;
    for(std::size_t i=0;i<3U;++i)
        crest_ready=crest_ready&&
            out.prepared.d04_type7_crest_p10_p50_p90_db[i]&&
            out.reference.d04_type7_crest_p10_p50_p90_db[i];
    if(crest_ready&&out.prepared.crest_native_support&&
       out.reference.crest_native_support)
        out.paired_d04_support=std::min(
            *out.prepared.crest_native_support,
            *out.reference.crest_native_support);
    return out;
}
} // namespace rgsml::app::l3_automatch::m05m06_real
