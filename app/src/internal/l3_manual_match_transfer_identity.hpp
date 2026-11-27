#pragma once
#include "l3_manual_match_transfer_draft.hpp"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace rgsml::app::l3_automatch {
struct ManualMatchTransferIdentity final {
    project::OpaqueJsonValue canonical_content;
    std::string sha256;
    [[nodiscard]] constexpr bool is_solver_winner() const noexcept {return false;}
    [[nodiscard]] constexpr bool is_verified() const noexcept {return false;}
    [[nodiscard]] constexpr bool authorizes_project_commit() const noexcept {return false;}
};
// Exact ordered graph data, including bypass and ownership, is hashed
// separately from M10/M11 checksums. These prior checksums are provenance
// ONLY. They cannot attest safety or convergence of the derived graph.
[[nodiscard]] inline core::Result<ManualMatchTransferIdentity>
identify_manual_match_transfer_graph(const ManualMatchTransferDraft& draft)
{
    using R=core::Result<ManualMatchTransferIdentity>;
    const auto fail=[](const char* code){
        return R::failure(core::Error{core::ErrorCode::InvalidState,code});
    };
    const auto prior=draft.exact_prior_owner;
    if(!prior||!prior->replay||!prior->replay->registry_owner)
        return fail("MANUAL_MATCH_IDENTITY_NO_PRIOR_OWNER");
    const auto& replay=*prior->replay;
    const auto& manual=draft.manual_after.proposed_manual_chain;
    const auto& mb=draft.manual_after.proposed_bindings;
    const auto& lb=draft.linked_bindings_after;
    // The aggregate draft is not itself an authority. Reconstruct its
    // allowed delta from the immutable before-owner before hashing or
    // rendering, so an arbitrary peer edit cannot borrow this operation.
    auto priorManual=dsp::ProcessingChain::restore(*replay.registry_owner,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL},
        prior->exact_manual_chain_revision,prior->exact_manual_nodes);
    if(!priorManual)return R::failure(*priorManual.error());
    const auto expected=prepare_manual_match_transfer_graph(prior,*priorManual.value(),
        prior->exact_manual_bindings,*replay.registry_owner,draft.linked_instance);
    if(!expected)return R::failure(*expected.error());
    const auto same=[](const dsp::ProcessingChain& a,const dsp::ProcessingChain& b){
        return a.context()==b.context()&&a.revision()==b.revision()&&
            std::equal(a.instances().begin(),a.instances().end(),
                       b.instances().begin(),b.instances().end());
    };
    if(expected.value()->manual_instance!=draft.manual_instance||
       !same(expected.value()->manual_after.proposed_manual_chain,manual)||
       expected.value()->manual_after.proposed_bindings!=mb||
       !same(expected.value()->linked_after,draft.linked_after)||
       expected.value()->linked_bindings_after!=lb)
        return fail("MANUAL_MATCH_IDENTITY_UNAUTHORIZED_GRAPH_DELTA");
    if(conflicting_active_match_dsps(manual.instances(),draft.linked_after.instances()))
        return fail("MANUAL_MATCH_IDENTITY_ACTIVE_CONFLICT");
    const auto encode=[&](const dsp::ProcessingChain& chain,
                          std::span<const dsp::ModuleExecutionBinding> bindings)
                          ->std::optional<QJsonObject>{
        const auto nodes=chain.instances();
        if(nodes.size()!=bindings.size())return {};
        QJsonArray modules;
        for(std::size_t i=0;i<nodes.size();++i) {
            const auto& n=nodes[i];
            if(n.instance_id()!=bindings[i].instance_id)return {};
            const auto descriptor=replay.registry_owner->find_descriptor(n.module_type_id());
            if(!descriptor)return {};
            const auto& d=descriptor.value()->get();
            if(!d.algorithm_version()||!d.parameter_schema_id())return {};
            const auto encoded=dsp::encode_module_parameters_json(bindings[i].parameters);
            if(!encoded)return {};
            const auto canonical=project::OpaqueJsonValue::parse(*encoded.value());
            if(!canonical)return {};
            const auto decoded=dsp::decode_module_parameters_json(
                *d.parameter_schema_id(),canonical.value()->canonical_utf8());
            if(!decoded||*decoded.value()!=bindings[i].parameters)return {};
            const auto params=QJsonDocument::fromJson(
                QByteArray::fromStdString(canonical.value()->canonical_utf8()));
            if(!params.isObject())return {};
            const auto q=[](std::string_view v){return QString::fromStdString(std::string(v));};
            QJsonObject module{
                {"instanceId",q(n.instance_id().to_string())},
                {"typeId",q(n.module_type_id())},
                {"algorithmVersion",q(*d.algorithm_version())},
                {"parameterSchemaId",q(*d.parameter_schema_id())},
                {"enabled",n.enabled()},{"userBypass",n.user_bypass()},
                {"controllerSuspended",n.controller_suspended()},
                {"domainSuspended",n.domain_suspended()},
                {"provenance",static_cast<int>(n.provenance())},
                {"owner",static_cast<int>(n.owner())},
                {"linkState",static_cast<int>(n.link_state())},
                {"semanticNodeId",n.semantic_node_id()?q(n.semantic_node_id()->to_string()):QString{}},
                {"parameters",params.object()}};
            modules.append(module);
        }
        return QJsonObject{{"revision",QString::number(static_cast<qulonglong>(chain.revision()))},
            {"modules",modules}};
    };
    const auto manualContent=encode(manual,mb);
    const auto linkedContent=encode(draft.linked_after,lb);
    const auto priorManualContent=encode(*priorManual.value(),prior->exact_manual_bindings);
    const auto priorLinkedContent=encode(replay.reference_linked,replay.reference_bindings);
    if(!manualContent||!linkedContent||!priorManualContent||!priorLinkedContent)
        return fail("MANUAL_MATCH_IDENTITY_INVALID_GRAPH");
    const auto q=[](const std::string& v){return QString::fromStdString(v);};
    const QJsonObject provenance{
        {"projectId",q(replay.project_id.to_string())},{"sourceId",q(replay.source_id.to_string())},
        {"referenceId",q(replay.reference_id.to_string())},{"controllerId",q(replay.controller_id.to_string())},
        {"linkedChainId",q(replay.linked_chain_id.to_string())},{"planId",q(replay.plan_id.to_string())},
        {"planChecksum",q(replay.exact_plan_checksum)},
        {"winnerChecksum",q(replay.exact_m10_winner_checksum)},
        {"protectedChecksum",q(replay.exact_m11_protection_checksum)},
        {"sourceReferenceScope",q(replay.exact_source_reference_scope_revision)}};
    const QJsonObject record{
        {"schemaId",QStringLiteral("rgsml.reference-match.manual-transfer-candidate/1.0.0")},
        {"priorAcceptedProvenance",provenance},
        {"linkedInstance",q(draft.linked_instance.to_string())},
        {"manualInstance",q(draft.manual_instance.to_string())},
        {"manual",*manualContent},{"referenceLinked",*linkedContent},
        // Recovery data is part of the same canonical digest. It permits
        // exact reversal of the single authorized transfer after reopen;
        // the old accepted receipt alone does not record prior MANUAL values.
        {"priorManual",*priorManualContent},{"priorReferenceLinked",*priorLinkedContent},
        {"verificationState",QStringLiteral("NOT_VERIFIED")},
        {"candidateState",QStringLiteral("PROVISIONAL")}};
    auto canonical=project::OpaqueJsonValue::parse(
        QJsonDocument(record).toJson(QJsonDocument::Compact).toStdString());
    if(!canonical)return R::failure(*canonical.error());
    const auto hash=QCryptographicHash::hash(
        QByteArray::fromStdString(canonical.value()->canonical_utf8()),
        QCryptographicHash::Sha256).toHex().toStdString();
    return R::success(ManualMatchTransferIdentity{std::move(*canonical.value()),hash});
}
[[nodiscard]] inline core::Result<ManualMatchTransferIdentity>
identify_manual_match_transfer(const ManualMatchTransferDraft& draft)
{
    if(!draft.exact_prior_owner||!draft.exact_prior_owner->exact_prepared_owner)
        return core::Result<ManualMatchTransferIdentity>::failure(core::Error{
            core::ErrorCode::InvalidState,"MANUAL_MATCH_IDENTITY_NO_PREPARED_OWNER"});
    return identify_manual_match_transfer_graph(draft);
}
// Render the exact derived graph once, using the same native engine and
// the same PREPARED owner as the original. No ticket is installed and
// no verification receipt can arise from this audition.
[[nodiscard]] inline core::Result<render::RenderResult>
render_manual_match_transfer_candidate(const ManualMatchTransferDraft& draft,
    const ManualMatchTransferIdentity& identity,core::FrameCount maxBlock,
    const std::atomic<bool>* cancellation=nullptr)
{
    using R=core::Result<render::RenderResult>;
    const auto fail=[](const char* why){
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    const auto current=identify_manual_match_transfer(draft);
    if(!current||current.value()->sha256!=identity.sha256||
       current.value()->canonical_content!=identity.canonical_content)
        return fail("MANUAL_MATCH_TRANSFER_CANDIDATE_IDENTITY_MISMATCH");
    const auto prior=draft.exact_prior_owner;
    const auto& registry=prior->replay->registry_owner;
    const auto& manual=draft.manual_after.proposed_manual_chain;
    const auto& bindings=draft.manual_after.proposed_bindings;
    const auto nodes=manual.instances();
    const bool p2=!nodes.empty()&&nodes.back().module_type_id()=="rgsml.dsp.streaming-true-peak-limiter";
    const auto prefixCount=nodes.size()-(p2?1U:0U);
    std::vector<dsp::ModuleInstance> prefixNodes(
        nodes.begin(),nodes.begin()+static_cast<std::ptrdiff_t>(prefixCount));
    auto prefix=dsp::ProcessingChain::restore(*registry,manual.context(),
        manual.revision(),std::move(prefixNodes));
    if(!prefix)return R::failure(*prefix.error());
    std::optional<dsp::ProcessingChain> terminal;
    std::vector<dsp::ModuleExecutionBinding> terminalBindings;
    if(p2) {
        auto tail=dsp::ProcessingChain::restore(*registry,manual.context(),manual.revision(),{nodes.back()});
        if(!tail)return R::failure(*tail.error());
        terminal=std::move(*tail.value());
        terminalBindings.push_back(bindings.back());
    }
    const m12_frozen::M12NativeExecutionGraphView graph{
        registry,*prefix.value(),std::span<const dsp::ModuleExecutionBinding>(bindings).first(prefixCount),
        draft.linked_after,draft.linked_bindings_after,terminal,terminalBindings};
    return m12_frozen::render_m12_native_graph_pcm_once(graph,*prior->exact_prepared_owner,
        maxBlock,nullptr,cancellation);
}
} // namespace rgsml::app::l3_automatch
