#include <rgsml/dsp/transient_shaper_module.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/core/error.hpp>
#include "internal/transient_shaper_c00_engine.hpp"
#include "internal/transient_shaper_effective_memory.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::dsp {
namespace {
constexpr auto kType="rgsml.dsp.transient-shaper";
constexpr auto kAlgorithm="1.0.0";
constexpr auto kParameters="rgsml.dsp.transient-shaper.parameters/1.0.0";
constexpr auto kCheckpoint="rgsml.dsp.transient-shaper.runtime-state/1.0.0";
constexpr auto kBackend="rgsml.dsp.backend.default.v1";
[[nodiscard]] core::Error fail(core::ErrorCode code,std::string category) {
    return core::Error{code,"L3_M03_TRANSIENT_SHAPER_MODULE_FAILURE",
        {{"category",std::move(category)}}};
}
[[nodiscard]] core::Status invalid(const char* category) {
    return core::Status::failure(
        fail(core::ErrorCode::InvalidArgument,category));
}
[[nodiscard]] bool domain(audio::FrameDomainId id) noexcept {
    return id==audio::FrameDomainId::SOURCE_PROCESSING_RATE
        ||id==audio::FrameDomainId::OUTPUT_RATE;
}
[[nodiscard]] core::Status validate(const DspProcessSpec& s) {
    const auto fs=s.audio_format.sample_rate().value();
    const auto layout=s.audio_format.channel_layout();
    if(!domain(s.frame_domain_id)||s.maximum_block_frames.value()<=0
       ||fs<=0||fs>std::numeric_limits<std::uint32_t>::max()
       ||(fs!=44100&&fs!=48000&&fs!=88200&&
        fs!=96000&&fs!=176400&&fs!=192000)
       ||(layout!=audio::ChannelLayout::MONO_C
          &&layout!=audio::ChannelLayout::STEREO_LR))
        return invalid("INVALID_PROCESS_SPEC");
    return core::Status::success();
}
[[nodiscard]] std::string hex(double value) {
    // Both +0/-0 are one effective signed-amount setting and must have
    // identical sonic identity, including neutral checkpoint compatibility.
    const auto bits=std::bit_cast<std::uint64_t>(
        value==0.0?0.0:value);
    constexpr char digits[]="0123456789abcdef";
    std::string result(16,'0');
    for(int i=0;i<16;++i)result[static_cast<std::size_t>(i)]=
        digits[(bits>>static_cast<unsigned>((15-i)*4))&15U];
    return result;
}
[[nodiscard]] std::string fingerprint(
    const TransientShaperParameters& p) {
    return "rgsml.dsp.transient-shaper.sonic/1;attack="+
        hex(p.attack_amount_db())+";sustain="+
        hex(p.sustain_amount_db());
}
void write_u64(std::vector<std::uint8_t>& out,std::uint64_t x) {
    for(unsigned i=0;i<8;++i)
        out.push_back(static_cast<std::uint8_t>((x>>(8*i))&255U));
}
[[nodiscard]] std::optional<std::uint64_t> read_u64(
    const std::vector<std::uint8_t>& bytes,std::size_t& pos) noexcept {
    if(pos>bytes.size()||bytes.size()-pos<8U)return std::nullopt;
    std::uint64_t value=0;
    for(unsigned i=0;i<8;++i)
        value|=static_cast<std::uint64_t>(bytes[pos++])<<(8*i);
    return value;
}
} // namespace

struct TransientShaperModule::Impl final {
    const ModuleDescriptor* descriptor;
    TransientShaperParameters params;
    std::optional<DspProcessSpec> spec;
    std::unique_ptr<internal::TransientShaperC00Engine> engine;
    std::optional<core::FrameIndex> next;
    bool finalized{false};
};
TransientShaperModule::TransientShaperModule(std::unique_ptr<Impl> impl) noexcept
    :impl_(std::move(impl)){}
TransientShaperModule::~TransientShaperModule()=default;
core::Result<std::unique_ptr<TransientShaperModule>>
TransientShaperModule::create(
    const ModuleDescriptor& descriptor,TransientShaperParameters params)
{
    using R=core::Result<std::unique_ptr<TransientShaperModule>>;
    if(descriptor.type_id()!=kType
       ||descriptor.algorithm_version()!=std::optional<std::string_view>{kAlgorithm}
       ||descriptor.parameter_schema_id()!=std::optional<std::string_view>{kParameters})
        return R::failure(fail(core::ErrorCode::InvalidArgument,
            "INCOMPATIBLE_FROZEN_MODULE_DESCRIPTOR"));
    try {
        auto impl=std::make_unique<Impl>(
            Impl{&descriptor,params,std::nullopt,nullptr,std::nullopt,false});
        return R::success(std::unique_ptr<TransientShaperModule>(
            new TransientShaperModule(std::move(impl))));
    }catch(...) {
        return R::failure(fail(core::ErrorCode::InvalidState,
            "MODULE_CONSTRUCTION_ALLOCATION_FAILED"));
    }
}
const ModuleDescriptor& TransientShaperModule::descriptor() const noexcept {
    return *impl_->descriptor;
}
const TransientShaperParameters&
TransientShaperModule::parameters() const noexcept {
    return impl_->params;
}
core::Result<DspRuntimeRequirements>
TransientShaperModule::runtime_requirements(const DspProcessSpec& spec) const
{
    using R=core::Result<DspRuntimeRequirements>;
    const auto v=validate(spec);
    if(!v)return R::failure(*v.error());
    const auto h=internal::transient_shaper_conservative_precontext_frames(
        static_cast<std::uint32_t>(
            spec.audio_format.sample_rate().value()));
    if(!h)return R::failure(*h.error());
    const auto zero=core::FrameCount::create(0);
    if(!zero)return R::failure(*zero.error());
    // P07 is a conservative IDEAL-ARITHMETIC metadata bound. An exact
    // random-access audio null/cropped-prefix theorem is NOT implied.
    return R::success(DspRuntimeRequirements{
        DspExecutionModel::STREAMING_CAUSAL,
        *zero.value(),*zero.value(),*h.value(),
        *zero.value(),*zero.value(),false});
}
core::Status TransientShaperModule::prepare(const DspProcessSpec& spec)
{
    const auto v=validate(spec);
    if(!v)return v;
    try {
        auto engine=internal::TransientShaperC00Engine::create(
            impl_->params,
            static_cast<std::uint32_t>(
                spec.audio_format.sample_rate().value()),
            static_cast<std::uint32_t>(
                spec.audio_format.channel_count()),
            static_cast<std::size_t>(spec.maximum_block_frames.value()));
        if(!engine)return core::Status::failure(*engine.error());
        impl_->engine=std::make_unique<internal::TransientShaperC00Engine>(
            std::move(*engine.value()));
        impl_->spec=spec;
        impl_->next.reset();
        impl_->finalized=false;
        return core::Status::success();
    }catch(...) {
        return core::Status::failure(fail(
            core::ErrorCode::InvalidState,"PREPARE_FAILURE"));
    }
}
void TransientShaperModule::reset() noexcept {
    if(impl_->engine)impl_->engine->reset();
    impl_->next.reset();impl_->finalized=false;
}
core::Status TransientShaperModule::process(
    audio::AudioBufferView input,audio::MutableAudioBufferView output,
    const DspProcessContext& context)
{
    try {
        if(!impl_->spec||!impl_->engine)
            return core::Status::failure(fail(
                core::ErrorCode::InvalidState,"MODULE_NOT_PREPARED"));
        if(impl_->finalized)return invalid("PROCESS_AFTER_TRUE_EOS");
        const auto& spec=*impl_->spec;
        if(input.format()!=spec.audio_format
           ||output.format()!=spec.audio_format
           ||input.timebase().frame_domain_id()!=spec.frame_domain_id
           ||output.timebase().frame_domain_id()!=spec.frame_domain_id
           ||input.timebase().sample_rate()!=spec.audio_format.sample_rate()
           ||output.timebase().sample_rate()!=spec.audio_format.sample_rate()
           ||input.absolute_range()!=context.output_frame_range
           ||output.absolute_range()!=context.output_frame_range
           ||input.frame_count()!=output.frame_count()
           ||input.frame_count().value()<=0
           ||input.frame_count().value()>spec.maximum_block_frames.value()
           ||context.output_frame_range.begin().value()<0
           ||context.output_frame_range.end().value()<0)
            return invalid("INVALID_PROCESS_CONTEXT");
        if(!impl_->next) {
            if(!context.begins_stream)return invalid("MISSING_BEGIN_STREAM");
        }else if(context.begins_stream ||
                 context.output_frame_range.begin()!=*impl_->next)
            return invalid("NONCONTIGUOUS_OR_REPEATED_BEGIN");
        const auto in0=input.channel(0);
        const auto out0=output.channel(0);
        if(!in0||!out0)return invalid("INVALID_CHANNEL_ZERO");
        std::span<const double> in1;
        std::span<double> out1;
        if(spec.audio_format.channel_count()==2) {
            const auto l=input.channel(1);
            const auto r=output.channel(1);
            if(!l||!r)return invalid("INVALID_CHANNEL_ONE");
            in1=*l.value();out1=*r.value();
        }
        const auto status=impl_->engine->process_block(
            *in0.value(),in1,*out0.value(),out1);
        if(!status)return status;
        impl_->next=context.output_frame_range.end();
        impl_->finalized=context.ends_stream;
        return core::Status::success();
    }catch(...) {
        return core::Status::failure(fail(
            core::ErrorCode::InvalidState,"PROCESS_EXCEPTION"));
    }
}
core::Result<DspRuntimeCheckpoint>
TransientShaperModule::runtime_checkpoint() const
{
    using R=core::Result<DspRuntimeCheckpoint>;
    if(!impl_->spec||!impl_->engine)
        return R::failure(fail(core::ErrorCode::InvalidState,
            "MODULE_NOT_PREPARED"));
    if(impl_->finalized)
        return R::failure(fail(core::ErrorCode::UnsupportedOperation,
            "RUNTIME_CHECKPOINT_DURING_FINALIZE_UNSUPPORTED"));
    try {
        DspRuntimeCheckpoint cp;
        cp.module_type_id=kType;
        cp.algorithm_version=kAlgorithm;
        cp.parameter_schema_id=kParameters;
        cp.sonic_fingerprint=fingerprint(impl_->params);
        cp.audio_format=impl_->spec->audio_format;
        cp.frame_domain_id=impl_->spec->frame_domain_id;
        cp.checkpoint_schema_version=kCheckpoint;
        cp.backend_identity=kBackend;
        cp.next_input_frame=impl_->next;
        cp.payload.reserve(48);
        write_u64(cp.payload,impl_->next
            ?static_cast<std::uint64_t>(impl_->next->value())
            :std::numeric_limits<std::uint64_t>::max());
        const auto& s=impl_->engine->state();
        for(const double d:{s.f,s.m,s.l,s.g_attack_db,s.g_sustain_db})
            write_u64(cp.payload,std::bit_cast<std::uint64_t>(d));
        return R::success(std::move(cp));
    }catch(...) {
        return R::failure(fail(core::ErrorCode::InvalidState,
            "CHECKPOINT_SERIALIZE_FAILURE"));
    }
}
core::Status TransientShaperModule::restore_runtime_checkpoint(
    const DspRuntimeCheckpoint& cp)
{
    if(!impl_->spec||!impl_->engine)
        return core::Status::failure(fail(
            core::ErrorCode::InvalidState,"MODULE_NOT_PREPARED"));
    if(impl_->finalized)return invalid("RESTORE_AFTER_TRUE_EOS");
    try {
        if(cp.module_type_id!=kType
           ||cp.algorithm_version!=kAlgorithm
           ||cp.parameter_schema_id!=kParameters
           ||cp.sonic_fingerprint!=fingerprint(impl_->params)
           ||cp.audio_format!=impl_->spec->audio_format
           ||cp.frame_domain_id!=impl_->spec->frame_domain_id
           ||cp.checkpoint_schema_version!=kCheckpoint
           ||cp.backend_identity!=kBackend
           ||(cp.next_input_frame&&cp.next_input_frame->value()<0)
           ||(impl_->next&&impl_->next!=cp.next_input_frame))
            return invalid("CHECKPOINT_INCOMPATIBLE_METADATA");
        if(cp.payload.size()!=48U)return invalid("CHECKPOINT_PAYLOAD_SIZE");
        std::size_t pos=0;
        const auto epoch=read_u64(cp.payload,pos);
        const auto expected=cp.next_input_frame
            ?static_cast<std::uint64_t>(cp.next_input_frame->value())
            :std::numeric_limits<std::uint64_t>::max();
        if(!epoch||*epoch!=expected)
            return invalid("CHECKPOINT_EPOCH_MISMATCH");
        std::array<double,5> fields{};
        for(auto& value:fields) {
            const auto bits=read_u64(cp.payload,pos);
            if(!bits)return invalid("CHECKPOINT_TRUNCATED");
            if(!cp.next_input_frame && *bits!=0U)
                return invalid("UNBOUND_CHECKPOINT_STATE_NOT_ZERO");
            value=std::bit_cast<double>(*bits);
        }
        if(pos!=cp.payload.size())
            return invalid("CHECKPOINT_EXTRA_PAYLOAD");
        const internal::TransientShaperC00State candidate{
            fields[0],fields[1],fields[2],fields[3],fields[4]};
        const auto restored=impl_->engine->restore_state(candidate);
        if(!restored)return restored;
        impl_->next=cp.next_input_frame;
        impl_->finalized=false;
        return core::Status::success();
    }catch(...) {
        return core::Status::failure(fail(
            core::ErrorCode::InvalidArgument,"CHECKPOINT_PARSE_FAILURE"));
    }
}
} // namespace rgsml::dsp
