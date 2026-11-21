#pragma once
// RGS MasterLab R1d/I1.6 — isolated proof against the *actual* IModule ABI.
// Never add this header to a product target, ModuleRegistry, QML or M17.
// The configured module belongs to exactly one mono/stereo SOURCE_RATE stream.
// prepare/reset are off-callback operations and require host quiescence.

#include "p2_planar_host_bridge.hpp"
#include "p2_epoch_session.hpp"
#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>

namespace rgsml::research::r1d_p2 {

class ResearchP2IModule final : public dsp::IModule {
public:
    explicit ResearchP2IModule(dsp::ModuleDescriptor descriptor,
                              Configuration configuration, bool bypass) noexcept
        : descriptor_(std::move(descriptor)), config_(configuration), bypass_(bypass) {}

    [[nodiscard]] const dsp::ModuleDescriptor& descriptor() const noexcept override {
        return descriptor_;
    }

    [[nodiscard]] core::Result<dsp::DspRuntimeRequirements>
    runtime_requirements(const dsp::DspProcessSpec& spec) const override {
        if (!valid_spec(spec))
            return core::Result<dsp::DspRuntimeRequirements>::failure(
                core::Error{core::ErrorCode::InvalidArgument});
        // These are control-thread requirements, not callback work.
        CallbackEngine probe;
        auto cfg = effective_config(spec);
        if (probe.prepare(cfg) != Status::ok)
            return core::Result<dsp::DspRuntimeRequirements>::failure(
                core::Error{core::ErrorCode::UnsupportedOperation});
        const auto d = core::FrameCount::create(
            static_cast<std::int64_t>(probe.latency_frames()));
        const auto zero = core::FrameCount::create(0);
        if (!d || !zero)
            return core::Result<dsp::DspRuntimeRequirements>::failure(
                core::Error{core::ErrorCode::InvalidState});
        return core::Result<dsp::DspRuntimeRequirements>::success({
            dsp::DspExecutionModel::STREAMING_CAUSAL,
            *d.value(), *core::FrameCount::create(d.value()->value()-63).value(),
            *zero.value(), *zero.value(),
            *d.value(), false});
    }

    [[nodiscard]] core::Status prepare(const dsp::DspProcessSpec& spec) override {
        if (!valid_spec(spec) || epoch_ == UINT64_MAX) return failure(core::ErrorCode::InvalidArgument);
        const auto cfg=effective_config(spec);
        // All heap work is confined to prepare; the active epoch is recreated
        // transactionally, without mutating the prior one on failure.
        if (bridge_.begin_epoch(epoch_+1U,cfg,bypass_,0) != HostBridgeStatus::ok)
            return failure(core::ErrorCode::InvalidState);
        ++epoch_;
        spec_ = spec;
        latency_ = bridge_.latency_frames();
        clear_clocks();
        return core::Status::success();
    }

    void reset() noexcept override {
        if (!spec_ || epoch_ == UINT64_MAX) {
            prepared_=false;
            return;
        }
        // Contract: host invokes reset OFF the callback and with no in-flight
        // blocks.  A failed rearm leaves the module fail-closed.
        const auto rc=bridge_.begin_epoch(epoch_+1U,effective_config(*spec_),bypass_,0);
        if(rc != HostBridgeStatus::ok){prepared_=false;return;}
        ++epoch_;
        latency_=bridge_.latency_frames();
        clear_clocks();
    }

    [[nodiscard]] core::Status process(audio::AudioBufferView input,
            audio::MutableAudioBufferView output,
            const dsp::DspProcessContext& context) override {
        if (!prepared_ || !spec_ || failed_ || finishing_ || finalized_) {
            silence(output);return failure(core::ErrorCode::InvalidState);
        }
        const auto n=input.frame_count().value();
        const auto requested=context.output_frame_range.length();
        if (!requested || n<=0 || n>spec_->maximum_block_frames.value() ||
            n>static_cast<std::int64_t>(PlanarHostBridge<EpochSession,Configuration,EpochStatus>::kMaxBlock) ||
            requested.value()->value()!=n || output.frame_count().value()!=n ||
            input.format()!=spec_->audio_format || output.format()!=spec_->audio_format ||
            input.timebase().frame_domain_id()!=spec_->frame_domain_id ||
            output.timebase().frame_domain_id()!=spec_->frame_domain_id ||
            input.absolute_range()!=context.output_frame_range ||
            output.absolute_range()!=context.output_frame_range ||
            (bound_ && context.begins_stream) ||
            (!bound_ && !context.begins_stream) ||
            (bound_ && context.output_frame_range.begin().value()!=next_frame_) ||
            context.output_frame_range.begin().value()<0 ||
            context.output_frame_range.end().value() >
                std::numeric_limits<std::int64_t>::max()-static_cast<std::int64_t>(latency_)) {
            silence(output);return failure(core::ErrorCode::InvalidArgument);
        }
        const auto channels=spec_->audio_format.channel_count();
        const auto in0=input.channel(0);
        const auto out0=output.channel(0);
        if(!in0||!out0){silence(output);return failure(core::ErrorCode::InvalidArgument);}
        std::span<const double> in1;
        std::span<double> out1;
        if(channels==2) {
            const auto lhs=input.channel(1);
            const auto rhs=output.channel(1);
            if(!lhs||!rhs){silence(output);return failure(core::ErrorCode::InvalidArgument);}
            in1=*lhs.value();out1=*rhs.value();
        }
        if(!bound_) {
            if(!bridge_.bind_first_absolute_frame(epoch_,context.output_frame_range.begin().value())) {
                silence(output);failed_=true;return failure(core::ErrorCode::InvalidState);
            }
        }
        const auto rc=bridge_.process(epoch_,context.output_frame_range.begin().value(),
                *in0.value(),in1,*out0.value(),out1,
                !bound_,context.ends_stream);
        if (rc != HostBridgeStatus::ok) {
            failed_=true;silence(output);return failure(core::ErrorCode::InvalidState);
        }
        bound_=true;
        finishing_=context.ends_stream;
        next_frame_=context.output_frame_range.end().value();
        return core::Status::success();
    }

    // RGS calls finalize in 1..maxBlock chunks. P2's pinned session instead
    // requires one exact D-frame tail; stage once in fixed arrays, then copy
    // to caller chunks. Absolutely no analyzer-only M16 zeros are published.
    [[nodiscard]] core::Status finalize(audio::MutableAudioBufferView output,
                       const dsp::DspProcessContext& context) override {
        if(!prepared_ || !spec_ || !bound_ || failed_ || finalized_) {
            silence(output);return failure(core::ErrorCode::InvalidState);
        }
        const auto n=output.frame_count().value();
        const auto remaining=static_cast<std::int64_t>(latency_)-tail_emitted_;
        const auto requested=context.output_frame_range.length();
        if(!requested || n<=0 || n>spec_->maximum_block_frames.value() ||
            n>remaining || requested.value()->value()!=n ||
            context.begins_stream || context.ends_stream!=(n==remaining) ||
            output.format()!=spec_->audio_format ||
            output.timebase().frame_domain_id()!=spec_->frame_domain_id ||
            output.absolute_range()!=context.output_frame_range ||
            context.output_frame_range.begin().value()!=next_frame_+tail_emitted_) {
            silence(output);return failure(core::ErrorCode::InvalidArgument);
        }
        const auto channels=spec_->audio_format.channel_count();
        const auto l=output.channel(0);
        if(!l){silence(output);return failure(core::ErrorCode::InvalidArgument);}
        std::span<double> r;
        if(channels==2) {
            const auto maybe=output.channel(1);
            if(!maybe){silence(output);return failure(core::ErrorCode::InvalidArgument);}
            r=*maybe.value();
        }
        if(!tail_ready_) {
            if(!finishing_) {
                // RGS terminal convention: process(...ends_stream=false)
                // until a finalize-only EOS. Mark EOS with a ZERO-length
                // bridge call; this does not push a sample into P2 history.
                std::span<const double> emptyIn{};
                std::span<double> emptyOut{};
                auto marker=bridge_.process(epoch_,next_frame_,emptyIn,emptyIn,
                    emptyOut,emptyOut,false,true);
                if(marker!=HostBridgeStatus::ok){
                    failed_=true;silence(output);return failure(core::ErrorCode::InvalidState);
                }
                finishing_=true;
            }
            const auto rc=bridge_.finish(epoch_,next_frame_,
                {tail_left_.data(),latency_},
                channels==2 ? std::span<double>{tail_right_.data(),latency_} : std::span<double>{});
            if(rc!=HostBridgeStatus::ok) {
                failed_=true;silence(output);return failure(core::ErrorCode::InvalidState);
            }
            tail_ready_=true;
        }
        const auto start=static_cast<std::size_t>(tail_emitted_);
        const auto count=static_cast<std::size_t>(n);
        std::copy_n(tail_left_.data()+start,count,l.value()->begin());
        if(channels==2)std::copy_n(tail_right_.data()+start,count,r.begin());
        tail_emitted_+=n;
        if(context.ends_stream)finalized_=true;
        return core::Status::success();
    }

private:
    [[nodiscard]] static core::Status failure(core::ErrorCode code) {
        // Empty message/details: no dynamic error string allocation during
        // a failed real-time callback, unlike an ordinary diagnostic Error.
        return core::Status::failure(core::Error{code});
    }
    [[nodiscard]] bool valid_spec(const dsp::DspProcessSpec& spec) const noexcept {
        if (spec.frame_domain_id != audio::FrameDomainId::SOURCE_PROCESSING_RATE ||
            spec.maximum_block_frames.value()<1 || spec.maximum_block_frames.value()>4096 ||
            (spec.audio_format.channel_layout()!=audio::ChannelLayout::MONO_C &&
             spec.audio_format.channel_layout()!=audio::ChannelLayout::STEREO_LR)) return false;
        const auto fs=spec.audio_format.sample_rate().value();
        return fs==44100 || fs==48000 || fs==88200 || fs==96000 ||
               fs==176400 || fs==192000;
    }
    [[nodiscard]] Configuration effective_config(const dsp::DspProcessSpec& spec) const noexcept {
        auto cfg=config_;
        cfg.sample_rate=static_cast<int>(spec.audio_format.sample_rate().value());
        cfg.channels=static_cast<int>(spec.audio_format.channel_count());
        return cfg;
    }
    static void silence(audio::MutableAudioBufferView out) noexcept {
        for(std::size_t c=0;c<out.format().channel_count();++c) {
            auto channel=out.channel(c);
            if(channel)std::fill(channel.value()->begin(),channel.value()->end(),0.0);
        }
    }
    void clear_clocks() noexcept {
        prepared_=true;bound_=false;finishing_=false;tail_ready_=false;
        finalized_=false;failed_=false;next_frame_=0;tail_emitted_=0;
    }
    dsp::ModuleDescriptor descriptor_;
    Configuration config_{};
    bool bypass_{false};
    PlanarHostBridge<EpochSession,Configuration,EpochStatus> bridge_{};
    std::optional<dsp::DspProcessSpec> spec_{};
    std::array<double,4096> tail_left_{};
    std::array<double,4096> tail_right_{};
    std::uint64_t epoch_{0};
    std::size_t latency_{0};
    std::int64_t next_frame_{0};
    std::int64_t tail_emitted_{0};
    bool prepared_{false},bound_{false},finishing_{false};
    bool tail_ready_{false},finalized_{false},failed_{false};
};

} // namespace rgsml::research::r1d_p2
