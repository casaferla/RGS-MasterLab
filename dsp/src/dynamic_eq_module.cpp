#include <rgsml/dsp/dynamic_eq_module.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/core/error.hpp>
#include "internal/dynamic_eq_c01_engine.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::dsp {
namespace {
constexpr auto kType="rgsml.dsp.dynamic-eq";
constexpr auto kAlgorithm="1.0.0";
constexpr auto kParameters="rgsml.dsp.dynamic-eq.parameters/1.0.0";
constexpr auto kCheckpoint="rgsml.dsp.dynamic-eq.checkpoint/1.0.0";
constexpr auto kBackend="rgsml.dsp.backend.default.v1";

[[nodiscard]] core::Error bad(core::ErrorCode code,std::string category) {
    return core::Error{code,"L3_M01_DYNAMIC_EQ_MODULE_FAILURE",
                       {{"category",std::move(category)}}};
}
[[nodiscard]] core::Status invalid(std::string reason) {
    return core::Status::failure(
        bad(core::ErrorCode::InvalidArgument,std::move(reason)));
}
[[nodiscard]] bool valid_domain(audio::FrameDomainId id) noexcept {
    return id==audio::FrameDomainId::SOURCE_PROCESSING_RATE ||
           id==audio::FrameDomainId::OUTPUT_RATE;
}
[[nodiscard]] core::Status validate(const DspProcessSpec& spec,
                                    const DynamicEqParameters& parameters)
{
    const auto fs=spec.audio_format.sample_rate().value();
    const auto channel=spec.audio_format.channel_layout();
    if(!valid_domain(spec.frame_domain_id)||
       spec.maximum_block_frames.value()<=0||
       fs<=0||fs>std::numeric_limits<std::uint32_t>::max()||
       (channel!=audio::ChannelLayout::MONO_C &&
        channel!=audio::ChannelLayout::STEREO_LR))
        return invalid("INVALID_PROCESS_SPEC");
    return parameters.validate_for_audio(
        static_cast<std::uint32_t>(fs),
        static_cast<std::uint32_t>(spec.audio_format.channel_count()));
}
[[nodiscard]] std::string hex_double(double v) {
    // Preserve exact binary64 parameter identity. UUID-only changes and
    // disabled bands have no effect on sonic execution fingerprint.
    const auto bits=std::bit_cast<std::uint64_t>(v);
    constexpr char digits[]="0123456789abcdef";
    std::string out(16,'0');
    for(int i=0;i<16;++i)
        out[static_cast<std::size_t>(i)]=
            digits[(bits>>static_cast<unsigned>((15-i)*4))&15U];
    return out;
}
[[nodiscard]] std::string sonic_fingerprint(const DynamicEqParameters& p) {
    std::string fp="rgsml.dsp.dynamic-eq.sonic/C01;active";
    for(const auto& band:p.bands()) {
        const auto& s=band.spec();
        if(!s.enabled)continue;
        fp+="|type="+std::to_string(static_cast<unsigned>(s.filter_type));
        fp+=";route="+std::to_string(static_cast<unsigned>(s.routing));
        fp+=";f="+hex_double(std::visit([](const auto& sh) {
            return sh.frequency_hz;
        },s.shape));
        fp+=";shape="+hex_double(s.filter_type==DynamicEqFilterType::BELL
            ?std::get<DynamicEqBellShape>(s.shape).q
            :std::get<DynamicEqShelfShape>(s.shape).shelf_slope);
        const auto& c=s.control;
        for(const double d:{c.base_gain_db,c.range_db,c.threshold_dbfs,c.ratio,
                            c.rms_time_constant_ms,c.attack_ms,c.release_ms})
            fp+=";"+hex_double(d);
    }
    return fp;
}
void write_u64(std::vector<std::uint8_t>& out,std::uint64_t v) {
    for(unsigned i=0;i<8;++i)
        out.push_back(static_cast<std::uint8_t>((v>>(8*i))&255U));
}
[[nodiscard]] std::optional<std::uint64_t> read_u64(
    const std::vector<std::uint8_t>& v,std::size_t& pos) {
    if(pos>v.size()||v.size()-pos<8U)return std::nullopt;
    std::uint64_t n=0;
    for(unsigned i=0;i<8;++i)
        n|=static_cast<std::uint64_t>(v[pos++])<<(8*i);
    return n;
}
void write_double(std::vector<std::uint8_t>& v,double d) {
    write_u64(v,std::bit_cast<std::uint64_t>(d));
}
[[nodiscard]] std::optional<double> read_double(
    const std::vector<std::uint8_t>& v,std::size_t& pos) {
    const auto u=read_u64(v,pos);
    if(!u)return std::nullopt;
    return std::bit_cast<double>(*u);
}
[[nodiscard]] std::array<double,11> fields(
    const internal::C01BandState& b) noexcept {
    const auto& a=b.lanes[0];const auto& c=b.lanes[1];
    return {a.detector.s1,a.detector.s2,a.audio.s1,a.audio.s2,a.power,
            c.detector.s1,c.detector.s2,c.audio.s1,c.audio.s2,c.power,
            b.unsigned_magnitude_db};
}
void assign(internal::C01BandState& b,const std::array<double,11>& f) {
    auto& a=b.lanes[0];auto& c=b.lanes[1];
    a.detector.s1=f[0];a.detector.s2=f[1];
    a.audio.s1=f[2];a.audio.s2=f[3];a.power=f[4];
    c.detector.s1=f[5];c.detector.s2=f[6];
    c.audio.s1=f[7];c.audio.s2=f[8];c.power=f[9];
    b.unsigned_magnitude_db=f[10];
}
} // namespace

struct DynamicEqModule::Impl final {
    const ModuleDescriptor* descriptor;
    DynamicEqParameters parameters;
    std::optional<DspProcessSpec> spec;
    std::unique_ptr<internal::C01FrameEngine> engine;
    std::optional<core::FrameIndex> next;
    bool ended{false};
};
DynamicEqModule::DynamicEqModule(std::unique_ptr<Impl> p) noexcept
    :impl_(std::move(p)) {}
DynamicEqModule::~DynamicEqModule()=default;

core::Result<std::unique_ptr<DynamicEqModule>> DynamicEqModule::create(
    const ModuleDescriptor& descriptor,DynamicEqParameters parameters)
{
    using R=core::Result<std::unique_ptr<DynamicEqModule>>;
    if(descriptor.type_id()!=kType ||
       descriptor.algorithm_version()!=std::optional<std::string_view>{kAlgorithm} ||
       descriptor.parameter_schema_id()!=std::optional<std::string_view>{kParameters})
        return R::failure(bad(core::ErrorCode::InvalidArgument,
                              "INCOMPATIBLE_MODULE_DESCRIPTOR"));
    try {
        auto impl=std::make_unique<Impl>(Impl{&descriptor,std::move(parameters),
                                              std::nullopt,nullptr,std::nullopt,false});
        return R::success(std::unique_ptr<DynamicEqModule>(
            new DynamicEqModule(std::move(impl))));
    }catch(...) {
        return R::failure(bad(core::ErrorCode::InvalidState,
                              "MODULE_CREATION_FAILURE"));
    }
}
const DynamicEqParameters& DynamicEqModule::parameters() const noexcept {
    return impl_->parameters;
}
const ModuleDescriptor& DynamicEqModule::descriptor() const noexcept {
    return *impl_->descriptor;
}
core::Result<DspRuntimeRequirements>
DynamicEqModule::runtime_requirements(const DspProcessSpec& spec) const
{
    const auto checked=validate(spec,impl_->parameters);
    if(!checked)
        return core::Result<DspRuntimeRequirements>::failure(*checked.error());
    // Frozen DYN_EQ_EFFECTIVE_MEMORY_V1 requires a real, input/state-bound
    // certificate. An unconditional 0 pre-context/tail is NOT a valid
    // declaration. Direct full-prefix streaming can still be exercised via
    // prepare/process; Product preview/random access remains fail-closed.
    return core::Result<DspRuntimeRequirements>::failure(
        bad(core::ErrorCode::UnsupportedOperation,
            "DYN_EQ_EFFECTIVE_MEMORY_CERTIFICATE_REQUIRED"));
}
core::Status DynamicEqModule::prepare(const DspProcessSpec& spec)
{
    const auto valid=validate(spec,impl_->parameters);
    if(!valid)return valid;
    try {
        auto engine=internal::C01FrameEngine::create(
            impl_->parameters,
            static_cast<std::uint32_t>(spec.audio_format.sample_rate().value()),
            static_cast<std::uint32_t>(spec.audio_format.channel_count()));
        if(!engine)return core::Status::failure(*engine.error());
        auto staged=std::make_unique<internal::C01FrameEngine>(
            std::move(*engine.value()));
        impl_->engine=std::move(staged);
        impl_->spec=spec;
        impl_->next.reset();
        impl_->ended=false;
        return core::Status::success();
    }catch(...) {
        return core::Status::failure(
            bad(core::ErrorCode::InvalidState,"PREPARE_FAILURE"));
    }
}
void DynamicEqModule::reset() noexcept {
    if(impl_->engine)impl_->engine->reset();
    impl_->next.reset();
    impl_->ended=false;
}
core::Status DynamicEqModule::process(
    audio::AudioBufferView input,audio::MutableAudioBufferView output,
    const DspProcessContext& context)
{
    try {
        if(!impl_->spec || !impl_->engine)
            return core::Status::failure(
                bad(core::ErrorCode::InvalidState,"MODULE_NOT_PREPARED"));
        if(impl_->ended)return invalid("PROCESS_AFTER_EOS");
        const auto& spec=*impl_->spec;
        if(input.format()!=spec.audio_format||
           output.format()!=spec.audio_format||
           input.timebase().frame_domain_id()!=spec.frame_domain_id||
           output.timebase().frame_domain_id()!=spec.frame_domain_id||
           input.timebase().sample_rate()!=spec.audio_format.sample_rate()||
           output.timebase().sample_rate()!=spec.audio_format.sample_rate()||
           input.absolute_range()!=context.output_frame_range||
           output.absolute_range()!=context.output_frame_range||
           input.frame_count()!=output.frame_count()||
           input.frame_count().value()<=0||
           input.frame_count().value()>spec.maximum_block_frames.value()||
           context.output_frame_range.begin().value()<0||
           context.output_frame_range.end().value()<0)
            return invalid("INVALID_PROCESS_CONTEXT");
        if(!impl_->next.has_value()) {
            if(!context.begins_stream)return invalid("MISSING_BEGIN_STREAM");
        }else if(context.begins_stream||
                 context.output_frame_range.begin()!=*impl_->next)
            return invalid("NONCONTIGUOUS_OR_REPEATED_BEGIN");

        const auto in0=input.channel(0);
        const auto out0=output.channel(0);
        if(!in0||!out0)return invalid("INVALID_CHANNEL_ZERO");
        std::span<const double> in1;
        std::span<double> out1;
        if(spec.audio_format.channel_count()==2) {
            const auto a=input.channel(1);
            const auto b=output.channel(1);
            if(!a||!b)return invalid("INVALID_CHANNEL_ONE");
            in1=*a.value();out1=*b.value();
        }
        const auto result=impl_->engine->process_block(
            *in0.value(),in1,*out0.value(),out1);
        if(!result)return result;
        impl_->next=context.output_frame_range.end();
        impl_->ended=context.ends_stream;
        return core::Status::success();
    }catch(...) {
        return core::Status::failure(
            bad(core::ErrorCode::InvalidState,"PROCESS_EXCEPTION"));
    }
}
core::Result<DspRuntimeCheckpoint>
DynamicEqModule::runtime_checkpoint() const
{
    using R=core::Result<DspRuntimeCheckpoint>;
    if(!impl_->spec || !impl_->engine)
        return R::failure(bad(core::ErrorCode::InvalidState,"MODULE_NOT_PREPARED"));
    if(impl_->ended)
        return R::failure(bad(core::ErrorCode::UnsupportedOperation,
                              "CHECKPOINT_AFTER_EOS"));
    try {
        DspRuntimeCheckpoint cp;
        cp.module_type_id=kType;
        cp.algorithm_version=kAlgorithm;
        cp.parameter_schema_id=kParameters;
        cp.sonic_fingerprint=sonic_fingerprint(impl_->parameters);
        cp.audio_format=impl_->spec->audio_format;
        cp.frame_domain_id=impl_->spec->frame_domain_id;
        cp.checkpoint_schema_version=kCheckpoint;
        cp.backend_identity=kBackend;
        cp.next_input_frame=impl_->next;
        const auto& states=impl_->engine->states();
        cp.payload.reserve(16+88*states.size());
        write_u64(cp.payload,static_cast<std::uint64_t>(states.size()));
        // Mirror the authoritative next-frame ownership inside the payload.
        // Mismatched carrier/payload epoch is rejected before state import.
        write_u64(cp.payload,impl_->next
            ?static_cast<std::uint64_t>(impl_->next->value())
            :std::numeric_limits<std::uint64_t>::max());
        for(const auto& b:states)
            for(const double d:fields(b))
                write_double(cp.payload,d);
        return R::success(std::move(cp));
    }catch(...) {
        return R::failure(bad(core::ErrorCode::InvalidState,
                              "CHECKPOINT_SERIALIZE_FAILURE"));
    }
}
core::Status DynamicEqModule::restore_runtime_checkpoint(
    const DspRuntimeCheckpoint& cp)
{
    if(!impl_->spec || !impl_->engine)
        return core::Status::failure(
            bad(core::ErrorCode::InvalidState,"MODULE_NOT_PREPARED"));
    if(impl_->ended)return invalid("RESTORE_AFTER_EOS");
    try {
        if(cp.module_type_id!=kType||cp.algorithm_version!=kAlgorithm||
           cp.parameter_schema_id!=kParameters||
           cp.sonic_fingerprint!=sonic_fingerprint(impl_->parameters)||
           cp.audio_format!=impl_->spec->audio_format||
           cp.frame_domain_id!=impl_->spec->frame_domain_id||
           cp.checkpoint_schema_version!=kCheckpoint||
           cp.backend_identity!=kBackend||
           (cp.next_input_frame.has_value()&&
            cp.next_input_frame->value()<0)||
           (impl_->next.has_value()&&impl_->next!=cp.next_input_frame))
            return invalid("INCOMPATIBLE_CHECKPOINT_METADATA");
        const auto count=impl_->engine->states().size();
        if(cp.payload.size()!=16U+count*88U)
            return invalid("CHECKPOINT_LENGTH");
        std::size_t pos=0;
        const auto n=read_u64(cp.payload,pos);
        if(!n||*n!=count)return invalid("CHECKPOINT_BAND_COUNT");
        const auto epoch=read_u64(cp.payload,pos);
        const auto expected_epoch=cp.next_input_frame
            ?static_cast<std::uint64_t>(cp.next_input_frame->value())
            :std::numeric_limits<std::uint64_t>::max();
        if(!epoch||*epoch!=expected_epoch)
            return invalid("CHECKPOINT_EPOCH_MISMATCH");
        std::vector<internal::C01BandState> candidate(count);
        for(auto& b:candidate) {
            std::array<double,11> f{};
            for(auto& x:f) {
                auto v=read_double(cp.payload,pos);
                if(!v)return invalid("CHECKPOINT_TRUNCATED");
                x=*v;
                if(!cp.next_input_frame &&
                   std::bit_cast<std::uint64_t>(x)!=0U)
                    return invalid("UNBOUND_STATE_NOT_ZERO");
            }
            assign(b,f);
        }
        if(pos!=cp.payload.size())return invalid("CHECKPOINT_TRAILING");
        const auto checked=impl_->engine->restore_states(candidate);
        if(!checked)return checked;
        impl_->next=cp.next_input_frame;
        impl_->ended=false;
        return core::Status::success();
    }catch(...) {
        return core::Status::failure(
            bad(core::ErrorCode::InvalidArgument,"CHECKPOINT_PARSE_FAILURE"));
    }
}
} // namespace rgsml::dsp
