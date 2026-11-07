#include <rgsml/analysis/m18_delivered_true_peak_candidate.hpp>
#include "internal/true_peak_itu4x_channel_stream.hpp"
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <utility>

namespace rgsml::analysis {
namespace {
[[nodiscard]] bool supported_rate(std::int64_t hz) noexcept
{
    return hz==44100 ||hz==48000 ||hz==88200 ||
           hz==96000 ||hz==176400 ||hz==192000;
}
[[nodiscard]] core::Error invalid(core::ErrorCode code,const char* s)
{
    return core::Error{code,s};
}
} // namespace

struct M18DeliveredTruePeakCandidate::State final {
    State(audio::AudioFormat fmt,std::int64_t count)
        :format(std::move(fmt)),full_frames(count) {}
    audio::AudioFormat format;
    std::int64_t full_frames;
    std::int64_t accepted_frames{0};
    std::array<internal::TruePeakItu4xChannelStream,2> fir{};
    bool completed{false};
};

M18DeliveredTruePeakCandidate::M18DeliveredTruePeakCandidate(
    std::unique_ptr<State> state) noexcept :state_(std::move(state)) {}

M18DeliveredTruePeakCandidate::~M18DeliveredTruePeakCandidate() noexcept=default;

core::Result<std::unique_ptr<M18DeliveredTruePeakCandidate>>
M18DeliveredTruePeakCandidate::create(
    audio::AudioFormat decodedFormat,core::FrameCount fullFrames)
{
    using R=core::Result<std::unique_ptr<M18DeliveredTruePeakCandidate>>;
    if(!supported_rate(decodedFormat.sample_rate().value())
        ||(decodedFormat.channel_layout()!=audio::ChannelLayout::MONO_C
            &&decodedFormat.channel_layout()!=audio::ChannelLayout::STEREO_LR)
        ||fullFrames.value()<=0) {
        return R::failure(invalid(core::ErrorCode::InvalidArgument,
            "M18_DELIVERED_TP_UNSUPPORTED_FORMAT_OR_EMPTY"));
    }
    try {
        return R::success(std::unique_ptr<M18DeliveredTruePeakCandidate>(
            new M18DeliveredTruePeakCandidate(
                std::make_unique<State>(std::move(decodedFormat),
                                        fullFrames.value()))));
    } catch(const std::bad_alloc&) {
        return R::failure(invalid(core::ErrorCode::IoFailure,
            "M18_DELIVERED_TP_ALLOCATION_FAILURE"));
    }
}

core::Status M18DeliveredTruePeakCandidate::push_decoded(
    audio::AudioBufferView chunk)
{
    auto fail=[](core::ErrorCode code,const char* msg)->core::Status {
        return core::Status::failure(invalid(code,msg));
    };
    if(state_->completed)
        return fail(core::ErrorCode::InvalidState,
            "M18_DELIVERED_TP_ALREADY_FINAL");
    const auto frameCount=chunk.frame_count().value();
    if(chunk.format()!=state_->format
        ||chunk.timebase().frame_domain_id()
            !=audio::FrameDomainId::SOURCE_PROCESSING_RATE
        ||chunk.timebase().sample_rate()!=state_->format.sample_rate()
        ||chunk.absolute_start_frame()!=core::FrameIndex{state_->accepted_frames}
        ||frameCount<=0
        ||frameCount>state_->full_frames-state_->accepted_frames)
        return fail(core::ErrorCode::InvalidFrameRange,
            "M18_DELIVERED_TP_NONCHRONOLOGICAL_OR_INVALID_CHUNK");
    // A channel-1 error must never commit partial channel-0 progress.
    auto newFir=state_->fir;
    for(std::size_t ch=0;ch<state_->format.channel_count();++ch) {
        const auto p=chunk.channel(ch);
        if(!p)return core::Status::failure(*p.error());
        const auto st=newFir[ch].push_chunk(*p.value());
        if(!st)return st;
    }
    state_->fir=std::move(newFir);
    state_->accepted_frames+=frameCount;
    return core::Status::success();
}

core::Result<M18DeliveredTruePeakEvidence>
M18DeliveredTruePeakCandidate::finalize()
{
    using R=core::Result<M18DeliveredTruePeakEvidence>;
    if(state_->completed||state_->accepted_frames!=state_->full_frames)
        return R::failure(invalid(core::ErrorCode::InvalidState,
            "M18_DELIVERED_TP_INCOMPLETE_OR_ALREADY_FINAL"));
    auto finalFir=state_->fir;
    M18DeliveredTruePeakEvidence evidence{};
    evidence.sample_rate_hz=state_->format.sample_rate().value();
    evidence.channel_count=state_->format.channel_count();
    evidence.programme_frames=
        static_cast<std::uint64_t>(state_->full_frames);
    for(std::size_t ch=0;ch<evidence.channel_count;++ch) {
        const auto peak=finalFir[ch].finalize(); // genuine M16 FIR+11 EOS
        if(!peak)return R::failure(*peak.error());
        evidence.per_channel_linear_peak[ch]=*peak.value();
        evidence.global_linear_peak=std::max(
            evidence.global_linear_peak,*peak.value());
    }
    evidence.is_exact_silence=evidence.global_linear_peak==0.0;
    if(!evidence.is_exact_silence) {
        const double db=20.0*std::log10(evidence.global_linear_peak);
        if(!std::isfinite(db))
            return R::failure(invalid(core::ErrorCode::InvalidAudioSample,
                "M18_DELIVERED_TP_DB_CONVERSION_FAILED"));
        evidence.global_db_tp=db;
        if(evidence.channel_count==1U)
            evidence.peak_channel=TruePeakPeakChannel::MONO_C;
        else if(evidence.per_channel_linear_peak[0]>
                evidence.per_channel_linear_peak[1])
            evidence.peak_channel=TruePeakPeakChannel::LEFT;
        else if(evidence.per_channel_linear_peak[1]>
                evidence.per_channel_linear_peak[0])
            evidence.peak_channel=TruePeakPeakChannel::RIGHT;
        else
            evidence.peak_channel=TruePeakPeakChannel::TIE;
    }
    // No QUALIFIED status possible here. The M16 product Gate G
    // remains the separate release authority.
    state_->fir=std::move(finalFir);
    state_->completed=true;
    return R::success(evidence);
}
} // namespace rgsml::analysis
