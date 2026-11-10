#include <rgsml/analysis/streamed_reference_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include "internal/m19_basic_level_candidate.hpp"
#include "internal/m20_bounded_spectrum_candidate.hpp"
#include "internal/m20_stereo_candidate.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::analysis {
namespace {
using M19State=internal::m19::LoudnessPreproductionState;
using M20Diagnostic=m20_private::Diagnostic;
using M20State=TonalStereoMetricState;
[[nodiscard]] core::Error bad(const char* message) {
    return core::Error{core::ErrorCode::InvalidState,message};
}
[[nodiscard]] M20State status(M20Diagnostic d) noexcept {
    switch(d){
    case M20Diagnostic::valid:return M20State::AVAILABLE;
    case M20Diagnostic::not_applicable:return M20State::NOT_APPLICABLE;
    case M20Diagnostic::insufficient_content:return M20State::INSUFFICIENT_CONTENT;
    case M20Diagnostic::undefined_denominator:return M20State::UNDEFINED_DENOMINATOR;
    case M20Diagnostic::invalid_input:return M20State::UNAVAILABLE_AUTHORITATIVE_SUPPORT;
    }
    return M20State::UNAVAILABLE_AUTHORITATIVE_SUPPORT;
}
[[nodiscard]] TonalStereoValue metric(std::optional<double> x,M20Diagnostic d){
    return {x,status(d)};
}
[[nodiscard]] BasicLevelValue available_or_silent(std::optional<double> x) {
    return {x,x?BasicLevelValueState::AVAILABLE:BasicLevelValueState::SILENCE};
}
[[nodiscard]] BasicLevelValueState loudness_state(M19State s) noexcept {
    switch(s){
    case M19State::available:return BasicLevelValueState::AVAILABLE;
    case M19State::insufficient_duration:return BasicLevelValueState::INSUFFICIENT_DURATION;
    case M19State::no_absolute_gated_blocks:return BasicLevelValueState::BELOW_LOUDNESS_GATE;
    case M19State::not_finalized:
    case M19State::unsupported_k_weighting_rate:
    case M19State::k_numerically_undefined:
        return BasicLevelValueState::NUMERIC_UNDEFINED;
    }
    return BasicLevelValueState::NUMERIC_UNDEFINED;
}
[[nodiscard]] BasicLevelAnalysisResult copy_m19(
    const internal::m19::BasicLevelPreproductionSnapshot& s)
{
    BasicLevelAnalysisResult out;
    out.scope_begin_frame=static_cast<std::int64_t>(s.scope_begin_frame);
    out.scope_frame_count=static_cast<std::int64_t>(s.scope_frames);
    out.sample_rate_hz=static_cast<std::int64_t>(s.sample_rate_hz);
    out.channel_count=s.channel_count;
    out.frame_domain=audio::FrameDomainId::SOURCE_PROCESSING_RATE;
    out.k_weighting_bank_member_id=s.k_weighting_bank_member_id;
    out.k_weighting_bank_sha256=s.k_weighting_bank_sha256;
    out.retained_block_disclosure_complete=s.retained_block_disclosure_complete;
    out.retained_block_union.reserve(s.retained_block_union.size());
    for(const auto& part:s.retained_block_union)
        out.retained_block_union.push_back({part.begin,part.end});
    out.integrated_lufs={s.integrated_lufs,loudness_state(s.loudness_state)};
    out.full_scope_rms_dbfs=available_or_silent(s.full_scope_rms_dbfs);
    out.sample_peak_dbfs=available_or_silent(s.sample_peak_dbfs);
    out.sample_peak_crest_db=available_or_silent(s.sample_peak_crest_db);
    // M16 public true peak accepts an entire owning buffer only.
    // Never substitute peak or claim qualification on streaming input.
    out.true_peak_db_tp={std::nullopt,BasicLevelValueState::M16_SCOPE_UNAVAILABLE};
    return out;
}
} // namespace

core::Result<StreamedReferenceAnalysis> measure_streamed_reference_scope(
    const audio::AudioFormat& format,
    core::FrameCount totalFrames,
    const StreamedReferenceRead& read)
{
    using R=core::Result<StreamedReferenceAnalysis>;
    constexpr std::uint64_t kBlock=4096U;
    const auto rate64=format.sample_rate().value();
    const auto channels=format.channel_count();
    const auto total64=totalFrames.value();
    if(!read||total64<=0||rate64<=0||
       rate64>static_cast<std::int64_t>((std::numeric_limits<std::uint32_t>::max)())||
       !m20_private::supported_rate(static_cast<std::uint32_t>(rate64))||
       (channels!=1U&&channels!=2U))
        return R::failure(bad("M21_STREAM_INVALID_RATE_LAYOUT_OR_READER"));
    const auto fs=static_cast<std::uint32_t>(rate64);
    const auto total=static_cast<std::uint64_t>(total64);

    // The reader itself verifies every absolute start and format. The block
    // owner must be newly constructed with that same start: a mutable view
    // with the previous block's timebase would be rejected by WavReader.
    auto decode=[&](std::uint64_t start,std::uint64_t frames)
        ->core::Result<audio::AudioBuffer>
    {
        using B=core::Result<audio::AudioBuffer>;
        if(frames==0||start>total||frames>total-start)
            return B::failure(bad("M21_STREAM_BLOCK_OUT_OF_SCOPE"));
        auto count=core::FrameCount::create(static_cast<std::int64_t>(frames));
        if(!count)return B::failure(*count.error());
        auto buffer=audio::AudioBuffer::create(
            format,audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{static_cast<std::int64_t>(start)},*count.value());
        if(!buffer)return B::failure(*buffer.error());
        auto n=read(core::FrameIndex{static_cast<std::int64_t>(start)},
                    buffer.value()->mutable_view());
        if(!n)return B::failure(*n.error());
        if(*n.value()!=*count.value())
            return B::failure(bad("M21_STREAM_WAV_SHORT_OR_PARTIAL_DECODE"));
        return B::success(std::move(*buffer.value()));
    };

    internal::m19::BasicLevelPreproduction m19(fs,static_cast<std::uint32_t>(channels),
                                               0,total);
    m20_private::BoundedSpectrumAccumulatorCandidate m20(
        fs,0,total,static_cast<std::uint32_t>(channels),false,{});
    for(std::uint64_t pos=0;pos<total;){
        const auto count=std::min(kBlock,total-pos);
        auto block=decode(pos,count);
        if(!block)return R::failure(*block.error());
        const auto view=block.value()->view();
        const auto l=view.channel(0U);
        if(!l)return R::failure(*l.error());
        std::span<const double> right{};
        if(channels==2U){
            const auto r=view.channel(1U);
            if(!r)return R::failure(*r.error());
            right=*r.value();
        }
        if(!m19.push(*l.value(),right))
            return R::failure(bad("M21_STREAM_M19_PCM_OR_SCOPE_INVALID"));
        if(!m20.push(*l.value(),right))
            return R::failure(bad("M21_STREAM_M20_PSD_PCM_INVALID"));
        pos+=count;
    }
    auto pre=m19.finalize();
    if(!pre)return R::failure(bad("M21_STREAM_M19_FINALIZE_FAILED"));
    auto spectrum=m20.finalize();
    if(spectrum.diagnostic==M20Diagnostic::invalid_input)
        return R::failure(bad("M21_STREAM_M20_PSD_FINALIZE_FAILED"));

    StreamedReferenceAnalysis result;
    result.max_decoded_block_frames=static_cast<std::size_t>(kBlock);
    result.level=copy_m19(*pre);
    auto& out=result.tonal;
    out.scope_begin_frame=0;
    out.scope_frame_count=total64;
    out.sample_rate_hz=rate64;
    out.channel_count=channels;
    out.frame_domain=audio::FrameDomainId::SOURCE_PROCESSING_RATE;
    out.complete_spectral_windows=spectrum.complete_frames;
    out.selected_spectral_windows=spectrum.selected_frames;
    out.spectral_all_frames_fallback=spectrum.all_frames_fallback;
    out.spectrum_state=status(spectrum.diagnostic);
    if(spectrum.diagnostic==M20Diagnostic::valid){
        out.mean_linear_psd=std::move(spectrum.average_psd);
        out.primary_bands_1_12.reserve(spectrum.bands_1_12.size());
        for(const auto& b:spectrum.bands_1_12){
            out.primary_bands_1_12.push_back({
                b.band.index,b.band.center,b.band.low,b.band.high,
                b.band.fully_supported,b.measured_energy,
                {std::nullopt,M20State::UNAVAILABLE_AUTHORITATIVE_SUPPORT}});
        }
    }

    m20_private::WidthResult width;
    m20_private::CorrelationResult correlation;
    if(channels==1U){
        width.diagnostic=M20Diagnostic::not_applicable;
        correlation.diagnostic=M20Diagnostic::not_applicable;
    }else{
        // The SAME M19 producer's retained union drives P06; empty is an
        // authoritative available-empty gate, never an ALL_SCOPE substitute.
        std::vector<m20_private::Interval> retained;
        retained.reserve(pre->retained_block_union.size());
        for(const auto& x:pre->retained_block_union)
            retained.push_back({x.begin,x.end});
        const auto unionBlocks=m20_private::selected_union_frames(
            0,total,pre->retained_block_disclosure_complete,retained);
        if(!unionBlocks)
            return R::failure(bad("M21_STREAM_P06_RETAINED_UNION_INVALID"));
        std::uint64_t selected=0;
        m20_private::WidthTwoPassAccumulator acc;
        for(int pass=0;pass<2;++pass){
            for(const auto& interval:*unionBlocks){
                if(pass==0)selected+=interval.end-interval.begin;
                for(std::uint64_t pos=interval.begin;pos<interval.end;){
                    const auto count=std::min(kBlock,interval.end-pos);
                    auto block=decode(pos,count);
                    if(!block)return R::failure(*block.error());
                    auto view=block.value()->view();
                    auto left=view.channel(0U),right=view.channel(1U);
                    if(!left)return R::failure(*left.error());
                    if(!right)return R::failure(*right.error());
                    for(std::size_t i=0;i<left.value()->size();++i){
                        if(pass==0)acc.observe((*left.value())[i],(*right.value())[i]);
                        else acc.add((*left.value())[i],(*right.value())[i]);
                    }
                    pos+=count;
                }
            }
        }
        width=acc.finish(selected,!pre->retained_block_disclosure_complete);

        // Exact existing 400-ms centered correlation per 100-ms grid window:
        // calling the existing one-shot kernel on one complete window emits
        // precisely that Grid100 window, with identical sum ordering/Type7.
        const auto window=m20_private::round_even_div(2ULL*fs,5U);
        result.max_correlation_window_frames=static_cast<std::size_t>(window);
        correlation.window_frames=window;
        std::vector<double> valid;
        if(total>=window){
            for(std::uint64_t j=0;;++j){
                const auto off=m20_private::grid100_offset(j,fs);
                if(!off||*off>total||window>total-*off)break;
                auto block=decode(*off,window);
                if(!block)return R::failure(*block.error());
                const auto view=block.value()->view();
                const auto l=view.channel(0U),r=view.channel(1U);
                if(!l)return R::failure(*l.error());
                if(!r)return R::failure(*r.error());
                const auto single=m20_private::correlation_summary(
                    {fs,*off,*l.value(),*r.value(),false,{}});
                if(single.diagnostic==M20Diagnostic::invalid_input||
                   single.series.size()!=1U||
                   single.series.front().absolute_start!=*off)
                    return R::failure(bad("M21_STREAM_CORRELATION_WINDOW_INVALID"));
                const auto w=single.series.front();
                correlation.series.push_back(w);
                if(w.diagnostic==M20Diagnostic::valid&&w.rho)
                    valid.push_back(*w.rho);
                if(j==(std::numeric_limits<std::uint64_t>::max)())break;
            }
        }
        if(valid.empty())correlation.diagnostic=M20Diagnostic::insufficient_content;
        else{
            correlation.p10=m20_private::type7(valid,0.1);
            correlation.p50=m20_private::type7(valid,0.5);
            correlation.p90=m20_private::type7(valid,0.9);
            correlation.diagnostic=M20Diagnostic::valid;
        }
    }
    out.width_all_scope_fallback=width.used_all_scope_fallback;
    out.width_ratio=metric(width.ratio,width.diagnostic);
    out.correlation_p10=metric(correlation.p10,correlation.diagnostic);
    out.correlation_p50=metric(correlation.p50,correlation.diagnostic);
    out.correlation_p90=metric(correlation.p90,correlation.diagnostic);
    out.correlation_grid100.reserve(correlation.series.size());
    for(const auto& c:correlation.series)
        out.correlation_grid100.push_back({
            c.absolute_start,metric(c.rho,c.diagnostic)});
    return R::success(std::move(result));
}
} // namespace rgsml::analysis
