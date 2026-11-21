#pragma once
// L3-M07 P01/PO ACCEPTED_FROZEN: STATIC artistic/intent projection.
// 27 ARTISTIC slots are exact M05/M06 ownership and rational masses;
// T1's exclusive 1/3 fallback remains ONE slot. Safety/evidence carry
// ZERO objective mass. This is NOT a DSP ModuleRegistry, feature
// analyzer, runtime AvailabilityAssessment, project serializer or
// M08 target/solver registry.
//
// The frozen M07 JSON/checksum/Phase-9 adapter has NOT been implemented
// here; never claim registry content checksum or VALID/CURRENT from the
// compile-time policy projection. Missing exact M07 authority/dependency
// identities leave runtime feature consumption unresolved.
#include <rgsml/dsp/module_registry.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string_view>

namespace rgsml::app::l3_automatch::m07_frozen {
inline constexpr std::string_view kRegistryId=
    "rgsml.reference-match.domain-registry";
inline constexpr std::string_view kSchemaId=
    "rgsml.reference-match.domain-registry.schema/1.0.0";
inline constexpr std::string_view kPolicyVersion=
    "rgsml.reference-match.binding-policy/1.0.0";
inline constexpr std::string_view kCanonicalSerializationId=
    "rgsml.reference-match.registry.canonical-json/1.0.0";
inline constexpr std::uint32_t kInitialRegistryRevision=1;

enum class Domain:std::uint8_t {tonal,dynamics,stereo,loudness};
enum class Transform:std::uint8_t {
    identity,power_fraction_db,log2_positive,fisher_z,
    mono_endpoint_log2
};
enum class ComponentShare:std::uint8_t {one,thirds,configured_n};
struct Rational final {
    std::int32_t numerator{},denominator{};
    friend bool operator==(const Rational&,const Rational&)=default;
    [[nodiscard]] constexpr bool reduced_positive() const noexcept {
        return numerator>0&&denominator>0
            &&std::gcd(numerator,denominator)==1;
    }
    [[nodiscard]] constexpr int in_units_8000() const noexcept {
        return reduced_positive() &&8000%denominator==0
            ?numerator*(8000/denominator):-1;
    }
};
struct Binding final {
    std::string_view binding_id; // deterministic, canonical M07 key
    std::string_view source_feature_id;
    std::string_view exclusive_fallback_feature_id;
    std::string_view group_id;
    std::string_view support_policy_key;
    Domain domain;
    Transform transform;
    Rational domain_mass;
    ComponentShare component_share;
    double scale;
    double deadband;
};
inline constexpr std::array<std::string_view,4> kDomainIds{{
    "rgsml.reference-match.domain.tonal",
    "rgsml.reference-match.domain.dynamics",
    "rgsml.reference-match.domain.stereo",
    "rgsml.reference-match.domain.loudness"
}};
inline constexpr std::array<Binding,27> kBindings{{
    {"rgsml.reference-match.binding.tonal.t01",
     "rgsml.tonal.profile.normalized.fractional_octave_1_12", "rgsml.tonal.profile.normalized.fractional_octave_1_3", "",
     "SUP.TPROFILE", Domain::tonal, Transform::power_fraction_db,
     {4,5}, ComponentShare::configured_n,1,0.5},
    {"rgsml.reference-match.binding.tonal.t02",
     "rgsml.tonal.spectral_tilt.100hz_10khz", "", "",
     "SUP.SCALAR", Domain::tonal, Transform::identity,
     {3,50}, ComponentShare::one,0.5,0.25},
    {"rgsml.reference-match.binding.tonal.t03",
     "rgsml.tonal.spectral_centroid.frequency_hz", "", "",
     "SUP.SCALAR", Domain::tonal, Transform::log2_positive,
     {1,25}, ComponentShare::one,0.25,0.1},
    {"rgsml.reference-match.binding.tonal.t04",
     "rgsml.tonal.rolloff.p85.frequency_hz", "", "",
     "SUP.SCALAR", Domain::tonal, Transform::log2_positive,
     {3,100}, ComponentShare::one,0.25,0.1},
    {"rgsml.reference-match.binding.tonal.t05",
     "rgsml.tonal.rolloff.p95.frequency_hz", "", "",
     "SUP.SCALAR", Domain::tonal, Transform::log2_positive,
     {1,50}, ComponentShare::one,0.25,0.1},
    {"rgsml.reference-match.binding.tonal.t06",
     "rgsml.tonal.hf_slope.8khz_16khz", "", "",
     "SUP.SCALAR", Domain::tonal, Transform::identity,
     {1,20}, ComponentShare::one,0.5,0.25},
    {"rgsml.reference-match.binding.dynamics.d01",
     "rgsml.loudness.short_term.series", "", "rgsml.reference-match.domain.dynamics.macro",
     "SUP.NATIVE.ST", Domain::dynamics, Transform::identity,
     {3,20}, ComponentShare::thirds,1,0.5},
    {"rgsml.reference-match.binding.dynamics.d02",
     "rgsml.loudness.lra", "", "rgsml.reference-match.domain.dynamics.macro",
     "SUP.SCALAR.LRA", Domain::dynamics, Transform::identity,
     {1,10}, ComponentShare::one,1,1},
    {"rgsml.reference-match.binding.dynamics.d03",
     "rgsml.level.rms.windowed.series", "", "rgsml.reference-match.domain.dynamics.macro",
     "SUP.NATIVE.RMS", Domain::dynamics, Transform::identity,
     {3,40}, ComponentShare::one,1,0.5},
    {"rgsml.reference-match.binding.dynamics.d04",
     "rgsml.dynamics.crest.local.series", "", "rgsml.reference-match.domain.dynamics.macro",
     "SUP.NATIVE.CREST", Domain::dynamics, Transform::identity,
     {3,40}, ComponentShare::thirds,1,0.5},
    {"rgsml.reference-match.binding.dynamics.d05",
     "rgsml.dynamics.plr", "", "rgsml.reference-match.domain.dynamics.macro",
     "SUP.SCALAR.OPERANDS", Domain::dynamics, Transform::identity,
     {1,20}, ComponentShare::one,1,0.5},
    {"rgsml.reference-match.binding.dynamics.d06",
     "rgsml.dynamics.crest.global", "", "rgsml.reference-match.domain.dynamics.macro",
     "SUP.SCALAR.OPERANDS", Domain::dynamics, Transform::identity,
     {1,20}, ComponentShare::one,1,0.5},
    {"rgsml.reference-match.binding.dynamics.d07",
     "rgsml.dynamics.transient.contrast.distribution", "", "rgsml.reference-match.domain.dynamics.microdynamics",
     "SUP.EVENT.CONTRAST", Domain::dynamics, Transform::identity,
     {9,80}, ComponentShare::thirds,1,0.75},
    {"rgsml.reference-match.binding.dynamics.d08",
     "rgsml.dynamics.transient.local_crest.distribution", "", "rgsml.reference-match.domain.dynamics.microdynamics",
     "SUP.EVENT.CREST", Domain::dynamics, Transform::identity,
     {3,40}, ComponentShare::thirds,1,0.75},
    {"rgsml.reference-match.binding.dynamics.d09",
     "rgsml.dynamics.transient.attack_rise_time.distribution", "", "rgsml.reference-match.domain.dynamics.microdynamics",
     "SUP.EVENT.RISE", Domain::dynamics, Transform::log2_positive,
     {1,16}, ComponentShare::thirds,0.25,0.125},
    {"rgsml.reference-match.binding.dynamics.d10",
     "rgsml.dynamics.spectral.relative_spread", "", "rgsml.reference-match.domain.dynamics.spectral",
     "SUP.SPECTRAL.RELATIVE", Domain::dynamics, Transform::identity,
     {1,8}, ComponentShare::configured_n,1,0.5},
    {"rgsml.reference-match.binding.dynamics.d11",
     "rgsml.dynamics.spectral.absolute_spread", "", "rgsml.reference-match.domain.dynamics.spectral",
     "SUP.SPECTRAL.ABSOLUTE", Domain::dynamics, Transform::identity,
     {1,16}, ComponentShare::configured_n,1,0.5},
    {"rgsml.reference-match.binding.dynamics.d12",
     "rgsml.dynamics.spectral.broadband_correlation", "", "rgsml.reference-match.domain.dynamics.spectral",
     "SUP.SPECTRAL.RHO", Domain::dynamics, Transform::fisher_z,
     {1,16}, ComponentShare::configured_n,0.25,0.05},
    {"rgsml.reference-match.binding.stereo.s01",
     "rgsml.stereo.width_ratio", "", "rgsml.reference-match.domain.stereo.broadband-width",
     "SUP.BROADBAND.WIDTH", Domain::stereo, Transform::mono_endpoint_log2,
     {1,4}, ComponentShare::one,0.25,0.1},
    {"rgsml.reference-match.binding.stereo.s02",
     "rgsml.stereo.correlation.series", "", "rgsml.reference-match.domain.stereo.correlation",
     "SUP.NATIVE.RHO", Domain::stereo, Transform::fisher_z,
     {1,5}, ComponentShare::thirds,0.25,0.05},
    {"rgsml.reference-match.binding.stereo.s03",
     "rgsml.stereo.multiband.width_ratio", "", "rgsml.reference-match.domain.stereo.multiband",
     "SUP.STEREO.BAND_WIDTH", Domain::stereo, Transform::mono_endpoint_log2,
     {3,20}, ComponentShare::configured_n,0.25,0.1},
    {"rgsml.reference-match.binding.stereo.s04",
     "rgsml.stereo.multiband.correlation", "", "rgsml.reference-match.domain.stereo.multiband",
     "SUP.STEREO.BAND_RHO", Domain::stereo, Transform::fisher_z,
     {3,20}, ComponentShare::configured_n,0.25,0.05},
    {"rgsml.reference-match.binding.stereo.s05",
     "rgsml.stereo.low_end.width_ratio", "", "rgsml.reference-match.domain.stereo.low-end",
     "SUP.LOW_END.WIDTH", Domain::stereo, Transform::mono_endpoint_log2,
     {1,8}, ComponentShare::thirds,0.25,0.1},
    {"rgsml.reference-match.binding.stereo.s06",
     "rgsml.stereo.low_end.correlation", "", "rgsml.reference-match.domain.stereo.low-end",
     "SUP.LOW_END.RHO", Domain::stereo, Transform::fisher_z,
     {1,8}, ComponentShare::thirds,0.25,0.05},
    {"rgsml.reference-match.binding.loudness.l01",
     "rgsml.loudness.integrated", "", "",
     "SUP.SCALAR.INTEGRATED", Domain::loudness, Transform::identity,
     {7,10}, ComponentShare::one,1,0.5},
    {"rgsml.reference-match.binding.loudness.l02",
     "rgsml.loudness.short_term.series", "", "",
     "SUP.NATIVE.ST", Domain::loudness, Transform::identity,
     {3,20}, ComponentShare::one,1,0.5},
    {"rgsml.reference-match.binding.loudness.l03",
     "rgsml.level.true_peak", "", "",
     "SUP.SCALAR.SCOPED_TP", Domain::loudness, Transform::identity,
     {3,20}, ComponentShare::one,0.5,0.2}
}};
struct CapabilityIntent final {
    std::string_view intent_id;
    std::string_view dsp_type_id; // FOREIGN KEY to sole ModuleRegistry
    dsp::ProcessingStage required_stage;
    dsp::ChainSegment required_segment;
};
inline constexpr std::array<CapabilityIntent,8> kIntents{{
    {"rgsml.reference-match.capability-intent.tonal-broad-eq",
     "rgsml.dsp.parametric-eq",dsp::ProcessingStage::MASTER,
     dsp::ChainSegment::MANUAL},
    {"rgsml.reference-match.capability-intent.macro-compressor",
     "rgsml.dsp.compressor",dsp::ProcessingStage::MASTER,
     dsp::ChainSegment::MANUAL},
    {"rgsml.reference-match.capability-intent.micro-transient-shaper",
     "rgsml.dsp.transient-shaper",dsp::ProcessingStage::MASTER,
     dsp::ChainSegment::MANUAL},
    {"rgsml.reference-match.capability-intent.spectral-dynamic-eq",
     "rgsml.dsp.dynamic-eq",dsp::ProcessingStage::MASTER,
     dsp::ChainSegment::MANUAL},
    {"rgsml.reference-match.capability-intent.stereo-ms",
     "rgsml.dsp.stereo-ms",dsp::ProcessingStage::MASTER,
     dsp::ChainSegment::MANUAL},
    {"rgsml.reference-match.capability-intent.stereo-broad-ms-eq",
     "rgsml.dsp.parametric-eq",dsp::ProcessingStage::MASTER,
     dsp::ChainSegment::MANUAL},
    {"rgsml.reference-match.capability-intent.loudness-gain",
     "rgsml.dsp.gain",dsp::ProcessingStage::MASTER,
     dsp::ChainSegment::MANUAL},
    {"rgsml.reference-match.capability-intent.loudness-tp-limit",
     "rgsml.dsp.true-peak-limiter",dsp::ProcessingStage::MASTER,
     dsp::ChainSegment::TERMINAL}
}};
// Safety and evidence are separate M07 records. None is a 28th slot.
enum class Role:std::uint8_t{
    safety_constraint,diagnostic_evidence,ui_only,deferred
};
struct NonartisticRecord final {
    std::string_view record_id;
    Role role;
    Rational artistic_mass;
};
inline constexpr std::array<NonartisticRecord,13> kNonartistic{{
    {"rgsml.reference-match.constraint.final-true-peak-ceiling",
      Role::safety_constraint,{0,1}},
    {"rgsml.reference-match.constraint.correlation-p10-no-regression",
      Role::safety_constraint,{0,1}},
    {"rgsml.reference-match.constraint.negative-correlation-occupancy",
      Role::safety_constraint,{0,1}},
    {"rgsml.reference-match.constraint.zero-side-cannot-widen",
      Role::safety_constraint,{0,1}},
    {"rgsml.reference-match.constraint.no-low-end-widening",
      Role::safety_constraint,{0,1}},
    {"rgsml.reference-match.constraint.low-end-correlation-no-regression",
      Role::safety_constraint,{0,1}},
    {"rgsml.reference-match.constraint.no-artificial-decorrelation",
      Role::safety_constraint,{0,1}},
    {"rgsml.reference-match.evidence.mono-phase-evidence",
      Role::diagnostic_evidence,{0,1}},
    {"rgsml.reference-match.evidence.sample-peak-rms-level",
      Role::diagnostic_evidence,{0,1}},
    {"rgsml.reference-match.evidence.transient-detector-support",
      Role::diagnostic_evidence,{0,1}},
    {"rgsml.reference-match.evidence.stereo-side-fraction-lr-balance",
      Role::diagnostic_evidence,{0,1}},
    {"rgsml.reference-match.evidence.visual-history-panels",
      Role::ui_only,{0,1}},
    {"rgsml.reference-match.evidence.deferred-capability-evidence",
      Role::deferred,{0,1}}
}};
[[nodiscard]] constexpr bool static_structure_valid() noexcept {
    if(kBindings.size()!=27||kIntents.size()!=8
       ||kNonartistic.size()!=13)return false;
    std::array<int,4> sums{};
    std::array<int,4> counts{};
    for(std::size_t i=0;i<kBindings.size();++i) {
        const auto& b=kBindings[i];
        const auto d=static_cast<std::size_t>(b.domain);
        if(d>=4||b.binding_id.empty()||b.source_feature_id.empty()
           ||b.support_policy_key.empty()||b.scale<=0
           ||b.deadband<0||b.deadband>b.scale
           ||b.domain_mass.in_units_8000()<0)
            return false;
        sums[d]+=b.domain_mass.in_units_8000();
        ++counts[d];
        for(std::size_t j=0;j<i;++j)
            if(kBindings[j].binding_id==b.binding_id)return false;
    }
    if(sums!=std::array<int,4>{8000,8000,8000,8000}
       ||counts!=std::array<int,4>{6,12,6,3}
       ||kBindings.front().exclusive_fallback_feature_id.empty())
        return false;
    for(std::size_t i=1;i<kBindings.size();++i)
        if(!kBindings[i].exclusive_fallback_feature_id.empty())
            return false;
    for(const auto& entry:kNonartistic)
        if(entry.record_id.empty()||entry.artistic_mass!=Rational{0,1})
            return false;
    for(std::size_t i=0;i<kIntents.size();++i) {
        if(kIntents[i].intent_id.empty()||kIntents[i].dsp_type_id.empty())
            return false;
        for(std::size_t j=0;j<i;++j)
            if(kIntents[i].intent_id==kIntents[j].intent_id)
                return false;
    }
    return true;
}
static_assert(static_structure_valid());
[[nodiscard]] constexpr const Binding* find_binding(
    std::string_view key) noexcept {
    for(const auto& entry:kBindings)
        if(entry.binding_id==key)return &entry;
    return nullptr;
}
// Runtime availability is separate from the static M07 projection.
// Factory existence is NOT equivalent to successful typed binding,
// M09 seed or authoritative M07 feature source readiness.
enum class FactoryEvidence:std::uint8_t{
    unknown_type,descriptor_only,factory_present_unattested
};
struct AssessedIntent final{
    CapabilityIntent intent;
    FactoryEvidence factory_state;
    bool intrinsic_context_matches;
    [[nodiscard]] constexpr bool authorizes_match_planning()
        const noexcept{return false;} // exact deps + M09 are still required
};
[[nodiscard]] inline std::array<AssessedIntent,8>
assess_canonical_dsp_snapshot(const dsp::ModuleRegistry& catalog) {
    std::array<AssessedIntent,8> out{};
    for(std::size_t i=0;i<kIntents.size();++i) {
        const auto& source=kIntents[i];
        AssessedIntent assessed{source,FactoryEvidence::unknown_type,false};
        const auto descriptor=catalog.find_descriptor(source.dsp_type_id);
        if(descriptor) {
            const auto& d=descriptor.value()->get();
            assessed.intrinsic_context_matches=
                std::find(d.allowed_stages().begin(),
                          d.allowed_stages().end(),source.required_stage)
                    !=d.allowed_stages().end()
                &&std::find(d.allowed_segments().begin(),
                             d.allowed_segments().end(),source.required_segment)
                    !=d.allowed_segments().end();
            assessed.factory_state=catalog.has_factory(source.dsp_type_id)
                ?FactoryEvidence::factory_present_unattested
                :FactoryEvidence::descriptor_only;
        }
        out[i]=assessed;
    }
    return out;
}
} // namespace rgsml::app::l3_automatch::m07_frozen
