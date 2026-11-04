// L1-M22 PRIVATE typed read-only bridge to the EXISTING actual DSP domain types.
// No canonical parameter serializer, module factory activation or runtime execution.
// Supplied fingerprints/latencies remain EXTERNAL; fail closed until authoritative.
#pragma once
#include "m22_request_plan_candidate.hpp"

#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rgsml::m22_private {
enum class TypedPortError : std::uint8_t {
    NONE, WRONG_STAGE, WRONG_SEGMENT, WRONG_PROVENANCE, MISSING_FACTORY,
    MISSING_DESCRIPTOR_IDENTITY, WRONG_PLACEMENT, UNKNOWN_MODULE,
    MISSING_BINDING, DUPLICATE_BINDING, MISSING_PROOF, DUPLICATE_PROOF,
    WRONG_TYPED_PARAMETERS, EXTRA_BINDING_OR_PROOF
};
struct ExternalFrozenExecutionEvidence final {
    std::string instance_uuid;
    std::string exact_parameter_fingerprint;
    std::uint64_t externally_resolved_active_latency_frames{0};
};
// These identifiers come from the existing registry; never guessed per position.
[[nodiscard]] inline std::optional<InlineKind> inline_kind(
    std::string_view type) noexcept {
    if (type == "rgsml.dsp.gain") return InlineKind::GAIN;
    if (type == "rgsml.dsp.parametric-eq") return InlineKind::PARAMETRIC_EQ;
    if (type == "rgsml.dsp.compressor") return InlineKind::COMPRESSOR;
    if (type == "rgsml.dsp.stereo-ms") return InlineKind::STEREO_MS;
    return std::nullopt; // Limiter/Dither cannot be made arbitrary inline nodes.
}
[[nodiscard]] inline bool typed_payload_matches(
    InlineKind kind,const rgsml::dsp::ModuleParameterPayload& payload) noexcept {
    switch(kind) {
        case InlineKind::GAIN:
            return std::holds_alternative<rgsml::dsp::GainParameters>(payload);
        case InlineKind::PARAMETRIC_EQ:
            return std::holds_alternative<rgsml::dsp::ParametricEqParameters>(payload);
        case InlineKind::COMPRESSOR:
            return std::holds_alternative<rgsml::dsp::CompressorParameters>(payload);
        case InlineKind::STEREO_MS:
            return std::holds_alternative<rgsml::dsp::StereoMsParameters>(payload);
    }
    return false;
}
[[nodiscard]] inline std::optional<std::vector<InlineSnapshot>>
port_stored_manual_chain_candidate(
    const rgsml::dsp::ProcessingChain& chain,
    const rgsml::dsp::ModuleRegistry& registry,
    std::span<const rgsml::dsp::ModuleExecutionBinding> actual_bindings,
    std::span<const ExternalFrozenExecutionEvidence> external_proofs,
    TypedPortError& error)
{
    using namespace rgsml::dsp;
    error=TypedPortError::NONE;
    const auto ctx=chain.context();
    if(ctx.stage!=ProcessingStage::MASTER) {
        error=TypedPortError::WRONG_STAGE;return std::nullopt;
    }
    if(ctx.segment!=ChainSegment::MANUAL) {
        error=TypedPortError::WRONG_SEGMENT;return std::nullopt;
    }
    const auto instances=chain.instances();
    if(instances.size()!=actual_bindings.size() || instances.size()!=external_proofs.size()) {
        error=TypedPortError::EXTRA_BINDING_OR_PROOF;return std::nullopt;
    }
    std::vector<InlineSnapshot> out;
    out.reserve(instances.size());
    for(const auto& instance:instances) {
        if(instance.provenance()!=ModuleProvenance::MANUAL
            || instance.owner()!=ModuleOwner::USER
            || instance.link_state()!=ModuleLinkState::UNLINKED) {
            error=TypedPortError::WRONG_PROVENANCE;return std::nullopt;
        }
        const auto type=instance.module_type_id();
        const auto kind=inline_kind(type);
        if(!kind) {
            error=TypedPortError::UNKNOWN_MODULE;return std::nullopt;
        }
        const auto found=registry.find_descriptor(type);
        if(!found) {
            error=TypedPortError::MISSING_DESCRIPTOR_IDENTITY;return std::nullopt;
        }
        const auto& descriptor=found.value()->get();
        const auto algo=descriptor.algorithm_version();
        const auto schema=descriptor.parameter_schema_id();
        if(!algo || !schema || algo->empty() || schema->empty()) {
            error=TypedPortError::MISSING_DESCRIPTOR_IDENTITY;return std::nullopt;
        }
        if(descriptor.placement_class()!=PlacementClass::INLINE_CHAIN
            || descriptor.terminal_slot().has_value()
            || std::find(descriptor.allowed_stages().begin(),
                         descriptor.allowed_stages().end(),ProcessingStage::MASTER)
                  ==descriptor.allowed_stages().end()
            || std::find(descriptor.allowed_segments().begin(),
                         descriptor.allowed_segments().end(),ChainSegment::MANUAL)
                  ==descriptor.allowed_segments().end()) {
            error=TypedPortError::WRONG_PLACEMENT;return std::nullopt;
        }
        if(!registry.has_factory(type)) {
            error=TypedPortError::MISSING_FACTORY;return std::nullopt;
        }
        const auto binding_count=std::count_if(actual_bindings.begin(),actual_bindings.end(),
            [&instance](const ModuleExecutionBinding& b){
                return b.instance_id==instance.instance_id();
            });
        if(binding_count!=1) {
            error=binding_count==0?TypedPortError::MISSING_BINDING:
                                   TypedPortError::DUPLICATE_BINDING;
            return std::nullopt;
        }
        const auto bound=std::find_if(actual_bindings.begin(),actual_bindings.end(),
            [&instance](const ModuleExecutionBinding& b){
                return b.instance_id==instance.instance_id();
            });
        if(!typed_payload_matches(*kind,bound->parameters)) {
            error=TypedPortError::WRONG_TYPED_PARAMETERS;return std::nullopt;
        }
        const auto instance_id=instance.instance_id().to_string();
        const auto proof_count=std::count_if(external_proofs.begin(),external_proofs.end(),
            [&instance_id](const ExternalFrozenExecutionEvidence& p) {
                return p.instance_uuid==instance_id;
            });
        if(proof_count!=1) {
            error=proof_count==0?TypedPortError::MISSING_PROOF:
                                 TypedPortError::DUPLICATE_PROOF;
            return std::nullopt;
        }
        const auto proof=std::find_if(external_proofs.begin(),external_proofs.end(),
            [&instance_id](const ExternalFrozenExecutionEvidence& p) {
                return p.instance_uuid==instance_id;
            });
        if(proof->exact_parameter_fingerprint.empty()) {
            error=TypedPortError::MISSING_PROOF;return std::nullopt;
        }
        out.push_back(InlineSnapshot{
            instance_id,*kind,
            std::string(type)+":"+std::string(*algo)+":"+std::string(*schema),
            proof->exact_parameter_fingerprint,
            instance.user_bypass(),
            proof->externally_resolved_active_latency_frames,
            instance.enabled(),
            instance.controller_suspended(),
            instance.domain_suspended()
        });
    }
    return out;
}
} // namespace rgsml::m22_private
