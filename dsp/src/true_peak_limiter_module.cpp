#include <rgsml/dsp/true_peak_limiter_module.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/core/error.hpp>
#include "internal/true_peak_limiter_state.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::dsp {
namespace {
using core::Error;
using core::ErrorCode;
using core::Result;
using core::Status;
using internal::LimiterDetectorEvent;
using internal::LimiterDetectorQuarter;
using internal::TruePeakLimiterControllerState;
using internal::TruePeakLimiterDetectorState;

[[nodiscard]] Error fail(ErrorCode code, const char* category, const char* message)
{
    return Error{code, message, {{"category", category}}};
}
[[nodiscard]] bool supported(std::int64_t fs) noexcept
{
    return fs == 44100 || fs == 48000 || fs == 88200 || fs == 96000
        || fs == 176400 || fs == 192000;
}
[[nodiscard]] Status validate(const DspProcessSpec& spec)
{
    if (spec.frame_domain_id != audio::FrameDomainId::OUTPUT_RATE
        || !supported(spec.audio_format.sample_rate().value())
        || (spec.audio_format.channel_layout() != audio::ChannelLayout::MONO_C
            && spec.audio_format.channel_layout() != audio::ChannelLayout::STEREO_LR)
        || spec.maximum_block_frames.value() < 1) {
        return Status::failure(fail(ErrorCode::InvalidArgument,
            "INVALID_DSP_PROCESS_SPEC",
            "M17 requires mono/stereo binary64 OUTPUT_RATE at one of six rates."));
    }
    return Status::success();
}
} // namespace

struct TruePeakLimiterModule::Impl final {
    ModuleDescriptor descriptor;
    TruePeakLimiterParameters params;
    std::optional<DspProcessSpec> spec;
    std::optional<TruePeakLimiterDetectorState> detector;
    std::optional<TruePeakLimiterControllerState> controller;
    std::deque<std::array<double,2>> pcm_delay;
    // The event buffer retains only active demands, not silent ticks;
    // coverage_tick separately proves every elapsed detector sample.
    std::vector<LimiterDetectorEvent> events;
    std::size_t search_cursor{0};
    std::int64_t coverage_tick{-63};
    std::int64_t frame_count{0};
    std::int64_t raw_emitted{0};
    std::int64_t origin{0};
    std::int64_t drain_emitted{0};
    bool bound{false};
    bool detector_drained{false};
    bool finished{false};
    std::int64_t d{0};

    Impl(ModuleDescriptor desc, TruePeakLimiterParameters p)
        : descriptor(std::move(desc)), params(std::move(p)) {}

    void reset_stream() noexcept
    {
        if (detector) detector->reset();
        if (controller) controller->reset();
        pcm_delay.clear();
        events.clear();
        search_cursor=0;
        coverage_tick=-63;
        frame_count=0;
        raw_emitted=0;
        origin=0;
        drain_emitted=0;
        bound=false;
        detector_drained=false;
        finished=false;
    }
    [[nodiscard]] Status append(const LimiterDetectorQuarter& q)
    {
        const double ceiling=std::pow(10.0,params.ceiling_db_tp()/20.0);
        const auto accept=[&](const LimiterDetectorEvent& e) -> Status {
            if (!e.valid) return Status::success();
            if (e.tick <= coverage_tick || !std::isfinite(e.peak) || e.peak<0.0) {
                return Status::failure(fail(ErrorCode::InvalidState,
                    "INVALID_DETECTOR_EVENT", "M17 signed detector order/peak invalid."));
            }
            coverage_tick=e.tick;
            if (e.peak>ceiling) events.push_back(e);
            return Status::success();
        };
        if (auto s=accept(q.preceding_anchor);!s) return s;
        return accept(q.midpoint);
    }
    // Every tick from -62 is evaluated exactly once. Only certified complete
    // lookahead is eligible: future event history cannot be invented by seek.
    [[nodiscard]] Result<double> gain_for_frame(std::int64_t n)
    {
        const auto target=8*n;
        if (!detector_drained && coverage_tick < target+controller->lookahead_ticks()) {
            return Result<double>::failure(fail(ErrorCode::InvalidState,
                "UNPROVEN_LIMITER_LOOKAHEAD", "Detector has not proved entire attack horizon."));
        }
        double applied=1.0;
        while (controller->next_tick() <= target) {
            const auto m=controller->next_tick();
            while (search_cursor < events.size() && events[search_cursor].tick < m)
                ++search_cursor;
            const bool on_tick=search_cursor < events.size()
                && events[search_cursor].tick == m;
            const double current=on_tick ? events[search_cursor].peak : +0.0;
            const std::size_t first=search_cursor+(on_tick?1U:0U);
            std::size_t last=first;
            const auto cutoff=m+controller->lookahead_ticks();
            while (last<events.size() && events[last].tick<=cutoff)
                ++last;
            const std::span<const LimiterDetectorEvent> horizon = first==last
                ? std::span<const LimiterDetectorEvent>{}
                : std::span<const LimiterDetectorEvent>{events.data()+first,last-first};
            auto step=controller->advance(current,horizon);
            if (!step) return Result<double>::failure(*step.error());
            if (m==target) applied=step.value()->gain;
        }
        // Periodic bounded compaction; no stale former loop/seek events.
        if (search_cursor >= 4096 && search_cursor > events.size()/2) {
            events.erase(events.begin(),
                events.begin()+static_cast<std::ptrdiff_t>(search_cursor));
            search_cursor=0;
        }
        return Result<double>::success(applied);
    }
    [[nodiscard]] Result<std::array<double,2>> emit_raw()
    {
        std::array<double,2> out{+0.0,+0.0};
        if (raw_emitted>=d) {
            if (pcm_delay.empty()) return Result<std::array<double,2>>::failure(
                fail(ErrorCode::InvalidState,"PCM_DELAY_UNDERFLOW",
                    "Limiter attempted to emit an unowned programme frame."));
            const auto n=raw_emitted-d;
            auto gain=gain_for_frame(n);
            if (!gain) return Result<std::array<double,2>>::failure(*gain.error());
            const auto sample=pcm_delay.front();
            pcm_delay.pop_front();
            for (std::size_t ch=0;ch<spec->audio_format.channel_count();++ch)
                out[ch]=*gain.value()==1.0 ? sample[ch] : sample[ch]*(*gain.value());
        }
        ++raw_emitted;
        return Result<std::array<double,2>>::success(out);
    }
};

TruePeakLimiterModule::TruePeakLimiterModule(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}
TruePeakLimiterModule::~TruePeakLimiterModule() = default;

Result<std::unique_ptr<TruePeakLimiterModule>>
TruePeakLimiterModule::create(const ModuleDescriptor& descriptor,
    TruePeakLimiterParameters p)
{
    if (descriptor.type_id() != "rgsml.dsp.true-peak-limiter"
        || descriptor.algorithm_version() != std::optional<std::string_view>{"1.0.0"}
        || descriptor.parameter_schema_id() !=
            std::optional<std::string_view>{"rgsml.dsp.true-peak-limiter.parameters/1.0.0"}) {
        return Result<std::unique_ptr<TruePeakLimiterModule>>::failure(fail(
            ErrorCode::InvalidArgument,"INVALID_M17_DESCRIPTOR",
            "M17 module requires frozen descriptor version and Ceiling schema."));
    }
    try {
        return Result<std::unique_ptr<TruePeakLimiterModule>>::success(
            std::unique_ptr<TruePeakLimiterModule>(
                new TruePeakLimiterModule(std::make_unique<Impl>(descriptor,p))));
    } catch (...) {
        return Result<std::unique_ptr<TruePeakLimiterModule>>::failure(fail(
            ErrorCode::InvalidState,"M17_CREATE_FAILURE","M17 module allocation failed."));
    }
}
const ModuleDescriptor& TruePeakLimiterModule::descriptor() const noexcept
{
    return impl_->descriptor;
}
const TruePeakLimiterParameters& TruePeakLimiterModule::parameters() const noexcept
{
    return impl_->params;
}
Result<DspRuntimeRequirements>
TruePeakLimiterModule::runtime_requirements(const DspProcessSpec& spec) const
{
    if (auto s=validate(spec);!s)
        return Result<DspRuntimeRequirements>::failure(*s.error());
    auto ctr=TruePeakLimiterControllerState::create(
        spec.audio_format.sample_rate().value(),impl_->params.ceiling_db_tp());
    if (!ctr) return Result<DspRuntimeRequirements>::failure(*ctr.error());
    const auto count=[](std::int64_t n) {return *core::FrameCount::create(n).value();};
    return Result<DspRuntimeRequirements>::success(DspRuntimeRequirements{
        DspExecutionModel::STREAMING_CAUSAL,
        count(ctr.value()->delay_frames()),
        count(ctr.value()->lookahead_frames()),
        count(0),count(0),count(ctr.value()->delay_frames()),false});
}
Status TruePeakLimiterModule::prepare(const DspProcessSpec& spec)
{
    if (auto s=validate(spec);!s) return s;
    auto ctr=TruePeakLimiterControllerState::create(
        spec.audio_format.sample_rate().value(),impl_->params.ceiling_db_tp());
    if (!ctr) return Status::failure(*ctr.error());
    try {
        impl_->spec=spec;
        impl_->d=ctr.value()->delay_frames();
        impl_->controller=*ctr.value();
        impl_->detector.emplace(
            static_cast<std::uint32_t>(spec.audio_format.channel_count()));
        impl_->reset_stream();
        return Status::success();
    } catch (...) {
        reset();
        return Status::failure(fail(ErrorCode::InvalidState,
            "M17_PREPARE_FAILURE","M17 module prepare allocation failed."));
    }
}
void TruePeakLimiterModule::reset() noexcept
{
    impl_->reset_stream();
}
Status TruePeakLimiterModule::process(audio::AudioBufferView input,
    audio::MutableAudioBufferView output,const DspProcessContext& ctx)
{
    try {
        if (!impl_->spec || !impl_->controller || !impl_->detector
            || impl_->detector_drained || impl_->finished) {
            return Status::failure(fail(ErrorCode::InvalidState,
                "INVALID_M17_PROCESS_STATE","M17 must be prepared and not finalized."));
        }
        const auto& spec=*impl_->spec;
        const auto length=input.frame_count().value();
        auto requested=ctx.output_frame_range.length();
        if (!requested || length<=0 || length>spec.maximum_block_frames.value()
            || requested.value()->value()!=length
            || output.frame_count().value()!=length
            || input.format()!=spec.audio_format || output.format()!=spec.audio_format
            || input.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
            || output.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
            || input.absolute_range()!=ctx.output_frame_range
            || output.absolute_range()!=ctx.output_frame_range
            || ctx.ends_stream || (!impl_->bound && !ctx.begins_stream)
            || (impl_->bound && (ctx.begins_stream ||
                ctx.output_frame_range.begin().value()!=impl_->origin+impl_->raw_emitted))
            || length>std::numeric_limits<std::int64_t>::max()-impl_->frame_count-impl_->d) {
            return Status::failure(fail(ErrorCode::InvalidArgument,
                "INVALID_M17_PROCESS_CONTEXT","Invalid/discontinuous OUTPUT_RATE process block."));
        }
        std::array<std::span<const double>,2> src{};
        std::array<std::span<double>,2> dst{};
        for (std::size_t ch=0;ch<spec.audio_format.channel_count();++ch) {
            auto in=input.channel(ch);
            auto out=output.channel(ch);
            if (!in || !out) return Status::failure(fail(ErrorCode::InvalidArgument,
                "INVALID_M17_BUFFER","Missing input/output channel."));
            src[ch]=*in.value();
            dst[ch]=*out.value();
            for (const auto x:src[ch]) if (!std::isfinite(x))
                return Status::failure(fail(ErrorCode::InvalidAudioSample,
                    "INVALID_AUDIO_SAMPLE","M17 nonfinite input is rejected."));
        }
        // No partial output/state mutation if any detector/control operation fails.
        Impl candidate=*impl_;
        if (!candidate.bound) {
            candidate.origin=ctx.output_frame_range.begin().value();
            candidate.bound=true;
        }
        std::array<std::vector<double>,2> staged;
        for (std::size_t ch=0;ch<spec.audio_format.channel_count();++ch)
            staged[ch].reserve(static_cast<std::size_t>(length));
        for (std::int64_t i=0;i<length;++i) {
            const double left=src[0][static_cast<std::size_t>(i)];
            const double right=spec.audio_format.channel_count()==2
                ? src[1][static_cast<std::size_t>(i)] : +0.0;
            auto quarters=candidate.detector->push_programme_frame(left,right);
            if (!quarters) return Status::failure(*quarters.error());
            for (const auto& q:*quarters.value()) {
                auto status=candidate.append(q);
                if (!status)return status;
            }
            candidate.pcm_delay.push_back({left,right});
            ++candidate.frame_count;
            auto raw=candidate.emit_raw();
            if (!raw)return Status::failure(*raw.error());
            for (std::size_t ch=0;ch<spec.audio_format.channel_count();++ch)
                staged[ch].push_back((*raw.value())[ch]);
        }
        *impl_=std::move(candidate);
        for (std::size_t ch=0;ch<spec.audio_format.channel_count();++ch)
            std::copy(staged[ch].begin(),staged[ch].end(),dst[ch].begin());
        return Status::success();
    } catch (...) {
        return Status::failure(fail(ErrorCode::InvalidState,
            "M17_PROCESS_FAILURE","M17 process failed without committing output."));
    }
}
Status TruePeakLimiterModule::finalize(audio::MutableAudioBufferView output,
    const DspProcessContext& ctx)
{
    try {
        if (!impl_->spec || !impl_->bound || !impl_->detector || !impl_->controller
            || impl_->finished || impl_->frame_count<=0) {
            return Status::failure(fail(ErrorCode::InvalidState,
                "INVALID_M17_FINALIZE","M17 finalize requires nonempty active stream."));
        }
        const auto length=output.frame_count().value();
        const auto remaining=impl_->d-impl_->drain_emitted;
        auto requested=ctx.output_frame_range.length();
        if (!requested || length<=0
            || length>impl_->spec->maximum_block_frames.value()
            || length>remaining || requested.value()->value()!=length
            || output.format()!=impl_->spec->audio_format
            || output.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
            || output.absolute_range()!=ctx.output_frame_range
            || ctx.output_frame_range.begin().value()!=impl_->origin+impl_->raw_emitted
            || ctx.begins_stream || (ctx.ends_stream!=(length==remaining))) {
            return Status::failure(fail(ErrorCode::InvalidArgument,
                "INVALID_M17_FINALIZE_CONTEXT","M17 true EOS drain is not exact/contiguous."));
        }
        std::array<std::span<double>,2> dst{};
        for(std::size_t ch=0;ch<impl_->spec->audio_format.channel_count();++ch) {
            auto out=output.channel(ch);
            if(!out)return Status::failure(fail(ErrorCode::InvalidArgument,
                "INVALID_M17_BUFFER","M17 finalize output channel missing."));
            dst[ch]=*out.value();
        }
        Impl candidate=*impl_;
        if(!candidate.detector_drained) {
            auto tail=candidate.detector->finalize_detector();
            if(!tail)return Status::failure(*tail.error());
            for(const auto& quarter:*tail.value()) {
                auto status=candidate.append(quarter);
                if(!status)return status;
            }
            candidate.detector_drained=true;
        }
        std::array<std::vector<double>,2> staged;
        for(std::size_t ch=0;ch<candidate.spec->audio_format.channel_count();++ch)
            staged[ch].reserve(static_cast<std::size_t>(length));
        for(std::int64_t i=0;i<length;++i) {
            auto raw=candidate.emit_raw();
            if(!raw)return Status::failure(*raw.error());
            for(std::size_t ch=0;ch<candidate.spec->audio_format.channel_count();++ch)
                staged[ch].push_back((*raw.value())[ch]);
        }
        candidate.drain_emitted+=length;
        if(ctx.ends_stream) {
            if(candidate.drain_emitted!=candidate.d || !candidate.pcm_delay.empty())
                return Status::failure(fail(ErrorCode::InvalidState,
                    "INVALID_M17_DRAIN","Limiter did not emit precisely N real programme frames."));
            candidate.finished=true;
        }
        *impl_=std::move(candidate);
        for(std::size_t ch=0;ch<impl_->spec->audio_format.channel_count();++ch)
            std::copy(staged[ch].begin(),staged[ch].end(),dst[ch].begin());
        return Status::success();
    } catch (...) {
        return Status::failure(fail(ErrorCode::InvalidState,
            "M17_FINALIZE_FAILURE","M17 finalize failed without committing output."));
    }
}
} // namespace rgsml::dsp
