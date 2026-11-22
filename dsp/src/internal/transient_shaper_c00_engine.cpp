#include "transient_shaper_c00_engine.hpp"
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::dsp::internal {
namespace {
[[nodiscard]] core::Status fail(core::ErrorCode code,const char* why) {
    return core::Status::failure(core::Error{
        code,"L3_M03_TRANSIENT_SHAPER_C00_FAILURE",
        {{"category",std::string(why)}}});
}
[[nodiscard]] bool supported_rate(std::uint32_t fs) noexcept {
    return fs==44100U||fs==48000U||fs==96000U||fs==192000U;
}
[[nodiscard]] double coefficient(double fs,double tau_s) {
    return std::exp(-1.0/(tau_s*fs));
}
[[nodiscard]] bool overlap(std::span<double> a,
                            std::span<double> b) noexcept {
    if(a.empty()||b.empty())return false;
    const auto x=reinterpret_cast<std::uintptr_t>(a.data());
    const auto y=reinterpret_cast<std::uintptr_t>(b.data());
    return x<=y?(y-x)<a.size_bytes():(x-y)<b.size_bytes();
}
[[nodiscard]] bool finite(const TransientShaperC00State& s) noexcept {
    return std::isfinite(s.f)&&std::isfinite(s.m)&&std::isfinite(s.l)
        &&std::isfinite(s.g_attack_db)&&std::isfinite(s.g_sustain_db)
        &&s.f>=0.0&&s.m>=0.0&&s.l>=0.0;
}
} // namespace

core::Result<TransientShaperC00Engine> TransientShaperC00Engine::create(
    const TransientShaperParameters& params,std::uint32_t fs,
    std::uint32_t channels,std::size_t max,
    bool enable_activity_observation)
{
    using R=core::Result<TransientShaperC00Engine>;
    if(!supported_rate(fs)||channels<1U||channels>2U||max==0U)
        return R::failure(core::Error{
            core::ErrorCode::UnsupportedOperation,
            "L3_M03_TRANSIENT_SHAPER_UNSUPPORTED_RATE_LAYOUT_OR_BLOCK"});
    const double rate=static_cast<double>(fs);
    const double a_f=coefficient(rate,0.030);
    const double a_m=coefficient(rate,0.010);
    const double a_l=coefficient(rate,0.150);
    const double a_g=coefficient(rate,0.00025);
    for(const double a:{a_f,a_m,a_l,a_g})
        if(!std::isfinite(a)||a<0.0||a>=1.0)
            return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "L3_M03_TRANSIENT_SHAPER_COEFFICIENT_INVALID"});
    try {
        std::vector<double> left(max,0.0);
        std::vector<double> right(channels==2U?max:0U,0.0);
        std::vector<TransientC00AppliedGains> gains(
            enable_activity_observation?max:0U);
        return R::success(TransientShaperC00Engine{
            params,fs,channels,max,a_f,a_m,a_l,a_g,
            std::move(left),std::move(right),std::move(gains)});
    }catch(const std::bad_alloc&) {
        return R::failure(core::Error{
            core::ErrorCode::IoFailure,
            "L3_M03_TRANSIENT_SHAPER_PREPARE_ALLOCATION_FAILED"});
    }catch(const std::length_error&) {
        return R::failure(core::Error{
            core::ErrorCode::OutOfRange,
            "L3_M03_TRANSIENT_SHAPER_MAX_BLOCK_TOO_LARGE"});
    }
}
core::Status TransientShaperC00Engine::restore_state(
    const TransientShaperC00State& proposed) noexcept
{
    if(!finite(proposed))
        return fail(core::ErrorCode::InvalidArgument,
                    "TRANSIENT_C00_CORRUPT_NONFINITE_OR_NEGATIVE_ENVELOPE");
    constexpr double slack=1e-12;
    const double a=params_.attack_amount_db();
    const double s=params_.sustain_amount_db();
    if(proposed.g_attack_db<std::min(0.0,a)-slack
       ||proposed.g_attack_db>std::max(0.0,a)+slack
       ||proposed.g_sustain_db<std::min(0.0,s)-slack
       ||proposed.g_sustain_db>std::max(0.0,s)+slack)
        return fail(core::ErrorCode::InvalidArgument,
                    "TRANSIENT_C00_CHECKPOINT_GAIN_OUT_OF_BOUNDS");
    state_=proposed;
    return core::Status::success();
}

core::Status TransientShaperC00Engine::process_block(
    std::span<const double> in_l,std::span<const double> in_r,
    std::span<double> out_l,std::span<double> out_r,
    ITransientC00ActivitySink* activity_sink,
    std::uint64_t absolute_first_frame)
{
    const auto n=in_l.size();
    if(n==0U||n>maximum_block_frames_||out_l.size()!=n
       ||(channels_==1U &&(!in_r.empty()||!out_r.empty()))
       ||(channels_==2U &&(in_r.size()!=n||out_r.size()!=n))
       ||overlap(out_l,out_r))
        return fail(core::ErrorCode::InvalidArgument,
                    "TRANSIENT_C00_INVALID_PROCESS_VIEWS");
    // No observer means no changed execution path and no extra allocation.
    // A refused observation request must not mutate audio or the C00 state.
    if(activity_sink &&
       (activity_scratch_.size()<n ||
        n>std::numeric_limits<std::uint64_t>::max()
          -absolute_first_frame))
        return fail(core::ErrorCode::InvalidArgument,
                    "TRANSIENT_C00_OBSERVATION_BUFFER_OR_FRAME_RANGE");
    for(std::size_t i=0;i<n;++i)
        if(!std::isfinite(in_l[i])||
           (channels_==2U&&!std::isfinite(in_r[i])))
            return fail(core::ErrorCode::InvalidAudioSample,
                        "TRANSIENT_C00_NONFINITE_INPUT");
    TransientShaperC00State staged=state_;
    constexpr double epsilon=1e-9;
    const double attack=params_.attack_amount_db();
    const double sustain=params_.sustain_amount_db();
    const bool neutral=params_.neutral();
    for(std::size_t i=0;i<n;++i) {
        const double p=channels_==2U
            ?std::max(std::abs(in_l[i]),std::abs(in_r[i]))
            :std::abs(in_l[i]);
        const double f=p>=staged.f?p:
            a_f_*staged.f+(1.0-a_f_)*p;
        const double m=a_m_*staged.m+(1.0-a_m_)*f;
        const double l=p>=staged.l?p:
            a_l_*staged.l+(1.0-a_l_)*p;
        const double descriptor_attack=std::max(f-m,0.0)/(f+epsilon);
        const double descriptor_sustain=std::max(l-f,0.0)/(l+epsilon);
        const double t_attack=attack*descriptor_attack;
        const double t_sustain=sustain*descriptor_sustain;
        const double g_attack=a_g_*staged.g_attack_db+
                              (1.0-a_g_)*t_attack;
        const double g_sustain=a_g_*staged.g_sustain_db+
                               (1.0-a_g_)*t_sustain;
        const double g_db=g_attack+g_sustain;
        if(!std::isfinite(p)||!std::isfinite(f)||!std::isfinite(m)||
           !std::isfinite(l)||!std::isfinite(descriptor_attack)||
           !std::isfinite(descriptor_sustain)||
           !std::isfinite(t_attack)||!std::isfinite(t_sustain)||
           !std::isfinite(g_attack)||!std::isfinite(g_sustain)||
           !std::isfinite(g_db)||f<0.0||m<0.0||l<0.0)
            return fail(core::ErrorCode::InvalidAudioSample,
                        "TRANSIENT_C00_NONFINITE_INTERMEDIATE");
        staged={f,m,l,g_attack,g_sustain};
        // Collect only in the already preallocated scratch. An upstream
        // failure later in this block can never leak telemetry to the sink.
        if(activity_sink)
            activity_scratch_[i]={g_attack,g_sustain};
        if(neutral) {
            // Signed-zero and subnormal preserving active identity:
            // states STILL advance, no multiplication by computed gain.
            scratch_left_[i]=in_l[i];
            if(channels_==2U)scratch_right_[i]=in_r[i];
            continue;
        }
        const double gain=std::pow(10.0,g_db/20.0);
        const double left=in_l[i]*gain;
        const double right=channels_==2U?in_r[i]*gain:0.0;
        if(!std::isfinite(gain)||!std::isfinite(left)||
           (channels_==2U&&!std::isfinite(right)))
            return fail(core::ErrorCode::InvalidAudioSample,
                        "TRANSIENT_C00_NONFINITE_OUTPUT");
        scratch_left_[i]=left;
        if(channels_==2U)scratch_right_[i]=right;
    }
    // Commit only after EVERY input, state and output has been checked.
    std::copy_n(scratch_left_.data(),n,out_l.data());
    if(channels_==2U)
        std::copy_n(scratch_right_.data(),n,out_r.data());
    state_=staged;
    // Success-only callback, synchronous and noexcept. Collector errors
    // affect observation validity only, never already committed audio.
    if(activity_sink)
        activity_sink->on_successful_block(
            absolute_first_frame,
            std::span<const TransientC00AppliedGains>(
                activity_scratch_.data(),n));
    return core::Status::success();
}
} // namespace rgsml::dsp::internal
