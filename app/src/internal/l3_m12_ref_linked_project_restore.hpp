#pragma once
// L3 M12 Product Project open READ-ONLY REF_LINKED reconstruction.
// Rebuild the ordinary, typed native DSP nodes and their canonical
// bindings from persisted ProjectDocument. The current legacy
// ProjectSessionViewModel restores MANUAL only: this adapter is a
// necessary typed input for its future atomic REF_LINKED adoption,
// NOT evidence that Product playback/restore is already integrated.
// No secondary engine, no P2 duplication, no M17 export control.
// Even a successful parse does not mint M12 receipt or M13 VERIFIED.
#include "l3_m12_ref_linked_native_stage.hpp"
#include "l3_m12_retained_controller.hpp"
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/project/project_document.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m12_frozen {
struct RestoredM12LinkedProductState final {
    std::shared_ptr<const dsp::ModuleRegistry> registry_owner;
    dsp::ProcessingChain exact_native_chain;
    std::vector<dsp::ModuleExecutionBinding> exact_native_bindings;
    core::Uuid plan_id;
    core::Uuid controller_id;
    std::string exact_plan_checksum;
    // Canonical persisted provenance is a read-only carried identity,
    // never a live Project-owner attestation or M13 verifier receipt.
    core::Uuid persisted_linked_chain_id;
    std::uint64_t exact_plan_revision{};
    std::string exact_m10_winning_checksum;
    std::string exact_m11_protected_fixed_checksum;
    std::string exact_source_reference_scope_revision;
    [[nodiscard]] constexpr bool authorizes_project_adoption()
        const noexcept{return false;}
    [[nodiscard]] constexpr bool is_m12_materialization_receipt()
        const noexcept{return false;}
    [[nodiscard]] constexpr bool authorizes_m13_verification()
        const noexcept{return false;}
};
[[nodiscard]] inline core::Result<RestoredM12LinkedProductState>
decode_exact_m12_ref_linked_product_state(
    const project::ProjectDocument& doc,
    std::shared_ptr<const dsp::ModuleRegistry> registry_owner)
{
    using R=core::Result<RestoredM12LinkedProductState>;
    const auto fail=[](const char* why) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,why});
    };
    if(!registry_owner||doc.projectId.is_nil()||
       !doc.pipeline.masteringChainId||
       !doc.references.activeReferenceId)
        return fail("L3_M12_REOPEN_PROJECT_OR_REGISTRY_MISSING");
    const project::Chain* exact_manual=nullptr;
    const project::Chain* exact_linked=nullptr;
    for(const auto& chain:doc.chains) {
        if(chain.segment=="REF_LINKED"&&chain.stage!="MASTER")
            return fail("L3_M12_REOPEN_FOREIGN_LINKED_SEGMENT");
        if(chain.chainId==*doc.pipeline.masteringChainId) {
            if(exact_manual)return fail("L3_M12_REOPEN_DUPLICATE_MANUAL");
            exact_manual=&chain;
        }
        if(chain.stage=="MASTER"&&chain.segment=="REF_LINKED") {
            if(exact_linked)return fail("L3_M12_REOPEN_AMBIGUOUS_LINKED");
            exact_linked=&chain;
        }
    }
    if(!exact_manual||exact_manual->stage!="MASTER"||
       exact_manual->segment!="MANUAL"||
       !exact_linked||exact_linked->chainId.is_nil()||
       exact_linked->chainId==exact_manual->chainId)
        return fail("L3_M12_REOPEN_MISSING_MANUAL_OR_LINKED");
    for(const auto& module:exact_manual->modules)
        if(module.owner=="REFERENCE_MATCH"||
           module.provenance=="REFERENCE_MATCH"||
           module.linkState=="REF_LINKED")
            return fail("L3_M12_REOPEN_MANUAL_OWNER_CONTAMINATED");
    QJsonParseError parse_error;
    const auto document=QJsonDocument::fromJson(
        QByteArray::fromStdString(doc.referenceMatch.canonical_utf8()),
        &parse_error);
    if(parse_error.error!=QJsonParseError::NoError||
       !document.isObject())
        return fail("L3_M12_REOPEN_REF_METADATA_NOT_CANONICAL");
    const auto value=document.object().value(
        QStringLiteral("materialization"));
    if(!value.isObject())
        return fail("L3_M12_REOPEN_MATERIALIZATION_RECORD_MISSING");
    const auto meta=value.toObject();
    const auto controller=core::Uuid::parse(
        meta.value(QStringLiteral("controllerId")).toString().toStdString());
    const auto plan=core::Uuid::parse(
        meta.value(QStringLiteral("planId")).toString().toStdString());
    const auto checksum=meta.value(
        QStringLiteral("planContentChecksum")).toString().toStdString();
    // A merely named plan/controller is not sufficient: all canonical
    // M10/M11/source-dependency identities must survive Project Open.
    // The unsigned revisions were serialized as exact decimal STRINGS,
    // not JSON doubles (which would lose U64 precision).
    const auto exact_revision=std::to_string(exact_linked->revision);
    const auto winner_checksum=meta.value(
        QStringLiteral("winnerContentChecksum")).toString().toStdString();
    const auto protected_checksum=meta.value(
        QStringLiteral("protectedFixedChecksum")).toString().toStdString();
    const auto dependency=meta.value(
        QStringLiteral("sourceReferenceScopeRevision")).toString().toStdString();
    if(!controller||!plan||controller.value()->is_nil()||
       plan.value()->is_nil()||checksum.empty()||
       meta.value(QStringLiteral("policyId")).toString()!=
           QStringLiteral("rgsml.reference-match.materialization-policy/1.0.0")||
       winner_checksum.empty()||protected_checksum.empty()||
       dependency.empty()||
       meta.value(QStringLiteral("planRevision")).toString().toStdString()!=
           exact_revision||
       meta.value(QStringLiteral("schemaId")).toString()!=
           QStringLiteral("rgsml.reference-match.materialization.schema/1.0.0")||
       meta.value(QStringLiteral("materializationState")).toString()!=
           QStringLiteral("MATERIALIZED")||
       meta.value(QStringLiteral("linkedChainId")).toString().toStdString()!=
           exact_linked->chainId.to_string()||
       meta.value(QStringLiteral("linkedChainRevision")).toString().
           toStdString()!=exact_revision)
        return fail("L3_M12_REOPEN_MATERIALIZATION_OWNER_OR_REV_STALE");
    const auto freshness=meta.value(QStringLiteral("freshnessState"));
    if(!freshness.isUndefined()&&freshness.toString()!=QStringLiteral("CURRENT")&&
       freshness.toString()!=QStringLiteral("STALE"))
        return fail("L3_M12_REOPEN_FRESHNESS_UNSUPPORTED");
    if(freshness.toString()==QStringLiteral("STALE")&&
       std::any_of(exact_linked->modules.begin(),exact_linked->modules.end(),
           [](const auto& node){return !node.controllerSuspended;}))
        return fail("L3_M12_REOPEN_STALE_PLAN_ACTIVE");
    if(document.object().contains(QStringLiteral("retainedController"))) {
        const auto retained=decode_m12_retained_controller(document.object());
        if(!retained||retained->controller_id!=*controller.value()||
           retained->linked_chain_id!=exact_linked->chainId||
           retained->last_removed_revision>=exact_linked->revision)
            return fail("L3_M12_REOPEN_RETAINED_CONTROLLER_CONFLICT");
    }
    std::vector<dsp::ModuleInstance> nodes;
    std::vector<dsp::ModuleExecutionBinding> bindings;
    nodes.reserve(exact_linked->modules.size());
    bindings.reserve(exact_linked->modules.size());
    for(const auto& m:exact_linked->modules) {
        if(m.instanceId.is_nil()||m.typeId.empty()||
           m.provenance!="REFERENCE_MATCH"||
           m.owner!="REFERENCE_MATCH"||
           m.linkState!="REF_LINKED"||
           !m.semanticNodeId||m.semanticNodeId->is_nil()||
           m.typeId=="rgsml.dsp.true-peak-limiter"||
           m.typeId=="rgsml.dsp.dither")
            return fail("L3_M12_REOPEN_FOREIGN_OR_FORBIDDEN_MODULE");
        if(m.typeId=="rgsml.dsp.streaming-true-peak-limiter" &&
           (&m!=&exact_linked->modules.back() ||
            std::any_of(exact_manual->modules.begin(),exact_manual->modules.end(),
                [](const auto& prior){return prior.typeId==
                    "rgsml.dsp.streaming-true-peak-limiter";})))
            return fail("L3_M12_REOPEN_OWNED_P2_TERMINAL_CONFLICT");
        for(const auto& old:doc.chains)
            if(old.chainId!=exact_linked->chainId)
                for(const auto& prior:old.modules)
                    if(prior.instanceId==m.instanceId)
                        return fail("L3_M12_REOPEN_MODULE_ID_COLLISION");
        for(const auto& prior:nodes)
            if(prior.instance_id().uuid()==m.instanceId||
               prior.semantic_node_id()==m.semanticNodeId)
                return fail("L3_M12_REOPEN_DUPLICATE_OWNED_NODE");
        const auto descriptor=registry_owner->find_descriptor(m.typeId);
        if(!descriptor)return R::failure(*descriptor.error());
        const auto& d=descriptor.value()->get();
        if(!d.algorithm_version()||!d.parameter_schema_id()||
           !m.algorithmVersion||!m.parameterSchemaId||
           *m.algorithmVersion!=*d.algorithm_version()||
           *m.parameterSchemaId!=*d.parameter_schema_id()||
           !registry_owner->has_factory(m.typeId))
            return fail("L3_M12_REOPEN_FACTORY_OR_SCHEMA_UNAVAILABLE");
        const auto decoded=dsp::decode_module_parameters_json(
            *d.parameter_schema_id(),m.parameters.canonical_utf8());
        if(!decoded)return R::failure(*decoded.error());
        const auto id=dsp::ModuleInstanceId::from_uuid(m.instanceId);
        if(!id)return R::failure(*id.error());
        auto native=dsp::ModuleInstance::create({
            *id.value(),m.typeId,m.enabled,m.userBypass,
            m.controllerSuspended,m.domainSuspended,
            dsp::ModuleProvenance::REFERENCE_MATCH,
            dsp::ModuleOwner::REFERENCE_MATCH,
            dsp::ModuleLinkState::REF_LINKED,m.semanticNodeId,{}});
        if(!native)return R::failure(*native.error());
        const dsp::ModuleExecutionBinding binding{
            *id.value(),*decoded.value()};
        if(!binding_matches_instance(*native.value(),binding))
            return fail("L3_M12_REOPEN_PARAMETER_BINDING_MISMATCH");
        auto live=registry_owner->create_module(m.typeId,binding.parameters);
        if(!live)return R::failure(*live.error());
        nodes.push_back(std::move(*native.value()));
        bindings.push_back(std::move(binding));
    }
    auto restored=dsp::ProcessingChain::restore(
        *registry_owner,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED},
        exact_linked->revision,std::move(nodes));
    if(!restored)return R::failure(*restored.error());
    return R::success(RestoredM12LinkedProductState{
        std::move(registry_owner),std::move(*restored.value()),
        std::move(bindings),*plan.value(),*controller.value(),
        std::move(checksum),exact_linked->chainId,
        exact_linked->revision,std::move(winner_checksum),
        std::move(protected_checksum),std::move(dependency)});
}
} // namespace rgsml::app::l3_automatch::m12_frozen
