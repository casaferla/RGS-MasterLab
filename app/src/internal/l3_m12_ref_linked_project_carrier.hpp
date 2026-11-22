#pragma once
// L3-M12 exact ProjectDocument ordinary Module carrier adapter.
// Converts a validated REF_LINKED Product DSP STAGE into the existing
// native project Chain/Module structures using ONE canonical DSP
// parameter codec. Does not attach to ProjectSnapshot or publish.
// Atomic M12 write/receipt and M13 verifier remain separate.
#include "l3_m12_ref_linked_native_stage.hpp"
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/project/project_document.hpp>
#include <rgsml/project/opaque_json_value.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m12_frozen {
struct FrozenProjectLinkedCarrier final {
    project::Chain native_project_chain;
    core::Uuid exact_owner_controller;
    core::Uuid exact_accepted_plan_id;
    std::string exact_accepted_plan_checksum;
    [[nodiscard]] constexpr bool authorizes_project_commit()
        const noexcept{return false;}
};
[[nodiscard]] inline core::Result<FrozenProjectLinkedCarrier>
encode_exact_ref_linked_project_carrier(
    const StagedRefLinkedSegment& staged,
    core::Uuid fresh_ref_linked_chain_id)
{
    using R=core::Result<FrozenProjectLinkedCarrier>;
    const auto fail=[](const char* why) {
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    if(fresh_ref_linked_chain_id.is_nil()||
       staged.accepted_identity.plan_id.is_nil()||
       staged.accepted_identity.reference_match_controller_id.is_nil()||
       staged.accepted_identity.exact_plan_content_checksum.empty()||
       staged.chain.context()!=dsp::ProcessingChainContext{
           dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED}||
       staged.chain.instances().size()!=staged.exact_bindings.size()||
       !staged.registry_owner)
        return fail("L3_M12_PROJECT_CARRIER_NOT_CURRENT");
    project::Chain chain;
    chain.chainId=fresh_ref_linked_chain_id;
    chain.stage="MASTER";
    chain.segment="REF_LINKED";
    chain.revision=staged.chain.revision();
    for(std::size_t i=0;i<staged.exact_bindings.size();++i) {
        const auto& instance=staged.chain.instances()[i];
        const auto& binding=staged.exact_bindings[i];
        if(!binding_matches_instance(instance,binding)||
           instance.provenance()!=dsp::ModuleProvenance::REFERENCE_MATCH||
           instance.owner()!=dsp::ModuleOwner::REFERENCE_MATCH||
           instance.link_state()!=dsp::ModuleLinkState::REF_LINKED||
           !instance.semantic_node_id()||
           instance.semantic_node_id()->is_nil())
            return fail("L3_M12_PROJECT_LINKED_OWNER_MISMATCH");
        const auto registered=staged.registry_owner->find_descriptor(
            instance.module_type_id());
        if(!registered)return R::failure(*registered.error());
        const auto& descriptor=registered.value()->get();
        if(!descriptor.algorithm_version()||
           !descriptor.parameter_schema_id())
            return fail("L3_M12_PROJECT_MISSING_REGISTERED_SCHEMA");
        const auto serialized=dsp::encode_module_parameters_json(
            binding.parameters);
        if(!serialized)return R::failure(*serialized.error());
        const auto canonical=project::OpaqueJsonValue::parse(
            *serialized.value());
        if(!canonical)return R::failure(*canonical.error());
        const auto decoded=dsp::decode_module_parameters_json(
            *descriptor.parameter_schema_id(),canonical.value()->canonical_utf8());
        if(!decoded)return R::failure(*decoded.error());
        if(*decoded.value()!=binding.parameters)
            return fail("L3_M12_PROJECT_PARAMETER_CODEC_NOT_EXACT");
        project::Module module;
        module.instanceId=instance.instance_id().uuid();
        module.typeId=std::string{instance.module_type_id()};
        module.enabled=instance.enabled();
        module.userBypass=instance.user_bypass();
        module.controllerSuspended=instance.controller_suspended();
        module.domainSuspended=instance.domain_suspended();
        module.provenance="REFERENCE_MATCH";
        module.owner="REFERENCE_MATCH";
        module.linkState="REF_LINKED";
        module.semanticNodeId=instance.semantic_node_id();
        module.algorithmVersion=std::string{*descriptor.algorithm_version()};
        module.parameterSchemaId=std::string{*descriptor.parameter_schema_id()};
        module.parameters=*canonical.value();
        chain.modules.push_back(std::move(module));
    }
    return R::success(FrozenProjectLinkedCarrier{
        std::move(chain),staged.accepted_identity.reference_match_controller_id,
        staged.accepted_identity.plan_id,
        staged.accepted_identity.exact_plan_content_checksum});
}
} // namespace rgsml::app::l3_automatch::m12_frozen
