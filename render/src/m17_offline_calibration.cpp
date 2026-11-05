#include <rgsml/render/m17_offline_calibration.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace rgsml::render {
namespace {
[[nodiscard]] core::Error refuse(core::ErrorCode code,
    const char* category,const char* message) {
    return core::Error{code,message,{{"category",category}}};
}
[[nodiscard]] bool supported_m17_rate(std::int64_t hz) noexcept {
    return hz==44100||hz==48000||hz==88200
        ||hz==96000||hz==176400||hz==192000;
}
}

core::Result<M17OfflineCalibrationMeasurement> calibrate_m17_offline_once(
    core::SampleRate rate,audio::ChannelLayout layout,
    dsp::TruePeakLimiterParameters params,M17CalibrationStimulus stimulus,
    core::FrameCount frames,const dsp::ModuleRegistry& registry,
    core::FrameCount block,const TerminalRenderCancel& cancelled)
{
    using M=M17OfflineCalibrationMeasurement;
    if(!supported_m17_rate(rate.value()) || frames.value()<rate.value()/50
       || frames.value()>rate.value()*2 || block.value()<=0
       || block.value()>4096
       || (layout!=audio::ChannelLayout::MONO_C
           && layout!=audio::ChannelLayout::STEREO_LR))
        return core::Result<M>::failure(refuse(core::ErrorCode::InvalidArgument,
            "M17_CALIBRATION_SPEC_INVALID",
            "Calibration requires a supported rate, 20ms-2s PCM, mono/stereo and bounded block."));
    if(cancelled && cancelled())
        return core::Result<M>::failure(refuse(core::ErrorCode::InvalidState,
            "M17_CALIBRATION_CANCELLED","Calibration was cancelled."));
    auto format=audio::AudioFormat::create(rate,layout);
    if(!format)return core::Result<M>::failure(*format.error());
    auto audio=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::OUTPUT_RATE,core::FrameIndex{0},frames);
    if(!audio)return core::Result<M>::failure(*audio.error());
    for(std::size_t ch=0;ch<format.value()->channel_count();++ch) {
        auto lane=audio.value()->mutable_view().channel(ch);
        if(!lane)return core::Result<M>::failure(*lane.error());
        // A deliberately nonzero integer-index signal; ZERO device API calls,
        // no arbitrary waveform generation counted as DSP execution.
        for(std::size_t i=0;i<lane.value()->size();++i) {
            const auto phase=static_cast<unsigned>(i%97);
            const double quiet=phase<48 ? 0.105 : -0.095;
            const double transient=(i%211==0) ? 0.84
                : (i%307==0 ? -0.77 : quiet);
            (*lane.value())[i]=(stimulus==M17CalibrationStimulus::TRANSIENT_STRESS
                ? transient : quiet)*(ch==0 ? 1.0 : -0.73);
        }
    }
    render::TerminalRenderStageTimings timings{};
    const auto start=std::chrono::steady_clock::now();
    auto output=render_true_peak_limiter_checked_candidate(
        audio.value()->view(),params,registry,block,false,
        cancelled,&timings,true);
    const auto duration=std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-start).count();
    if(!output)return core::Result<M>::failure(*output.error());
    if(!output.value()->final_check
       ||output.value()->terminal.bypassed
       ||output.value()->terminal.compensated_pcm.view().frame_count()!=frames
       ||output.value()->terminal.compensated_pcm.view().format()!=*format.value())
        return core::Result<M>::failure(refuse(core::ErrorCode::InvalidState,
            "M17_CALIBRATION_UNVERIFIED",
            "M17 calibration rejected incomplete or unverified full-resource PCM."));
    if(timings.m17_core_us<0 || timings.m16_primary_us<0)
        return core::Result<M>::failure(refuse(core::ErrorCode::InvalidState,
            "M17_CALIBRATION_STAGE_MISSING",
            "Calibration requires measured M17 and M16 stages."));
    return core::Result<M>::success(M{
        rate,frames,layout,params.ceiling_db_tp(),stimulus,
        std::max<std::int64_t>(1,duration),
        timings.m17_core_us,timings.m16_primary_us,
        timings.verification_trim_us,timings.m16_secondary_us});
}

core::Result<std::int64_t> estimate_m17_m16_microseconds(
    const M17OfflineCalibrationMeasurement& calibration,
    core::SampleRate requested_rate,audio::ChannelLayout layout,
    double ceiling,M17CalibrationStimulus stimulus,
    core::FrameCount output_frames)
{
    if(calibration.output_rate!=requested_rate || calibration.layout!=layout
       ||calibration.ceiling_db_tp!=ceiling
       ||calibration.stimulus!=stimulus
       ||calibration.probe_frames.value()<=0
       ||calibration.m17_and_m16_us<=0
       ||output_frames.value()<=0)
        return core::Result<std::int64_t>::failure(refuse(
            core::ErrorCode::InvalidArgument,"M17_CALIBRATION_PROFILE_MISMATCH",
            "Time prediction requires a valid measurement at the same rate, layout, Ceiling and stimulus."));
    // Long-double is for a NON-AUTHORITATIVE wall-time projection only.
    // No audio-frame mapping, conversion, signal processing or FP certificate.
    const auto projected=
        static_cast<long double>(calibration.m17_and_m16_us)
        *static_cast<long double>(output_frames.value())
        /static_cast<long double>(calibration.probe_frames.value());
    if(!std::isfinite(projected) || projected<=0.0L
       ||projected>=static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
        return core::Result<std::int64_t>::failure(refuse(
            core::ErrorCode::InvalidArgument,"M17_CALIBRATION_ESTIMATE_OVERFLOW",
            "Calibration projection is not representable."));
    return core::Result<std::int64_t>::success(
        std::max<std::int64_t>(1,static_cast<std::int64_t>(std::ceil(projected))));
}
} // namespace rgsml::render
