#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>

#include <rgsml/core/checked_integer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/dsp/imodule.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::render {
namespace {
using core::Result;
using core::Error;
using core::ErrorCode;
using core::FrameIndex;
using core::FrameCount;
using core::FrameRange;

[[nodiscard]] Error candidate_error(
    ErrorCode code, const char* category, const char* message)
{
    return Error{code, message, {{"category", category}}};
}

// M17 callback is noexcept. Reserve the exact N witness capacity BEFORE
// attaching; trivially-copyable witness append can never allocate here.
void record_applied_gain(
    void* opaque,
    std::span<const dsp::TruePeakLimiterAppliedGainFrame> records) noexcept
{
    auto& frames = *static_cast<std::vector<dsp::TruePeakLimiterAppliedGainFrame>*>(opaque);
    for (const auto& record : records) {
        // Overflow is an invariant violation; do not corrupt a full vector.
        if (frames.size() < frames.capacity()) frames.push_back(record);
    }
}
} // namespace

Result<TruePeakLimiterTerminalCandidate>
render_true_peak_limiter_terminal_candidate(
    audio::AudioBufferView input,
    dsp::TruePeakLimiterParameters params,
    const dsp::ModuleRegistry& registry,
    FrameCount max_block,
    bool bypassed)
{
    if (input.timebase().frame_domain_id() != audio::FrameDomainId::OUTPUT_RATE
        || input.frame_count().value() <= 0 || max_block.value() <= 0) {
        return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
            ErrorCode::InvalidArgument, "M17_TERMINAL_INPUT_UNAVAILABLE",
            "Full nonempty OUTPUT_RATE PCM and a positive block size are mandatory."));
    }
    const auto count = input.frame_count().value();
    const auto origin = input.absolute_start_frame().value();
    const auto outputEnd = core::checked_add(origin, count);
    if (!outputEnd || origin < 0) {
        return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
            ErrorCode::OutOfRange, "M17_TERMINAL_FRAME_OVERFLOW",
            "M17 source timeline is outside the representable frame range."));
    }

    // A bypass must not smuggle nonfinite PCM into a supposedly valid
    // realization. Check EVERY channel before writing either output lane.
    for (std::size_t channel=0;channel<input.format().channel_count();++channel) {
        const auto lane=input.channel(channel);
        if (!lane)
            return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
                ErrorCode::InvalidArgument,"M17_TERMINAL_CHANNEL_UNAVAILABLE",
                "Complete terminal PCM requires every channel."));
        for (const double sample:*lane.value())
            if (!std::isfinite(sample))
                return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
                    ErrorCode::InvalidAudioSample,"M17_TERMINAL_NONFINITE_PCM",
                    "Terminal PCM contains a nonfinite sample."));
    }

    try {
        auto output = audio::AudioBuffer::create(input.format(),
            audio::FrameDomainId::OUTPUT_RATE, FrameIndex{origin},
            input.frame_count());
        if (!output) return Result<TruePeakLimiterTerminalCandidate>::failure(*output.error());

        if (bypassed) {
            for (std::size_t channel=0; channel<input.format().channel_count(); ++channel) {
                const auto src = input.channel(channel);
                auto dst = output.value()->mutable_view().channel(channel);
                if (!src || !dst)
                    return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
                        ErrorCode::InvalidState, "M17_BYPASS_CHANNEL_UNAVAILABLE",
                        "BYP requires every input and output channel."));
                // No arithmetic: signed zero and subnormal bits survive.
                std::copy(src.value()->begin(), src.value()->end(), dst.value()->begin());
            }
            return Result<TruePeakLimiterTerminalCandidate>::success({
                std::move(*output.value()), {}, 0, true});
        }

        // Enforce immutable typed Ceiling; never fall back to generic/default
        // factory when the M17 execution payload is unavailable.
        auto registered = registry.create_module(
            "rgsml.dsp.true-peak-limiter", dsp::ModuleParameterPayload{params});
        if (!registered)
            return Result<TruePeakLimiterTerminalCandidate>::failure(*registered.error());
        auto& module = **registered.value();
        auto* limiter = dynamic_cast<dsp::TruePeakLimiterModule*>(&module);
        if (limiter == nullptr)
            return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
                ErrorCode::UnsupportedOperation, "M17_TERMINAL_FACTORY_MISMATCH",
                "The registry did not return the real typed M17 limiter."));

        const dsp::DspProcessSpec spec{
            input.format(), audio::FrameDomainId::OUTPUT_RATE, max_block};
        auto requirements = limiter->runtime_requirements(spec);
        if (!requirements)
            return Result<TruePeakLimiterTerminalCandidate>::failure(*requirements.error());
        const auto& req = *requirements.value();
        const auto d = req.algorithmic_latency_frames.value();
        if (req.execution_model != dsp::DspExecutionModel::STREAMING_CAUSAL
            || req.requires_prepass || d <= 0
            || d != req.effective_tail_frames.value())
            return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
                ErrorCode::UnsupportedOperation, "M17_TERMINAL_LATENCY_CONTRACT",
                "Terminal M17 must have an exact causal D-frame EOS drain."));

        const auto rawCount = core::checked_add(count, d);
        if (!rawCount)
            return Result<TruePeakLimiterTerminalCandidate>::failure(*rawCount.error());
        const auto rawEnd = core::checked_add(origin, *rawCount.value());
        if (!rawEnd)
            return Result<TruePeakLimiterTerminalCandidate>::failure(*rawEnd.error());
        const auto rawFrames = FrameCount::create(*rawCount.value());
        if (!rawFrames)
            return Result<TruePeakLimiterTerminalCandidate>::failure(*rawFrames.error());
        auto raw = audio::AudioBuffer::create(input.format(),
            audio::FrameDomainId::OUTPUT_RATE, FrameIndex{origin},
            *rawFrames.value());
        if (!raw) return Result<TruePeakLimiterTerminalCandidate>::failure(*raw.error());

        std::vector<dsp::TruePeakLimiterAppliedGainFrame> gain;
        gain.reserve(static_cast<std::size_t>(count));
        auto prepared = limiter->prepare(spec);
        if (!prepared)
            return Result<TruePeakLimiterTerminalCandidate>::failure(*prepared.error());
        limiter->set_applied_gain_sink(&record_applied_gain, &gain);
        for (std::int64_t pos=0; pos<count;) {
            const auto part = std::min(max_block.value(), count-pos);
            const auto begin = origin+pos;
            const auto size = *FrameCount::create(part).value();
            auto range = FrameRange::create(FrameIndex{begin},FrameIndex{begin+part});
            auto in = input.subview(FrameIndex{begin},size);
            auto out = raw.value()->mutable_view().subview(FrameIndex{begin},size);
            if (!range || !in || !out)
                return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
                    ErrorCode::InvalidState, "M17_TERMINAL_PROCESS_SLICE",
                    "M17 failed to bind a continuous programme block."));
            auto processed = limiter->process(*in.value(), *out.value(),
                dsp::DspProcessContext{*range.value(),pos==0,false});
            if (!processed)
                return Result<TruePeakLimiterTerminalCandidate>::failure(*processed.error());
            pos += part;
        }
        for (std::int64_t emitted=0;emitted<d;) {
            const auto part=std::min(max_block.value(),d-emitted);
            const auto begin=*outputEnd.value()+emitted;
            const auto size=*FrameCount::create(part).value();
            auto range=FrameRange::create(FrameIndex{begin},FrameIndex{begin+part});
            auto out=raw.value()->mutable_view().subview(FrameIndex{begin},size);
            if (!range || !out)
                return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
                    ErrorCode::InvalidState, "M17_TERMINAL_DRAIN_SLICE",
                    "M17 failed to bind the complete exact EOS drain."));
            auto finalized=limiter->finalize(*out.value(),
                dsp::DspProcessContext{*range.value(),false,emitted+part==d});
            if (!finalized)
                return Result<TruePeakLimiterTerminalCandidate>::failure(*finalized.error());
            emitted += part;
        }
        limiter->set_applied_gain_sink(nullptr,nullptr);
        if (gain.size()!=static_cast<std::size_t>(count))
            return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
                ErrorCode::InvalidState, "M17_TERMINAL_INCOMPLETE_GAIN_PROVENANCE",
                "Limiter did not supply one actual gain observation per programme frame."));
        for (std::int64_t i=0;i<count;++i) {
            const auto& w=gain[static_cast<std::size_t>(i)];
            if (w.programme_frame!=origin+i || w.raw_output_frame!=origin+d+i
                || !std::isfinite(w.gain) || w.gain<=0 || w.gain>1
                || !std::isfinite(w.reduction_db) || w.reduction_db<0)
                return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
                    ErrorCode::InvalidState, "M17_TERMINAL_INVALID_GAIN_PROVENANCE",
                    "Limiter published misaligned or invalid gain observations."));
        }
        for (std::size_t ch=0;ch<input.format().channel_count();++ch) {
            auto src=raw.value()->view().channel(ch);
            auto dst=output.value()->mutable_view().channel(ch);
            if (!src || !dst)
                return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
                    ErrorCode::InvalidState, "M17_TERMINAL_OUTPUT_UNAVAILABLE",
                    "M17 compensated PCM was not readable on every channel."));
            std::copy(src.value()->begin()+d,
                src.value()->begin()+d+count, dst.value()->begin());
        }
        // This result is deliberately PRE-VERIFICATION-TRIM and cannot be
        // exposed to audition/export or labelled final True Peak safe.
        return Result<TruePeakLimiterTerminalCandidate>::success({
            std::move(*output.value()), std::move(gain), d, false});
    } catch (const std::exception&) {
        return Result<TruePeakLimiterTerminalCandidate>::failure(candidate_error(
            ErrorCode::InvalidState, "M17_TERMINAL_CANDIDATE_FAILURE",
            "Unqualified M17 terminal candidate failed without publishing PCM."));
    }
}
} // namespace rgsml::render
