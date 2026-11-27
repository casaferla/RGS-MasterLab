#include "project_session_view_model.hpp"
#include "internal/l3_m12_ref_linked_project_open_gate.hpp"
#include "internal/l3_m12_product_preview_worker_ticket.hpp"
#include "internal/l3_m12_exact_postcommit_readback.hpp"
#include "internal/l3_m12_current_search_transaction.hpp"
#include "internal/l3_m12_product_reopen_preflight.hpp"
#include "internal/l3_m12_owned_controller_suspension.hpp"
#include "internal/l3_manual_match_transfer_project.hpp"
#include "internal/l3_m12_retained_controller.hpp"
#include "internal/l3_m12_stale_rebind.hpp"
#include "internal/l3_m13_actual_native_worker.hpp"

#include "audition_region_view_model.hpp"
#include "dsp_chain_adapter_model.hpp"
#include "eq_view_model.hpp"
#include "gain_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"
#include "master_output_rate_state.hpp"
#include "playback_transport_view_model.hpp"
#include "source_selection_view_model.hpp"

#include <rgsml/audio/source_resource.hpp>
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/platform/windows/windows_resource_identity.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/platform/windows/windows_resource_writer.hpp>
#include <rgsml/project/project_repository.hpp>
#include <rgsml/project/master_output_rate_preference.hpp>

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

namespace rgsml::app {
namespace {

// Only the Qt owner enters a Product-gated Project operation. The local
// scope rejects nested Open/Save commands (including reentrant signals)
// without blocking the event loop or leaking a held Product epoch.
struct M18ScopedProjectOperation final {
    explicit M18ScopedProjectOperation(bool* active) noexcept
        :active_(active) {if(active_)*active_=true;}
    M18ScopedProjectOperation(const M18ScopedProjectOperation&)=delete;
    M18ScopedProjectOperation& operator=(const M18ScopedProjectOperation&)=delete;
    ~M18ScopedProjectOperation() {if(active_)*active_=false;}
private:
    bool* active_;
};

[[nodiscard]] core::Error invalid(std::string_view field)
{
    return core::Error{core::ErrorCode::InvalidArgument,
        "Project operation requires a local .rgsml path", {{"field", std::string(field)}}};
}

[[nodiscard]] core::Result<core::ResourceReference> read_reference(const QUrl& url)
{
    if (!url.isValid() || !url.isLocalFile() ||
        !url.toLocalFile().endsWith(QStringLiteral(".rgsml"), Qt::CaseInsensitive)) {
        return core::Result<core::ResourceReference>::failure(invalid("projectFile"));
    }
    const auto path = url.toLocalFile().toUtf8();
    const auto name = QFileInfo{url.toLocalFile()}.fileName().toUtf8();
    return platform::windows::WindowsResourceReader::make_read_reference(
        {path.constData(), static_cast<std::size_t>(path.size())},
        {name.constData(), static_cast<std::size_t>(name.size())});
}

[[nodiscard]] core::Result<core::ResourceReference> external_reference(
    const project::ProjectDocument& document, core::Uuid id)
{
    const auto resource = std::find_if(document.resources.begin(), document.resources.end(),
        [id](const project::Resource& item) { return item.resourceId == id; });
    if (resource == document.resources.end()) return core::Result<core::ResourceReference>::failure(
        invalid("resourceId"));
    const auto hint = std::find_if(resource->locatorHints.begin(), resource->locatorHints.end(),
        [](const project::LocatorHint& item) {
            return item.providerId == platform::windows::WindowsResourceReader::provider_id();
        });
    if (hint == resource->locatorHints.end()) return core::Result<core::ResourceReference>::failure(
        core::Error{core::ErrorCode::UnsupportedOperation,
            "No supported Windows local-file locator", {{"field", "locatorHints"}}});
    return platform::windows::WindowsResourceReader::make_read_reference(
        hint->locator, hint->displayName);
}

[[nodiscard]] core::Result<audio::SourceResource> probe(
    const core::ResourceReference& reference)
{
    auto reader = platform::windows::WindowsResourceReader::open_read_only(reference);
    if (!reader) return core::Result<audio::SourceResource>::failure(*reader.error());
    return audio::SourceResource::probe(std::move(*reader.value()));
}

[[nodiscard]] std::string provenance_to_string(dsp::ModuleProvenance p)
{
    switch (p) {
        case dsp::ModuleProvenance::MANUAL: return "MANUAL";
        case dsp::ModuleProvenance::REFERENCE_MATCH: return "REFERENCE_MATCH";
    }
    return "MANUAL";
}

[[nodiscard]] std::string owner_to_string(dsp::ModuleOwner o)
{
    switch (o) {
        case dsp::ModuleOwner::USER: return "USER";
        case dsp::ModuleOwner::REFERENCE_MATCH: return "REFERENCE_MATCH";
    }
    return "USER";
}

[[nodiscard]] std::string link_state_to_string(dsp::ModuleLinkState l)
{
    switch (l) {
        case dsp::ModuleLinkState::UNLINKED: return "UNLINKED";
        case dsp::ModuleLinkState::REF_LINKED: return "REF_LINKED";
    }
    return "UNLINKED";
}

[[nodiscard]] bool has_future_semantics(const project::ProjectDocument& doc)
{
    const auto metadata=QJsonDocument::fromJson(QByteArray::fromStdString(doc.referenceMatch.canonical_utf8()));
    auto unknown=metadata.object();
    if(l3_automatch::m12_frozen::decode_m12_retained_controller(unknown))
        unknown.remove(QStringLiteral("retainedController"));
    const bool unknownReferenceMatch=!metadata.isObject()||!unknown.isEmpty();
    if (doc.pipeline.repairChainId || doc.pipeline.conditioningChainId ||
        doc.activeProfileApplication ||
        doc.timeSelections.analysisScopes.canonical_utf8() != "[]" ||
        doc.timeSelections.processingRegions.canonical_utf8() != "[]" ||
        doc.analyses.canonical_utf8() != "[]" || doc.plans.canonical_utf8() != "[]" ||
        unknownReferenceMatch ||
        doc.masteringDNA.canonical_utf8() != "{}" ||
        !project::MasterOutputRatePreference::only_known_fields(doc.output) ||
        doc.derivedArtifacts.canonical_utf8() != "[]" ||
        doc.history.canonical_utf8() != "[]" ||
        doc.extensions.canonical_utf8() != "{}" ||
        doc.timeSelections.auditionRegions.size() > 2U) {
        return true;
    }

    if (doc.chains.size() > 1U) {
        return true;
    }

    return false;
}

// A fully rehydrated native REF_LINKED segment is no longer an
// unsupported "future" processor. Other opaque processing/history
// semantics remain honestly degraded and are never thrown away.
[[nodiscard]] bool has_unsupported_semantics_after_native_m12_reopen(
    const project::ProjectDocument& saved)
{
    auto withoutKnownLinked=saved;
    std::erase_if(withoutKnownLinked.chains,
        [](const project::Chain& chain) {
            return chain.stage=="MASTER"&&chain.segment=="REF_LINKED";
        });
    const auto metadata=QJsonDocument::fromJson(QByteArray::fromStdString(saved.referenceMatch.canonical_utf8()));
    if(!metadata.isObject())return true;
    auto other=metadata.object();
    other.remove(QStringLiteral("materialization"));
    const auto canonical=project::OpaqueJsonValue::parse(QJsonDocument(other).toJson(QJsonDocument::Compact).toStdString());
    if(!canonical)return true;
    withoutKnownLinked.referenceMatch=*canonical.value();
    return has_future_semantics(withoutKnownLinked);
}

struct MasteringChainCandidate final {
    std::optional<MasteringChainState> chainState;
    bool isMaterialized{false};
};

[[nodiscard]] core::Result<MasteringChainCandidate> validate_mastering_chain(
    const project::ProjectDocument& doc,
    const dsp::ModuleRegistry& registry,
    const ProjectSessionViewModel::UuidFactory& uuidFactory)
{
    if (!doc.pipeline.masteringChainId.has_value()) {
        if (!doc.chains.empty()) {
            return core::Result<MasteringChainCandidate>::failure(core::Error{
                core::ErrorCode::InvalidArgument,
                "Ambiguous project document: pipeline.masteringChainId is absent but doc.chains is not empty"});
        }
        const auto chainUuid = uuidFactory();
        const auto gainUuid = uuidFactory();
        const auto eqUuid = uuidFactory();
        const auto compUuid = uuidFactory();
        const auto gainId = *dsp::ModuleInstanceId::from_uuid(gainUuid).value();
        const auto eqId = *dsp::ModuleInstanceId::from_uuid(eqUuid).value();
        const auto compId = *dsp::ModuleInstanceId::from_uuid(compUuid).value();

        auto defaultState = MasteringChainState::create_default(registry, chainUuid, gainId, eqId, compId);
        if (!defaultState) {
            return core::Result<MasteringChainCandidate>::failure(*defaultState.error());
        }
        return core::Result<MasteringChainCandidate>::success(
            MasteringChainCandidate{std::move(*defaultState.value()), true});
    }

    if (doc.chains.empty()) {
        return core::Result<MasteringChainCandidate>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "Referenced mastering chain is absent from doc.chains",
            {{"field", "pipeline.masteringChainId"}}});
    }

    const auto chainId = *doc.pipeline.masteringChainId;
    const auto chainIt = std::find_if(doc.chains.begin(), doc.chains.end(),
        [chainId](const project::Chain& c) { return c.chainId == chainId; });
    if (chainIt == doc.chains.end()) {
        return core::Result<MasteringChainCandidate>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "Referenced mastering chain ID not found in doc.chains",
            {{"field", "pipeline.masteringChainId"}}});
    }

    const auto& chain = *chainIt;
    if (chain.stage != "MASTER" || chain.segment != "MANUAL") {
        return core::Result<MasteringChainCandidate>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "Mastering chain must have stage MASTER and segment MANUAL",
            {{"stage", chain.stage}, {"segment", chain.segment}}});
    }

    const bool extended=chain.modules.size()>=5U &&
        chain.modules[2].typeId=="rgsml.dsp.dynamic-eq";
    const std::size_t offset=extended?2U:0U;
    const std::size_t baseCount=chain.modules.size()-offset;
    if (baseCount < 2U || baseCount > 5U || (extended && baseCount<3U)) {
        return core::Result<MasteringChainCandidate>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "Supported mastering chain must contain 2/3/4 legacy modules or 5 with typed P2",
            {{"moduleCount", std::to_string(chain.modules.size())}}});
    }

    const auto& m0 = chain.modules[0];
    const auto& m1 = chain.modules[1];

    if (m0.typeId != "rgsml.dsp.gain") {
        return core::Result<MasteringChainCandidate>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "Module 0 of supported mastering chain must be rgsml.dsp.gain",
            {{"typeId", m0.typeId}}});
    }

    if (m1.typeId != "rgsml.dsp.parametric-eq") {
        return core::Result<MasteringChainCandidate>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "Module 1 of supported mastering chain must be rgsml.dsp.parametric-eq",
            {{"typeId", m1.typeId}}});
    }

    const auto validate_module_metadata = [&](const project::Module& m,
                                              std::string_view expectedTypeId) -> core::Status {
        if (m.provenance != "MANUAL" || m.owner != "USER" ||
            m.linkState != "UNLINKED" || m.semanticNodeId.has_value()) {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidArgument,
                "Module structural ownership/link state is incompatible",
                {{"typeId", m.typeId}}});
        }

        auto descRes = registry.find_descriptor(expectedTypeId);
        if (!descRes) {
            return core::Status::failure(*descRes.error());
        }
        const auto& desc = descRes.value()->get();

        if (!desc.algorithm_version() || !m.algorithmVersion ||
            *m.algorithmVersion != *desc.algorithm_version()) {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidArgument,
                "Module algorithmVersion missing or does not match authoritative descriptor",
                {{"typeId", m.typeId}}});
        }

        if (!desc.parameter_schema_id() || !m.parameterSchemaId ||
            *m.parameterSchemaId != *desc.parameter_schema_id()) {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidArgument,
                "Module parameterSchemaId missing or does not match authoritative descriptor",
                {{"typeId", m.typeId}}});
        }

        return core::Status::success();
    };

    auto v0 = validate_module_metadata(m0, "rgsml.dsp.gain");
    if (!v0) return core::Result<MasteringChainCandidate>::failure(*v0.error());

    auto v1 = validate_module_metadata(m1, "rgsml.dsp.parametric-eq");
    if (!v1) return core::Result<MasteringChainCandidate>::failure(*v1.error());

    auto decodedGain = dsp::decode_gain_parameters_json(m0.parameters.canonical_utf8());
    if (!decodedGain) return core::Result<MasteringChainCandidate>::failure(*decodedGain.error());

    auto decodedEq = dsp::decode_parametric_eq_parameters_json(m1.parameters.canonical_utf8());
    if (!decodedEq) return core::Result<MasteringChainCandidate>::failure(*decodedEq.error());

    const auto gainInstanceId = *dsp::ModuleInstanceId::from_uuid(m0.instanceId).value();
    const auto eqInstanceId = *dsp::ModuleInstanceId::from_uuid(m1.instanceId).value();

    dsp::ModuleInstanceSpec gainSpec{
        .instance_id = gainInstanceId,
        .module_type_id = m0.typeId,
        .enabled = m0.enabled,
        .user_bypass = m0.userBypass,
        .controller_suspended = m0.controllerSuspended,
        .domain_suspended = m0.domainSuspended,
        .provenance = dsp::ModuleProvenance::MANUAL,
        .owner = dsp::ModuleOwner::USER,
        .link_state = dsp::ModuleLinkState::UNLINKED,
        .semantic_node_id = std::nullopt,
        .parameter_state = dsp::ModuleParameterState{}
    };
    auto gainInst = dsp::ModuleInstance::create(std::move(gainSpec));
    if (!gainInst) return core::Result<MasteringChainCandidate>::failure(*gainInst.error());

    dsp::ModuleInstanceSpec eqSpec{
        .instance_id = eqInstanceId,
        .module_type_id = m1.typeId,
        .enabled = m1.enabled,
        .user_bypass = m1.userBypass,
        .controller_suspended = m1.controllerSuspended,
        .domain_suspended = m1.domainSuspended,
        .provenance = dsp::ModuleProvenance::MANUAL,
        .owner = dsp::ModuleOwner::USER,
        .link_state = dsp::ModuleLinkState::UNLINKED,
        .semantic_node_id = std::nullopt,
        .parameter_state = dsp::ModuleParameterState{}
    };
    auto eqInst = dsp::ModuleInstance::create(std::move(eqSpec));
    if (!eqInst) return core::Result<MasteringChainCandidate>::failure(*eqInst.error());

    core::Uuid compUuid;
    dsp::CompressorParameters decodedComp = *dsp::CompressorParameters::create_default().value();
    bool compUserBypass = true;
    bool compEnabled = true;
    bool compCtrlSuspended = false;
    bool compDomainSuspended = false;

    if (baseCount == 2U) {
        // Legacy 2-module load: create one Compressor instance, exact defaults, user bypass = true
        compUuid = uuidFactory();
    } else {
        // 3-module load: validate and restore Module 2 (Compressor)
        const auto& m2 = chain.modules[2+offset];
        if (m2.typeId != "rgsml.dsp.compressor") {
            return core::Result<MasteringChainCandidate>::failure(core::Error{
                core::ErrorCode::InvalidArgument,
                "Module 2 of 3/4-module mastering chain must be rgsml.dsp.compressor",
                {{"typeId", m2.typeId}}});
        }
        auto v2 = validate_module_metadata(m2, "rgsml.dsp.compressor");
        if (!v2) return core::Result<MasteringChainCandidate>::failure(*v2.error());

        auto decodedCompRes = dsp::decode_compressor_parameters_json(m2.parameters.canonical_utf8());
        if (!decodedCompRes) return core::Result<MasteringChainCandidate>::failure(*decodedCompRes.error());

        compUuid = m2.instanceId;
        decodedComp = *decodedCompRes.value();
        compUserBypass = m2.userBypass;
        compEnabled = m2.enabled;
        compCtrlSuspended = m2.controllerSuspended;
        compDomainSuspended = m2.domainSuspended;
    }

    const auto compInstanceId = *dsp::ModuleInstanceId::from_uuid(compUuid).value();

    dsp::ModuleInstanceSpec compSpec{
        .instance_id = compInstanceId,
        .module_type_id = "rgsml.dsp.compressor",
        .enabled = compEnabled,
        .user_bypass = compUserBypass,
        .controller_suspended = compCtrlSuspended,
        .domain_suspended = compDomainSuspended,
        .provenance = dsp::ModuleProvenance::MANUAL,
        .owner = dsp::ModuleOwner::USER,
        .link_state = dsp::ModuleLinkState::UNLINKED,
        .semantic_node_id = std::nullopt,
        .parameter_state = dsp::ModuleParameterState{}
    };
    auto compInst = dsp::ModuleInstance::create(std::move(compSpec));
    if (!compInst) return core::Result<MasteringChainCandidate>::failure(*compInst.error());

    std::vector<dsp::ModuleInstance> restoredModules{
        *gainInst.value(), *eqInst.value()};
    std::optional<dsp::ModuleInstanceId> dynamicId,transientId;
    std::optional<dsp::DynamicEqParameters> decodedDynamic;
    std::optional<dsp::TransientShaperParameters> decodedTransient;
    if(extended) {
        const auto& dynamic=chain.modules[2];
        const auto& transient=chain.modules[3];
        if(transient.typeId!="rgsml.dsp.transient-shaper" ||
           !registry.has_factory(dynamic.typeId) ||
           !registry.has_factory(transient.typeId))
            return core::Result<MasteringChainCandidate>::failure(core::Error{
                core::ErrorCode::InvalidArgument,"MANUAL_MATCH_DSP_RESTORE_REQUIRES_TYPED_PAIR"});
        const auto vd=validate_module_metadata(dynamic,"rgsml.dsp.dynamic-eq");
        if(!vd)return core::Result<MasteringChainCandidate>::failure(*vd.error());
        const auto vt=validate_module_metadata(transient,"rgsml.dsp.transient-shaper");
        if(!vt)return core::Result<MasteringChainCandidate>::failure(*vt.error());
        auto de=dsp::decode_dynamic_eq_parameters_json(dynamic.parameters.canonical_utf8());
        if(!de)return core::Result<MasteringChainCandidate>::failure(*de.error());
        auto ts=dsp::decode_transient_shaper_parameters_json(transient.parameters.canonical_utf8());
        if(!ts)return core::Result<MasteringChainCandidate>::failure(*ts.error());
        const auto make=[](const project::Module& m) -> core::Result<dsp::ModuleInstance> {
            auto id=dsp::ModuleInstanceId::from_uuid(m.instanceId);
            if(!id)return core::Result<dsp::ModuleInstance>::failure(*id.error());
            return dsp::ModuleInstance::create(dsp::ModuleInstanceSpec{
                .instance_id=*id.value(),.module_type_id=m.typeId,
                .enabled=m.enabled,.user_bypass=m.userBypass,
                .controller_suspended=m.controllerSuspended,.domain_suspended=m.domainSuspended,
                .provenance=dsp::ModuleProvenance::MANUAL,.owner=dsp::ModuleOwner::USER,
                .link_state=dsp::ModuleLinkState::UNLINKED,.semantic_node_id=std::nullopt,
                .parameter_state=dsp::ModuleParameterState{}});
        };
        auto di=make(dynamic);
        if(!di)return core::Result<MasteringChainCandidate>::failure(*di.error());
        auto ti=make(transient);
        if(!ti)return core::Result<MasteringChainCandidate>::failure(*ti.error());
        dynamicId=di.value()->instance_id();
        transientId=ti.value()->instance_id();
        decodedDynamic=std::move(*de.value());
        decodedTransient=std::move(*ts.value());
        restoredModules.push_back(std::move(*di.value()));
        restoredModules.push_back(std::move(*ti.value()));
    }
    restoredModules.push_back(std::move(*compInst.value()));
    std::optional<dsp::ModuleInstanceId> stereoMsId;
    std::optional<dsp::StereoMsParameters> decodedStereoMs;
    if (baseCount >= 4U) {
        // A four/five-node document requires an exact typed
        // fourth Stereo/M-S node. Never silently drop or default its fields.
        const auto& ms = chain.modules[3+offset];
        if (ms.typeId != "rgsml.dsp.stereo-ms") {
            return core::Result<MasteringChainCandidate>::failure(core::Error{
                core::ErrorCode::InvalidArgument,
                "Module 3 must be rgsml.dsp.stereo-ms",
                {{"typeId", ms.typeId}}});
        }
        const auto metadata = validate_module_metadata(ms, "rgsml.dsp.stereo-ms");
        if (!metadata) return core::Result<MasteringChainCandidate>::failure(
            *metadata.error());
        auto decoded = dsp::decode_stereo_ms_parameters_json(
            ms.parameters.canonical_utf8());
        if (!decoded) return core::Result<MasteringChainCandidate>::failure(
            *decoded.error());
        stereoMsId = *dsp::ModuleInstanceId::from_uuid(ms.instanceId).value();
        decodedStereoMs = *decoded.value();
        dsp::ModuleInstanceSpec msSpec{
            .instance_id = *stereoMsId,
            .module_type_id = ms.typeId,
            .enabled = ms.enabled,
            .user_bypass = ms.userBypass,
            .controller_suspended = ms.controllerSuspended,
            .domain_suspended = ms.domainSuspended,
            .provenance = dsp::ModuleProvenance::MANUAL,
            .owner = dsp::ModuleOwner::USER,
            .link_state = dsp::ModuleLinkState::UNLINKED,
            .semantic_node_id = std::nullopt,
            .parameter_state = dsp::ModuleParameterState{}
        };
        auto msInstance = dsp::ModuleInstance::create(std::move(msSpec));
        if (!msInstance) return core::Result<MasteringChainCandidate>::failure(
            *msInstance.error());
        restoredModules.push_back(std::move(*msInstance.value()));
    }

    // Additive opt-in P2 fifth module; no schema change to historical 2/3/4.
    // All fields must pass preflight before changing SOURCE, audio or chain.
    std::optional<dsp::ModuleInstanceId> p2Id;
    std::optional<dsp::StreamingTruePeakLimiterParameters> decodedP2;
    if (baseCount == 5U) {
        const auto& p2 = chain.modules[4+offset];
        constexpr std::string_view kP2Type{"rgsml.dsp.streaming-true-peak-limiter"};
        if (p2.typeId != kP2Type) {
            return core::Result<MasteringChainCandidate>::failure(core::Error{
                core::ErrorCode::InvalidArgument,
                "Module 4 must be the exact opt-in Streaming P2 type",
                {{"typeId", p2.typeId}}});
        }
        const auto metadata = validate_module_metadata(p2,kP2Type);
        if (!metadata) return core::Result<MasteringChainCandidate>::failure(
            *metadata.error());
        if (!registry.has_factory(kP2Type)) {
            return core::Result<MasteringChainCandidate>::failure(core::Error{
                core::ErrorCode::UnsupportedOperation,
                "P2 project requires explicitly installed I2 registry"});
        }
        auto decoded = dsp::decode_streaming_true_peak_limiter_parameters_json(
            p2.parameters.canonical_utf8());
        if (!decoded) return core::Result<MasteringChainCandidate>::failure(
            *decoded.error());
        p2Id = *dsp::ModuleInstanceId::from_uuid(p2.instanceId).value();
        decodedP2 = *decoded.value();
        dsp::ModuleInstanceSpec p2Spec{
            .instance_id = *p2Id,
            .module_type_id = p2.typeId,
            .enabled = p2.enabled,
            .user_bypass = p2.userBypass,
            .controller_suspended = p2.controllerSuspended,
            .domain_suspended = p2.domainSuspended,
            .provenance = dsp::ModuleProvenance::MANUAL,
            .owner = dsp::ModuleOwner::USER,
            .link_state = dsp::ModuleLinkState::UNLINKED,
            .semantic_node_id = std::nullopt,
            .parameter_state = dsp::ModuleParameterState{}
        };
        auto p2Instance = dsp::ModuleInstance::create(std::move(p2Spec));
        if (!p2Instance) return core::Result<MasteringChainCandidate>::failure(
            *p2Instance.error());
        restoredModules.push_back(std::move(*p2Instance.value()));
    }

    auto regPtr = std::make_shared<const dsp::ModuleRegistry>(registry);
    auto chainRes = dsp::ProcessingChain::restore(*regPtr,
        {dsp::ProcessingStage::MASTER, dsp::ChainSegment::MANUAL},
        chain.revision, std::move(restoredModules));
    if (!chainRes) return core::Result<MasteringChainCandidate>::failure(*chainRes.error());

    auto stateRes = MasteringChainState::restore(std::move(regPtr),
        chainId,
        std::move(*chainRes.value()),
        gainInstanceId,
        *decodedGain.value(),
        eqInstanceId,
        *decodedEq.value(),
        compInstanceId,
        decodedComp,
        std::move(stereoMsId),
        std::move(decodedStereoMs),
        std::move(p2Id),
        std::move(decodedP2),std::move(dynamicId),std::move(decodedDynamic),
        std::move(transientId),std::move(decodedTransient));
    if (!stateRes) return core::Result<MasteringChainCandidate>::failure(*stateRes.error());

    return core::Result<MasteringChainCandidate>::success(
        MasteringChainCandidate{std::move(*stateRes.value()), true});
}

}  // namespace

ProjectSessionViewModel::ProjectSessionViewModel(
    SourceSelectionViewModel* source, GoldSelectionViewModel* gold,
    AuditionRegionViewModel* region, PlaybackTransportViewModel* playback,
    MasteringChainState* masteringChainState,
    GainViewModel* gainViewModel,
    EqViewModel* eqViewModel,
    MasteringPreviewController* previewController,
    UuidFactory uuidFactory, QObject* parent)
    : ProjectSessionViewModel(source, gold, region, playback,
                              masteringChainState, gainViewModel, eqViewModel,
                              nullptr, previewController, std::move(uuidFactory), parent)
{
}

ProjectSessionViewModel::ProjectSessionViewModel(
    SourceSelectionViewModel* source, GoldSelectionViewModel* gold,
    AuditionRegionViewModel* region, PlaybackTransportViewModel* playback,
    MasteringChainState* masteringChainState,
    GainViewModel* gainViewModel,
    EqViewModel* eqViewModel,
    DspChainAdapterModel* adapterModel,
    MasteringPreviewController* previewController,
    UuidFactory uuidFactory, QObject* parent)
    : QObject(parent), source_(source), gold_(gold), region_(region),
      playback_(playback), masteringChainState_(masteringChainState),
      gainViewModel_(gainViewModel), eqViewModel_(eqViewModel),
      adapterModel_(adapterModel), previewController_(previewController),
      uuidFactory_(std::move(uuidFactory))
{
    if (!uuidFactory_) {
        uuidFactory_ = [] {
            const auto text = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower().toStdString();
            auto parsed = core::Uuid::parse(text);
            return parsed ? *parsed.value() : core::Uuid{};
        };
    }
    connect(source_, &SourceSelectionViewModel::sourceChanged,
            this, &ProjectSessionViewModel::on_source_changed);
    connect(gold_, &GoldSelectionViewModel::changed,
            this, &ProjectSessionViewModel::on_gold_changed);
    connect(region_, &AuditionRegionViewModel::changed,
            this, &ProjectSessionViewModel::on_region_changed);
    if(previewController_) {
        // A newly installed/revoked M12 ticket changes whether SaveAs
        // can persist the actual audible graph. Qt/QML must re-evaluate
        // canSaveProject immediately, even with unchanged SOURCE.
        connect(previewController_,&MasteringPreviewController::changed,
                this,&ProjectSessionViewModel::changed);
    }
}

void ProjectSessionViewModel::set_reference_region_view_model(
    AuditionRegionViewModel* region)
{
    if(!region ||referenceRegion_ ||QThread::currentThread()!=thread()
       ||m18ProjectOperationActive_||isCommittingProjectOpen_)return;
    referenceRegion_=region;
    connect(referenceRegion_,&AuditionRegionViewModel::changed,
            this,&ProjectSessionViewModel::on_reference_region_changed);
}

bool ProjectSessionViewModel::can_save_project() const
{
    // The I2 fifth MANUAL module now has a distinct typed JSON codec and
    // opt-in restore path. SaveAs preflights the exact complete chain before
    // any on-disk project publication; legacy sessions remain unchanged.
    // A persisted calculated Project cannot be re-exported as an
    // ordinary MANUAL snapshot just because a private preview ticket
    // was revoked. Detect both native REF nodes and orphaned / damaged
    // materialization metadata; opening such a Project is separately
    // gated until full REF replay is installed.
    const bool persistedLinked=opened_ &&
        l3_automatch::m12_frozen::
        assess_current_product_ref_linked_open_safety(
            opened_->document())!=
            l3_automatch::m12_frozen::ProjectOpenLinkedSafety::
                ordinary_project_without_ref_linked;
    const auto ticket=previewController_
        ?previewController_->private_m12_product_preview_ticket()
        :nullptr;
    if(persistedLinked||ticket) {
        // A native draft / orphaned read-only Project never grants SaveAs.
        // The actual Product M12 model publication and receipt are required.
        if(!source_||!source_->source_resource()||!opened_||
           !ticket||!ticket->replay||!previewController_||
           !masteringChainState_)return false;
        const bool receiptFromThisSession=
            !m12UndoHistory_.empty()&&
            m12UndoHistory_.back().receipt.has_value()&&
            m12UndoHistory_.back().afterTicket==ticket&&
            l3_automatch::m12_frozen::m12_exact_project_snapshot_equal(
                *opened_,m12UndoHistory_.back().after);
        const bool importedWithExactReadback=
            m12ExactReplayReadback_.has_value()&&
            l3_automatch::m12_frozen::m12_exact_project_snapshot_equal(
                *opened_,*m12ExactReplayReadback_);
        if(!receiptFromThisSession&&!importedWithExactReadback)
            return false;
        const auto& persisted=opened_->document();
        if(ticket->replay->project_id!=persisted.projectId||
           ticket->replay->source_id!=persisted.sourceResourceId||
           !persisted.references.activeReferenceId||
           ticket->replay->reference_id!=
               *persisted.references.activeReferenceId)
            return false;
        const auto& provider=previewController_->snapshot_provider();
        const auto& chain=masteringChainState_->chain();
        const auto nodes=chain.instances();
        return provider&&provider()==ticket->exact_prepared_owner&&
            chain.revision()==ticket->exact_manual_chain_revision&&
            nodes.size()==ticket->exact_manual_nodes.size()&&
            std::equal(nodes.begin(),nodes.end(),
                       ticket->exact_manual_nodes.begin())&&
            masteringChainState_->execution_bindings()==
                ticket->exact_manual_bindings;
    }
    return source_&&source_->source_resource();
}

core::Uuid ProjectSessionViewModel::next_id() { return uuidFactory_(); }

void ProjectSessionViewModel::publish_error(const core::Error& error)
{
    errorMessage_ = QString::fromStdString(error.message());
    if (errorMessage_.isEmpty()) errorMessage_ = QStringLiteral("Project operation failed.");
    emit changed();
}

void ProjectSessionViewModel::clear_error()
{
    errorMessage_.clear();
    emit changed();
}

bool ProjectSessionViewModel::chooseM18WavEncoding(int index)
{
    // A format change is an OUTPUT preference, not permission to write
    // a file or to reuse an old M17 MASTER. Keep uint64 seed entirely
    // in native C++; QML never conveys it as an imprecise JS Number.
    if(QThread::currentThread()!=thread()||m18ProjectOperationActive_
       ||(index!=0 &&index!=1))return false;
    project::M18MasterWavPreference next{
        index==0?project::M18MasterWavEncoding::PCM24_TPDF
                :project::M18MasterWavEncoding::IEEE_FLOAT32,
        index==0
            ?std::optional<std::uint64_t>{m18_pcm24_seed()}
            :std::nullopt};
    if(m18OutputPreference_&&*m18OutputPreference_==next)return true;
    m18OutputPreference_=next;
    // M12 imported proof binds the entire OUTPUT/project content.
    m12ExactReplayReadback_.reset();
    emit changed();
    return true;
}

void ProjectSessionViewModel::on_source_changed()
{
    const auto* source = source_->source_resource();
    // A presentation-only change on the SAME original is not a new
    // source epoch. Revoke a linked ticket only when the actual
    // immutable audio owner differs (or disappears).
    const bool resourceChanged=source
        ?(!lastSource_||
          !lastSource_->same_resource_identity(source->reference()))
        :lastSource_.has_value();
    if(resourceChanged&&previewController_ &&
       (previewController_->has_private_m12_product_preview_ticket()||
        previewController_->private_manual_transfer_preview_ticket())) {
        if(!isCommittingProjectOpen_&&opened_) {
            const auto prior=previewController_->private_m12_product_preview_ticket();
            if(prior&&prior->replay&&l3_automatch::m12_frozen::
                m12_reopened_persisted_replay_matches_exact_ticket(opened_->document(),*prior->replay))
                m12RetiredReplayIdentity_=prior->replay;
        }
        previewController_->set_private_m12_product_preview_ticket({});
    }
    if(resourceChanged) {
        ++m12ProductOwnershipEpoch_;
        m12UndoHistory_.clear();
        m12RedoHistory_.clear();
        // Persisted REF replay is tied to the exact original owners,
        // not merely to the same displayed Source/Reference names.
        m12ExactReplayReadback_.reset();
    }
    if (source && lastSource_ &&
        !lastSource_->same_resource_identity(source->reference())) sourceId_.reset();
    if (source) lastSource_ = source->reference();
    emit changed();
}

void ProjectSessionViewModel::on_gold_changed()
{
    const auto* gold = gold_->gold_resource();
    const bool resourceChanged=gold
        ?(!lastGold_||
          !lastGold_->same_resource_identity(gold->reference()))
        :lastGold_.has_value();
    if(resourceChanged&&previewController_ &&
       (previewController_->has_private_m12_product_preview_ticket()||
        previewController_->private_manual_transfer_preview_ticket())) {
        if(!isCommittingProjectOpen_&&opened_) {
            const auto prior=previewController_->private_m12_product_preview_ticket();
            if(prior&&prior->replay&&l3_automatch::m12_frozen::
                m12_reopened_persisted_replay_matches_exact_ticket(opened_->document(),*prior->replay))
                m12RetiredReplayIdentity_=prior->replay;
        }
        previewController_->set_private_m12_product_preview_ticket({});
    }
    if(resourceChanged) {
        ++m12ProductOwnershipEpoch_;
        m12UndoHistory_.clear();
        m12RedoHistory_.clear();
        // Persisted REF replay is tied to the exact original owners,
        // not merely to the same displayed Source/Reference names.
        m12ExactReplayReadback_.reset();
    }
    if (!gold) {
        goldId_.reset(); referenceId_.reset();
        referenceRegionId_.reset(); lastGold_.reset();
    }
    else {
        if (lastGold_ && !lastGold_->same_resource_identity(gold->reference())) {
            goldId_.reset(); referenceId_.reset();
            referenceRegionId_.reset();
        }
        lastGold_ = gold->reference();
    }
    emit changed();
}

void ProjectSessionViewModel::on_region_changed()
{
    if(!isCommittingProjectOpen_)m12ExactReplayReadback_.reset();
    if (!region_->region() && regionId_) {
        clearedRegionId_ = regionId_;
        regionId_.reset();
    }
    emit changed();
}

void ProjectSessionViewModel::on_reference_region_changed()
{
    if(!isCommittingProjectOpen_)m12ExactReplayReadback_.reset();
    if(referenceRegion_ &&!referenceRegion_->region())
        referenceRegionId_.reset();
    emit changed();
}

core::Result<project::ProjectSnapshot> ProjectSessionViewModel::current_snapshot(
    const QString& destinationStem)
{
    const auto* source = source_->source_resource();
    if (!source) return core::Result<project::ProjectSnapshot>::failure(
        invalid("source"));
    if (!projectId_) projectId_ = next_id();
    if (!sourceId_) sourceId_ = next_id();
    project::ProjectDocument doc;
    std::vector<project::OptionalEntry> optional;
    if (opened_) { doc = opened_->document(); optional = opened_->optional_entries(); }
    if (opened_ && doc.sourceResourceId != *sourceId_ && !sessionChainMaterialized_ && has_future_semantics(doc)) {
        return core::Result<project::ProjectSnapshot>::failure(core::Error{
            core::ErrorCode::UnsupportedOperation,
            "Cannot replace Source while preserved future project semantics refer to it"});
    }
    if (opened_ && doc.sourceResourceId != *sourceId_)
        doc.timeSelections.auditionRegions.clear();
    if (clearedRegionId_) {
        std::erase_if(doc.timeSelections.auditionRegions,
            [this](const project::AuditionRegion& item) {
                return item.regionId == *clearedRegionId_;
            });
    }
    doc.projectId = *projectId_;
    if (!opened_) doc.displayName = destinationStem.toUtf8().toStdString();
    doc.sourceResourceId = *sourceId_;
    const auto upsert_resource = [&](core::Uuid id, const core::ResourceReference& ref) {
        const auto found = std::find_if(doc.resources.begin(), doc.resources.end(),
            [id](const project::Resource& item) { return item.resourceId == id; });
        project::LocatorHint hint{ref.provider_id(), ref.locator(), ref.display_name()};
        if (found == doc.resources.end())
            doc.resources.push_back({id, "AUDIO", {std::move(hint)}, std::nullopt});
        else found->locatorHints = {std::move(hint)};
    };
    upsert_resource(*sourceId_, source->reference());
    const auto* gold = gold_->gold_resource();
    if (gold) {
        if (!goldId_) goldId_ = next_id();
        if (!referenceId_) referenceId_ = next_id();
        upsert_resource(*goldId_, gold->reference());
        const auto found = std::find_if(doc.references.items.begin(), doc.references.items.end(),
            [this](const project::Reference& item) { return item.referenceId == *referenceId_; });
        if (found == doc.references.items.end())
            doc.references.items.push_back({*referenceId_, "GOLD", *goldId_});
        else found->resourceId = *goldId_;
        doc.references.activeReferenceId = *referenceId_;
    } else doc.references.activeReferenceId.reset();
    const auto region = region_->region();
    if (region) {
        if (!regionId_) regionId_ = next_id();
        const auto found = std::find_if(doc.timeSelections.auditionRegions.begin(),
            doc.timeSelections.auditionRegions.end(),
            [this](const project::AuditionRegion& item) { return item.regionId == *regionId_; });
        project::AuditionRegion value{*regionId_, *sourceId_, "SOURCE",
            region->begin().value(), region->end().value(),
            region_->loop_enabled()};
        if (found == doc.timeSelections.auditionRegions.end())
            doc.timeSelections.auditionRegions.push_back(std::move(value));
        else *found = std::move(value);
    }

    // REFERENCE is a distinct original file and frame clock. No recorded
    // REFERENCE interval may survive a removed/replaced reference resource.
    std::erase_if(doc.timeSelections.auditionRegions,
        [](const project::AuditionRegion& item) {
            return item.frameDomain=="REFERENCE";
        });
    if(gold &&goldId_ &&referenceRegion_ &&referenceRegion_->region()) {
        const auto refRange=*referenceRegion_->region();
        if(!referenceRegionId_)referenceRegionId_=next_id();
        doc.timeSelections.auditionRegions.push_back({
            *referenceRegionId_,*goldId_,"REFERENCE",
            refRange.begin().value(),refRange.end().value(),
            referenceRegion_->loop_enabled()});
    }

    if (masteringChainState_ && (!opened_ || sessionChainMaterialized_)) {
        const auto chainId = masteringChainState_->chain_id();
        doc.pipeline.masteringChainId = chainId;

        project::Chain masteringChain;
        masteringChain.chainId = chainId;
        masteringChain.stage = "MASTER";
        masteringChain.segment = "MANUAL";
        masteringChain.revision = masteringChainState_->chain().revision();

        // Fail-closed helper for Module serialization
        const auto serialize_module = [&](const std::string_view typeId,
                                           const std::string_view expectedParameterSchemaId,
                                           const auto& instanceResult,
                                           const auto& jsonCodecResult) -> core::Result<project::Module> {
            if (!instanceResult) {
                return core::Result<project::Module>::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "Failed to obtain module instance for mastering chain serialization",
                    {{"typeId", std::string(typeId)}}});
            }
            const auto descResult = masteringChainState_->find_descriptor(typeId);
            if (!descResult) {
                return core::Result<project::Module>::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "Failed to obtain module descriptor for mastering chain serialization",
                    {{"typeId", std::string(typeId)}}});
            }
            if (!jsonCodecResult) {
                return core::Result<project::Module>::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "Failed to encode module parameters to JSON for mastering chain serialization",
                    {{"typeId", std::string(typeId)}}});
            }

            const auto& inst = instanceResult.value()->get();
            const auto& desc = descResult.value()->get();

            if (inst.module_type_id() != typeId) {
                return core::Result<project::Module>::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "Module instance type ID mismatch during mastering chain serialization",
                    {{"typeId", std::string(typeId)}}});
            }

            if (desc.type_id() != typeId) {
                return core::Result<project::Module>::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "Module descriptor type ID mismatch during mastering chain serialization",
                    {{"typeId", std::string(typeId)}}});
            }

            const std::string_view expectedAlgorithmVersion =
                typeId == "rgsml.dsp.streaming-true-peak-limiter"
                    ? std::string_view{"0.1.0-preview"} : std::string_view{"1.0.0"};
            if (!desc.algorithm_version() || *desc.algorithm_version() != expectedAlgorithmVersion) {
                return core::Result<project::Module>::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "Module descriptor algorithmVersion is inconsistent with the frozen v1 contract",
                    {{"typeId", std::string(typeId)}}});
            }

            if (!desc.parameter_schema_id() || *desc.parameter_schema_id() != expectedParameterSchemaId) {
                return core::Result<project::Module>::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "Module descriptor parameterSchemaId is inconsistent with the frozen v1 contract",
                    {{"typeId", std::string(typeId)}}});
            }

            auto opaqueJson = project::OpaqueJsonValue::parse(*jsonCodecResult.value());
            if (!opaqueJson) {
                return core::Result<project::Module>::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "Failed to parse OpaqueJsonValue for module parameters during mastering chain serialization",
                    {{"typeId", std::string(typeId)}}});
            }

            project::Module m;
            m.instanceId = inst.instance_id().uuid();
            m.typeId = std::string(inst.module_type_id());
            m.enabled = inst.enabled();
            m.userBypass = inst.user_bypass();
            m.controllerSuspended = inst.controller_suspended();
            m.domainSuspended = inst.domain_suspended();
            m.provenance = provenance_to_string(inst.provenance());
            m.owner = owner_to_string(inst.owner());
            m.linkState = link_state_to_string(inst.link_state());
            m.semanticNodeId = inst.semantic_node_id();
            m.algorithmVersion = std::string(*desc.algorithm_version());
            m.parameterSchemaId = std::string(*desc.parameter_schema_id());
            m.parameters = std::move(*opaqueJson.value());
            return core::Result<project::Module>::success(std::move(m));
        };

        // Module 0: Input Gain
        auto m0 = serialize_module("rgsml.dsp.gain",
            "rgsml.dsp.gain.parameters/1.0.0",
            masteringChainState_->gain_instance(),
            dsp::encode_gain_parameters_json(masteringChainState_->gain_parameters()));
        if (!m0) return core::Result<project::ProjectSnapshot>::failure(*m0.error());
        masteringChain.modules.push_back(std::move(*m0.value()));

        // Module 1: Parametric EQ
        auto m1 = serialize_module("rgsml.dsp.parametric-eq",
            "rgsml.dsp.parametric-eq.parameters/1.0.0",
            masteringChainState_->eq_instance(),
            dsp::encode_parametric_eq_parameters_json(masteringChainState_->parametric_eq_parameters()));
        if (!m1) return core::Result<project::ProjectSnapshot>::failure(*m1.error());
        masteringChain.modules.push_back(std::move(*m1.value()));

        // A complete typed pair is serialized in its actual MANUAL order.
        const auto& dynamicId=masteringChainState_->dynamic_eq_instance_id();
        const auto& dynamicParams=masteringChainState_->dynamic_eq_parameters();
        const auto& transientId=masteringChainState_->transient_shaper_instance_id();
        const auto& transientParams=masteringChainState_->transient_shaper_parameters();
        const bool extended=dynamicId.has_value();
        if(extended!=dynamicParams.has_value() || extended!=transientId.has_value()
            || extended!=transientParams.has_value())
            return core::Result<project::ProjectSnapshot>::failure(core::Error{
                core::ErrorCode::InvalidState,"MANUAL_MATCH_DSP_INCOMPLETE_SAVE"});
        if(extended) {
            auto dynamic=serialize_module("rgsml.dsp.dynamic-eq",
                "rgsml.dsp.dynamic-eq.parameters/1.0.0",
                masteringChainState_->chain().find_instance(*dynamicId),
                dsp::encode_dynamic_eq_parameters_json(*dynamicParams));
            if(!dynamic)return core::Result<project::ProjectSnapshot>::failure(*dynamic.error());
            auto transient=serialize_module("rgsml.dsp.transient-shaper",
                "rgsml.dsp.transient-shaper.parameters/1.0.0",
                masteringChainState_->chain().find_instance(*transientId),
                dsp::encode_transient_shaper_parameters_json(*transientParams));
            if(!transient)return core::Result<project::ProjectSnapshot>::failure(*transient.error());
            masteringChain.modules.push_back(std::move(*dynamic.value()));
            masteringChain.modules.push_back(std::move(*transient.value()));
        }

        // Compressor
        auto m2 = serialize_module("rgsml.dsp.compressor",
            "rgsml.dsp.compressor.parameters/1.0.0",
            masteringChainState_->compressor_instance(),
            dsp::encode_compressor_parameters_json(masteringChainState_->compressor_parameters()));
        if (!m2) return core::Result<project::ProjectSnapshot>::failure(*m2.error());
        masteringChain.modules.push_back(std::move(*m2.value()));

        // Only an explicitly opted-in fourth M/S module is serialized.
        // Preserve all six canonical parameters and the instance's bypass
        // and structural flags; historical 3-node projects remain 3-node.
        if (masteringChainState_->stereo_ms_instance_id()) {
            const auto& params = masteringChainState_->stereo_ms_parameters();
            if (!params) {
                return core::Result<project::ProjectSnapshot>::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "Stereo/M-S instance exists without typed parameters"});
            }
            auto m3 = serialize_module("rgsml.dsp.stereo-ms",
                "rgsml.dsp.stereo-ms.parameters/1.0.0",
                masteringChainState_->stereo_ms_instance(),
                dsp::encode_stereo_ms_parameters_json(*params));
            if (!m3) return core::Result<project::ProjectSnapshot>::failure(
                *m3.error());
            masteringChain.modules.push_back(std::move(*m3.value()));
        } else if (masteringChainState_->stereo_ms_parameters()) {
            return core::Result<project::ProjectSnapshot>::failure(core::Error{
                core::ErrorCode::InvalidState,
                "Stereo/M-S parameters exist without a module instance"});
        }

        // Persist P2 only when the actual Product chain has the fifth
        // typed, executable MANUAL instance. Never serialize a decorative
        // editor or silently drop opt-in DSP audio from a saved Project.
        const auto& p2Id = masteringChainState_->streaming_p2_preview_instance_id();
        const auto& p2Params = masteringChainState_->streaming_p2_preview_parameters();
        if (p2Id || p2Params) {
            if (!p2Id || !p2Params ||
                !masteringChainState_->stereo_ms_instance_id() ||
                masteringChainState_->module_count()!=(extended?7U:5U)) {
                return core::Result<project::ProjectSnapshot>::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "Streaming P2 project state requires complete fifth-module identity"});
            }
            auto p2 = serialize_module("rgsml.dsp.streaming-true-peak-limiter",
                "rgsml.dsp.streaming-true-peak-limiter.parameters/0.1.0",
                masteringChainState_->chain().find_instance(*p2Id),
                dsp::encode_streaming_true_peak_limiter_parameters_json(*p2Params));
            if (!p2) return core::Result<project::ProjectSnapshot>::failure(
                *p2.error());
            masteringChain.modules.push_back(std::move(*p2.value()));
        }

        if(masteringChain.modules.size()!=masteringChainState_->module_count())
            return core::Result<project::ProjectSnapshot>::failure(core::Error{
                core::ErrorCode::InvalidState,"MANUAL_DSP_SAVE_TOPOLOGY_MISMATCH"});
        auto chainIt = std::find_if(doc.chains.begin(), doc.chains.end(),
            [chainId](const project::Chain& c) { return c.chainId == chainId; });
        if (chainIt == doc.chains.end()) {
            doc.chains.push_back(std::move(masteringChain));
        } else {
            *chainIt = std::move(masteringChain);
        }
    }
    // Persistent Project/Render preference. Preserve independently unknown
    // output keys; do not convert current playback DEVICE into master rate.
    if (masterOutputRateState_) {
        if (masterOutputRateState_->has_supported_master_rate()) {
            auto updated = project::MasterOutputRatePreference::with_target(
                doc.output, masterOutputRateState_->master_rate_hz());
            if (!updated)
                return core::Result<project::ProjectSnapshot>::failure(*updated.error());
            doc.output = std::move(*updated.value());
        } else {
            auto persisted = project::MasterOutputRatePreference::read(doc.output);
            if (!persisted || persisted.value()->has_value())
                return core::Result<project::ProjectSnapshot>::failure(
                    invalid("output.targetMasterOutputRateHz/source"));
        }
    }
    // User-chosen M18 WAV settings are a canonical Project OUTPUT
    // preference, not proof of a completed WAV delivery. Legacy projects
    // without an explicit choice remain unchanged on ordinary SaveAs.
    if(m18OutputPreference_) {
        auto updated=project::M18MasterWavPreferenceCodec::with_choice(
            doc.output,*m18OutputPreference_);
        if(!updated)
            return core::Result<project::ProjectSnapshot>::failure(
                *updated.error());
        doc.output=std::move(*updated.value());
    }
    return project::ProjectSnapshot::create(std::move(doc), std::move(optional));
}

core::Status ProjectSessionViewModel::admit_m18_project_mutation(
    const char* operation) const
{
    if(!m18ProjectMutationAdmission_)return core::Status::success();
    if(QThread::currentThread()!=thread())
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_PROJECT_MUTATION_REQUIRES_QT_OWNER_THREAD"});
    try{return m18ProjectMutationAdmission_(operation);}
    catch(...){return core::Status::failure(core::Error{
        core::ErrorCode::InvalidState,
        "M18_PROJECT_MUTATION_ADMISSION_THROWN"});}
}

core::Result<std::shared_ptr<const void>>
ProjectSessionViewModel::begin_m18_project_save() const
{
    using R=core::Result<std::shared_ptr<const void>>;
    // The future Product must keep a real exclusive epoch through the
    // entire .rgsml publication, not just grant a one-shot status.
    if(m18ProjectSaveBatchBegin_) {
        if(m18ProjectSaveAdmission_||m18ProjectMutationAdmission_)
            return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_PROJECT_SAVE_BATCH_ADMISSION_AMBIGUOUS"});
        if(QThread::currentThread()!=thread())
            return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_PROJECT_SAVE_BATCH_WRONG_OWNER_THREAD"});
        try {
            auto lease=m18ProjectSaveBatchBegin_();
            if(!lease)return lease;
            if(!*lease.value())
                return R::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_PROJECT_SAVE_BATCH_NO_EXCLUSIVE_LEASE"});
            return lease;
        }catch(...){
            return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_PROJECT_SAVE_BATCH_ADMISSION_THROWN"});
        }
    }
    // Installing a Product batch for Open forbids status-only SaveAs:
    // the worker might commit after admission but before SaveAs finishes.
    if(m18ProjectOpenBatchBegin_)
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_PROJECT_SAVE_REQUIRES_EXCLUSIVE_BATCH"});
    if(m18ProjectSaveAdmission_ &&m18ProjectMutationAdmission_)
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_PROJECT_SAVE_ADMISSION_MODES_AMBIGUOUS"});
    const auto admit=[&]()->core::Status {
        if(!m18ProjectSaveAdmission_)
            return admit_m18_project_mutation("save_project_as");
        if(QThread::currentThread()!=thread())
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_PROJECT_SAVE_REQUIRES_QT_OWNER_THREAD"});
        try{return m18ProjectSaveAdmission_("save_project_as");}
        catch(...){return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_PROJECT_SAVE_ADMISSION_THROWN"});}
    }();
    if(!admit)return R::failure(*admit.error());
    // No M18 Product registration is installed today. Legacy M17 stays
    // unchanged; this empty token is never permission for async export.
    return R::success(std::shared_ptr<const void>{});
}

void ProjectSessionViewModel::openProject(const QUrl& selectedFile)
{
    // With any M18 Product policy, even invalid-file diagnostics must
    // remain on the owning Qt thread. Never probe disk or emit errors
    // from a foreign worker before an eventual epoch admission.
    const bool m18Guarded=static_cast<bool>(m18ProjectOpenBatchBegin_
        ||m18ProjectMutationAdmission_||m18ProjectSaveAdmission_
        ||m18ProjectSaveBatchBegin_);
    if(m18Guarded &&QThread::currentThread()!=thread())return;
    if(m18Guarded &&m18ProjectOperationActive_)return;
    M18ScopedProjectOperation singleWriter{
        m18Guarded?&m18ProjectOperationActive_:nullptr};
    Q_UNUSED(singleWriter);
    // A Product that installed only the SaveAs epoch cannot permit an
    // unguarded Open Project to retire SOURCE/MASTER during M18 export.
    if((m18ProjectSaveAdmission_||m18ProjectSaveBatchBegin_)
       &&!m18ProjectOpenBatchBegin_&&!m18ProjectMutationAdmission_) {
        publish_error(core::Error{
            core::ErrorCode::InvalidState,
            "M18_PROJECT_OPEN_REQUIRES_OWN_MUTATION_ADMISSION"});
        return;
    }
    auto reference = read_reference(selectedFile);
    if (!reference) { publish_error(*reference.error()); return; }
    auto reader = platform::windows::WindowsResourceReader::open_read_only(*reference.value());
    if (!reader) { publish_error(*reader.error()); return; }
    auto opened = project::ProjectRepository::open(std::move(*reader.value()));
    if (!opened) { publish_error(*opened.error()); return; }
    const auto& doc = opened.value()->document();
    // A saved REF_LINKED plan may be reopened only with a live Product
    // owner, a physical exclusive Project batch and the real PCM Preview
    // controller. All native DSP schemas and factories are preflighted
    // before stopping transport or touching SOURCE/GOLD/MANUAL.
    const bool linkedRequested=
        l3_automatch::m12_frozen::
        assess_current_product_ref_linked_open_safety(doc)!=
        l3_automatch::m12_frozen::ProjectOpenLinkedSafety::
            ordinary_project_without_ref_linked;
    const auto matchMetadata=QJsonDocument::fromJson(
        QByteArray::fromStdString(doc.referenceMatch.canonical_utf8()));
    const bool transferRequested=matchMetadata.isObject()&&
        matchMetadata.object().contains(QStringLiteral("manualTransferCandidate"));
    if(linkedRequested&&(!m18ProjectOpenBatchBegin_||
        !m18ProjectSaveBatchBegin_||!previewController_||
        !masteringChainState_)) {
        publish_error(core::Error{
            core::ErrorCode::InvalidState,
            "L3_M12_PROJECT_OPEN_REQUIRES_EXCLUSIVE_PRODUCT_REPLAY"});
        return;
    }
    // Fail-closed preflight before transport/Source/chain mutation.
    auto rate = project::MasterOutputRatePreference::read(doc.output);
    if (!rate) { publish_error(*rate.error()); return; }
    // A malformed or partially persisted M18 format/seed is a Project
    // preflight error, BEFORE any source, playback or DSP mutation.
    auto m18Choice=project::M18MasterWavPreferenceCodec::read(doc.output);
    if(!m18Choice) {publish_error(*m18Choice.error());return;}
    auto sourceRef = external_reference(doc, doc.sourceResourceId);
    if (!sourceRef) { publish_error(*sourceRef.error()); return; }
    auto sourceCandidate = probe(*sourceRef.value());
    if (!sourceCandidate) { publish_error(*sourceCandidate.error()); return; }
    if (rate.value()->has_value() &&
        !MasterOutputRateState::supported(
            sourceCandidate.value()->wav_info().audio_format().sample_rate().value())) {
        publish_error(invalid("output.targetMasterOutputRateHz/source")); return;
    }
    std::optional<audio::SourceResource> goldCandidate;
    std::optional<core::Uuid> goldResourceId;
    if (doc.references.activeReferenceId) {
        const auto ref = std::find_if(doc.references.items.begin(), doc.references.items.end(),
            [&](const project::Reference& item) {
                return item.referenceId == *doc.references.activeReferenceId;
            });
        if (ref == doc.references.items.end()) { publish_error(invalid("references")); return; }
        goldResourceId = ref->resourceId;
        auto goldRef = external_reference(doc, *goldResourceId);
        if (!goldRef) { publish_error(*goldRef.error()); return; }
        auto distinct = platform::windows::same_underlying_local_file(
            *sourceRef.value(), *goldRef.value());
        if (!distinct) { publish_error(*distinct.error()); return; }
        if (*distinct.value()) { publish_error(invalid("sameFileSourceGold")); return; }
        auto candidate = probe(*goldRef.value());
        if (!candidate) { publish_error(*candidate.error()); return; }
        goldCandidate.emplace(std::move(*candidate.value()));
    }
    std::optional<core::FrameRange> regionCandidate;
    std::optional<core::FrameRange> referenceRegionCandidate;
    bool sourceLoop=false;
    bool referenceLoop=false;
    std::optional<core::Uuid> persistedSourceRegionId;
    std::optional<core::Uuid> persistedReferenceRegionId;
    // Resolve BOTH frame domains before the first destructive source or
    // Product epoch mutation. Wrong clock, reference or missing controller
    // rejects the entire open without touching the prior session.
    for (const auto& persisted : doc.timeSelections.auditionRegions) {
        const auto range = core::FrameRange::create(
            core::FrameIndex{persisted.startFrame},
            core::FrameIndex{persisted.endFrameExclusive});
        if(!range) {publish_error(*range.error());return;}
        if(persisted.frameDomain=="SOURCE") {
            if(persisted.resourceId!=doc.sourceResourceId
               ||persisted.endFrameExclusive>
                   sourceCandidate.value()->wav_info().frame_count().value()) {
                publish_error(invalid("timeSelections.auditionRegions"));return;
            }
            regionCandidate=*range.value();
            persistedSourceRegionId=persisted.regionId;
            sourceLoop=persisted.loopEnabled;
        } else if(persisted.frameDomain=="REFERENCE") {
            if(!referenceRegion_ ||!goldCandidate ||!goldResourceId
               ||persisted.resourceId!=*goldResourceId
               ||persisted.endFrameExclusive>
                   goldCandidate->wav_info().frame_count().value()) {
                publish_error(invalid("timeSelections.auditionRegions"));return;
            }
            referenceRegionCandidate=*range.value();
            persistedReferenceRegionId=persisted.regionId;
            referenceLoop=persisted.loopEnabled;
        } else {publish_error(invalid("timeSelections.auditionRegions"));return;}
    }

    auto registryRes = dsp::ModuleRegistry::create_dsp_package_v1();
    if (!registryRes) { publish_error(*registryRes.error()); return; }
    // A saved P2 project can be opened only in an explicitly opted-in I2
    // session. The original v1 registry never silently admits P2.
    const dsp::ModuleRegistry* restoreRegistry = &*registryRes.value();
    if (masteringChainState_ &&
        masteringChainState_->streaming_p2_preview_instance_id()) {
        const auto optInRegistry = masteringChainState_->registry_snapshot();
        if (!optInRegistry ||
            !optInRegistry->has_factory("rgsml.dsp.streaming-true-peak-limiter")) {
            publish_error(core::Error{
                core::ErrorCode::InvalidState,
                "P2 restore requires the active opt-in Product registry"});
            return;
        }
        restoreRegistry = optInRegistry.get();
    }
    // Admit C01/C00 only through their canonical typed factories. Keep
    // the registry for a saved P2 graph intact and never mutate it in place.
    // Preserve explicit opt-in factory capability across a legacy open,
    // without adding nodes to the historical persisted chain.
    const auto manualOptInRegistry=masteringChainState_
        ?masteringChainState_->registry_snapshot():nullptr;
    if(manualOptInRegistry &&
       manualOptInRegistry->has_factory("rgsml.dsp.dynamic-eq") &&
       manualOptInRegistry->has_factory("rgsml.dsp.transient-shaper"))
        restoreRegistry=manualOptInRegistry.get();
    std::optional<dsp::ModuleRegistry> extendedRegistry;
    const bool needsMatchFactories=std::any_of(doc.chains.begin(),doc.chains.end(),
        [](const project::Chain& chain) {
            return chain.stage=="MASTER" && chain.segment=="MANUAL" &&
                std::any_of(chain.modules.begin(),chain.modules.end(),
                    [](const project::Module& m) {
                        return m.typeId=="rgsml.dsp.dynamic-eq" || m.typeId=="rgsml.dsp.transient-shaper";
                    });
        });
    if(needsMatchFactories) {
        // Until the Product installs both real manual editors, do not open
        // a project whose audible processors would be invisible in the UI.
        const auto currentRegistry=masteringChainState_
            ?masteringChainState_->registry_snapshot():nullptr;
        if(!currentRegistry || !currentRegistry->has_factory("rgsml.dsp.dynamic-eq")
           || !currentRegistry->has_factory("rgsml.dsp.transient-shaper")) {
            publish_error(core::Error{core::ErrorCode::UnsupportedOperation,
                "Manual Dynamic EQ / Transient Shaper requires the enabled Product integration"});
            return;
        }
        extendedRegistry=*restoreRegistry;
        if(!extendedRegistry->has_factory("rgsml.dsp.dynamic-eq")) {
            auto next=dsp::ModuleRegistry::with_dynamic_eq_offline(std::move(*extendedRegistry));
            if(!next){publish_error(*next.error());return;}
            extendedRegistry=std::move(*next.value());
        }
        if(!extendedRegistry->has_factory("rgsml.dsp.transient-shaper")) {
            auto next=dsp::ModuleRegistry::with_transient_shaper_offline(std::move(*extendedRegistry));
            if(!next){publish_error(*next.error());return;}
            extendedRegistry=std::move(*next.value());
        }
        restoreRegistry=&*extendedRegistry;
    }
    auto chainCandidate = validate_mastering_chain(doc, *restoreRegistry, uuidFactory_);
    if (!chainCandidate) { publish_error(*chainCandidate.error()); return; }

    // Source/Gold/MASTER/chain/project identity all change below this
    // boundary. Product epoch admission must happen BEFORE stopping any
    // current sink or adopting the newly opened Project.
    // If a future Product supplies a genuine cross-controller batch,
    // keep its RAII owner through ALL the source/rate/Gold/chain writes,
    // including early error returns. A worker must not acquire commit
    // authority halfway through the Project replacement.
    std::shared_ptr<const void> projectMutationBatch;
    if(m18ProjectOpenBatchBegin_) {
        if(m18ProjectMutationAdmission_
           ||QThread::currentThread()!=thread()) {
            publish_error(core::Error{
                core::ErrorCode::InvalidState,
                "M18_PROJECT_BATCH_AMBIGUOUS_OR_WRONG_OWNER_THREAD"});
            return;
        }
        try {
            auto token=m18ProjectOpenBatchBegin_();
            if(!token) {publish_error(*token.error());return;}
            if(!*token.value()) {
                publish_error(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_PROJECT_BATCH_MISSING_EXCLUSIVE_LEASE"});
                return;
            }
            projectMutationBatch=std::move(*token.value());
        } catch(...) {
            publish_error(core::Error{
                core::ErrorCode::InvalidState,
                "M18_PROJECT_BATCH_ADMISSION_THROWN"});
            return;
        }
    } else {
        const auto admission=admit_m18_project_mutation("open_project");
        if(!admission) {publish_error(*admission.error());return;}
    }
    Q_UNUSED(projectMutationBatch); // RAII lifetime extends to function exit.
    std::shared_ptr<const
        l3_automatch::m12_frozen::RestoredM12NativeReplayPlan>
        reopeningReplay;
    std::optional<l3_automatch::ManualMatchTransferReopenPreflight> reopeningTransfer;
    if(linkedRequested) {
        if(!projectMutationBatch||!chainCandidate.value()->chainState) {
            publish_error(core::Error{core::ErrorCode::InvalidState,
                "L3_M12_PROJECT_OPEN_NO_REAL_OWNER_OR_MANUAL_CHAIN"});
            return;
        }
        if(transferRequested) {
            auto recovered=l3_automatch::recover_manual_match_transfer_before(*opened.value());
            if(!recovered){publish_error(*recovered.error());return;}
            auto prior=validate_mastering_chain(recovered.value()->document(),*restoreRegistry,uuidFactory_);
            if(!prior){publish_error(*prior.error());return;}
            if(!prior.value()->chainState){
                publish_error(core::Error{core::ErrorCode::InvalidState,"MANUAL_MATCH_REOPEN_PRIOR_CHAIN_ABSENT"});return;
            }
            auto preflight=l3_automatch::prepare_manual_match_transfer_reopen(
                *opened.value(),*prior.value()->chainState,*chainCandidate.value()->chainState);
            if(!preflight){publish_error(*preflight.error());return;}
            reopeningTransfer.emplace(std::move(*preflight.value()));
        } else {
            auto preflight=l3_automatch::m12_frozen::
                prepare_m12_product_reopen_preflight(
                    doc,*chainCandidate.value()->chainState,true);
            if(!preflight) {publish_error(*preflight.error());return;}
            reopeningReplay=std::move(preflight.value()->replay);
        }
    }
    isCommittingProjectOpen_ = true;
    auto stopped = playback_->stop_and_clear();
    if (!stopped) {
        isCommittingProjectOpen_ = false;
        publish_error(*stopped.error());
        return;
    }
    // An already accepted calculated master belongs to the PREVIOUS
    // Project identity. An ordinary Project pointing to the SAME SOURCE
    // and REFERENCE still cannot retain its older REF_LINKED ticket.
    // Revoke only AFTER transport-stop success and the M18 Project lease;
    // rejected preflights and failed stop requests preserve the prior owner.
    if(previewController_ &&
       (previewController_->has_private_m12_product_preview_ticket()||
        previewController_->private_manual_transfer_preview_ticket()))
        previewController_->set_private_m12_product_preview_ticket({});

    const auto adoptedSource=
        source_->adopt_probed_source(std::move(*sourceCandidate.value()));
    if(!adoptedSource) {
        isCommittingProjectOpen_=false;
        publish_error(*adoptedSource.error());
        return;
    }
    if (masterOutputRateState_) {
        // The production SOURCE callback reset the authority on commit.
        // Standalone ProjectSession clients may have no such callback.
        if (masterOutputRateState_->source_rate_hz()
            !=source_->sample_rate_hz()) {
            // The old void SOURCE-rate setter deliberately fails closed
            // once M18 admission is installed. Project Open already owns
            // the shared exclusive mutation batch, so use the explicit
            // status-bearing child API, never bypass the Product epoch.
            const bool committed=
                masterOutputRateState_->m18_rate_mutation_admission_installed()
                ?masterOutputRateState_->try_source_committed_for_m18(
                    source_->sample_rate_hz())
                :(masterOutputRateState_->source_committed(
                    source_->sample_rate_hz()),true);
            if(!committed) {
                isCommittingProjectOpen_=false;
                publish_error(core::Error{
                    core::ErrorCode::InvalidState,
                    "PROJECT_SOURCE_RATE_EPOCH_MUTATION_REFUSED"});
                return;
            }
        }
        if (rate.value()->has_value()) {
            const bool accepted = masterOutputRateState_->choose_master_rate(
                rate.value()->value());
            // M18 Product epoch admission is allowed to refuse the
            // requested MASTER rate while a worker owns a commit lease.
            // Never assert/crash in a debug build or silently continue a
            // partially restored Project in a release build. The outer
            // Project batch remains consent-revoked on every error path.
            if(!accepted) {
                isCommittingProjectOpen_=false;
                publish_error(core::Error{
                    core::ErrorCode::InvalidState,
                    "PROJECT_MASTER_RATE_MUTATION_REFUSED"});
                return;
            }
        }
    }
    if (goldCandidate) {
        auto accepted = gold_->adopt_probed_gold(std::move(*goldCandidate));
        if (!accepted) {
            isCommittingProjectOpen_ = false;
            publish_error(*accepted.error());
            return;
        }
    } else if (gold_->has_gold()) {
        gold_->clearGold();
    }

    if (regionCandidate) {
        auto accepted = region_->set_region(*regionCandidate);
        if (!accepted) {
            isCommittingProjectOpen_ = false;
            publish_error(*accepted.error());
            return;
        }
        accepted=region_->restore_project_loop_preference(sourceLoop);
        if(!accepted) {
            isCommittingProjectOpen_=false;
            publish_error(*accepted.error());
            return;
        }
    }
    if(referenceRegionCandidate) {
        auto accepted=referenceRegion_->set_region(*referenceRegionCandidate);
        if(!accepted) {
            isCommittingProjectOpen_=false;
            publish_error(*accepted.error());
            return;
        }
        accepted=referenceRegion_->restore_project_loop_preference(referenceLoop);
        if(!accepted) {
            isCommittingProjectOpen_=false;
            publish_error(*accepted.error());
            return;
        }
    }

    if (masteringChainState_ && chainCandidate.value()->chainState) {
        // This Project open was pre-admitted before all destructive changes.
        // Retain the Product's M18 epoch gate; ordinary assignment would
        // silently erase it with the newly deserialized chain snapshot.
        masteringChainState_->adopt_chain_after_authorized_project_open(
            std::move(*chainCandidate.value()->chainState));
    }

    if (adapterModel_) {
        adapterModel_->refreshFromAuthority();
    } else {
        if (gainViewModel_) {
            gainViewModel_->refreshFromAuthority();
        }
        if (eqViewModel_) {
            eqViewModel_->refreshFromAuthority();
        }
    }
    if(linkedRequested) {
        // SOURCE adoption has synchronously published its new PREPARED
        // realization through the Product's real source-committed path.
        // Do not reuse PCM captured before the Project identity switch.
        const auto& provider=previewController_->snapshot_provider();
        const auto currentPrepared=provider?provider():
            std::shared_ptr<const render::RenderResult>{};
        if(reopeningTransfer) {
            auto ticket=l3_automatch::bind_manual_match_transfer_reopen(
                *reopeningTransfer,currentPrepared,m12ProductOwnershipEpoch_+1U);
            if(!ticket||!previewController_->set_private_manual_transfer_preview_ticket(
                ticket?std::make_shared<const l3_automatch::ManualMatchTransferPreviewTicket>(
                    std::move(*ticket.value())):nullptr)) {
                previewController_->set_private_m12_product_preview_ticket(
                    std::make_shared<const l3_automatch::m12_frozen::M12ImmutableProductPreviewTicket>());
                isCommittingProjectOpen_=false;
                publish_error(core::Error{core::ErrorCode::InvalidState,
                    "MANUAL_MATCH_REOPEN_PREPARED_BIND_FAILED"});
                return;
            }
        } else {
        auto ticket=currentPrepared
            ?l3_automatch::m12_frozen::
                prepare_m12_immutable_product_preview_ticket(
                    reopeningReplay,currentPrepared,
                    masteringChainState_->chain(),
                    masteringChainState_->execution_bindings())
            :core::Result<
                l3_automatch::m12_frozen::M12ImmutableProductPreviewTicket>::
                failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "L3_M12_PROJECT_OPEN_PREPARED_NOT_OWNED"});
        if(!ticket) {
            // A completed source handoff must NEVER fall back to a
            // MANUAL-only PROCESSED audition. A deliberately invalid
            // private linked ticket holds the replay path fail-closed
            // until a later valid Project transaction replaces it.
            previewController_->set_private_m12_product_preview_ticket(
                std::make_shared<const l3_automatch::m12_frozen::
                    M12ImmutableProductPreviewTicket>());
            isCommittingProjectOpen_=false;
            publish_error(*ticket.error());
            return;
        }
        previewController_->set_private_m12_product_preview_ticket(
            std::make_shared<const l3_automatch::m12_frozen::
                M12ImmutableProductPreviewTicket>(
                    std::move(*ticket.value())));
        }
    }
    if (previewController_) {
        previewController_->request_preview();
    }

    isCommittingProjectOpen_ = false;

    const auto persistedProjectId = doc.projectId;
    const auto persistedSourceId = doc.sourceResourceId;
    const auto persistedReferenceId = doc.references.activeReferenceId;
    // Exact IDs are captured by frame domain, not vector order.
    const auto persistedRegionId=persistedSourceRegionId;
    const auto persistedName = doc.displayName;
    const bool isDegraded = reopeningTransfer
        ?has_unsupported_semantics_after_native_m12_reopen(reopeningTransfer->project.before.document())
        :linkedRequested?has_unsupported_semantics_after_native_m12_reopen(doc)
        :has_future_semantics(doc);
    // Publish the validated WAV preference only after the entire
    // Project/source/chain commit succeeded under the M18 physical lease.
    m18OutputPreference_=std::move(*m18Choice.value());
    opened_.emplace(std::move(*opened.value()));
    // The previous controller's Undo/Redo must never be applicable
    // across a different Project Open, even with identical SOURCE WAV.
    m12UndoHistory_.clear();
    m12RedoHistory_.clear();
    m12ExactReplayReadback_.reset();
    m12RetiredReplayIdentity_.reset();
    sessionChainMaterialized_ = chainCandidate.value()->isMaterialized;
    projectId_ = persistedProjectId;
    sourceId_ = persistedSourceId;
    referenceId_ = persistedReferenceId;
    goldId_ = goldResourceId;
    regionId_ = persistedRegionId;
    referenceRegionId_=persistedReferenceRegionId;
    clearedRegionId_.reset();
    lastSource_ = source_->source_resource()->reference();
    lastGold_ = gold_->gold_resource() ?
        std::optional{gold_->gold_resource()->reference()} : std::nullopt;
    degraded_ = isDegraded;
    projectDisplayName_ = QString::fromUtf8(persistedName.data(),
        static_cast<qsizetype>(persistedName.size()));
    if(reopeningTransfer) {
        const auto readback=current_snapshot(QString::fromStdString(persistedName));
        const auto ticket=previewController_->private_manual_transfer_preview_ticket();
        if(!readback||!ticket||!ticket->matches_project(*readback.value())||
           !ticket->matches_project(*opened_)) {
            previewController_->set_private_m12_product_preview_ticket(
                std::make_shared<const l3_automatch::m12_frozen::M12ImmutableProductPreviewTicket>());
            publish_error(core::Error{core::ErrorCode::InvalidState,
                "MANUAL_MATCH_REOPEN_READBACK_MISMATCH"});
            return;
        }
        statusText_=QStringLiteral("Project reopened: MANUAL TRANSFER / NOT_VERIFIED.");
    } else if(linkedRequested) {
        // This is a REAL owner projection after Source, Gold, MANUAL and
        // the immutable REF replay ticket were installed. Reopening a
        // persisted MATERIALIZED plan creates NO NEW receipt.
        const auto readback=current_snapshot(
            QString::fromStdString(persistedName));
        if(!readback||
           !l3_automatch::m12_frozen::m12_exact_project_snapshot_equal(
               *readback.value(),*opened_)||
           !previewController_||
           !previewController_->has_private_m12_product_preview_ticket()) {
            m12ExactReplayReadback_.reset();
            previewController_->set_private_m12_product_preview_ticket(
                std::make_shared<const l3_automatch::m12_frozen::
                    M12ImmutableProductPreviewTicket>());
            statusText_=QStringLiteral(
                "REF_LINKED reopen quarantined: Product readback mismatch.");
            publish_error(core::Error{core::ErrorCode::InvalidState,
                "L3_M12_PROJECT_OPEN_REF_LINKED_READBACK_MISMATCH"});
            return;
        }
        m12ExactReplayReadback_=std::move(*readback.value());
        statusText_=QStringLiteral(
            "Project reopened: REF_LINKED MATERIALIZED / NOT_VERIFIED.");
    } else {
        statusText_ = degraded_ ?QStringLiteral(
            "Project opened in degraded mode; unsupported processing is not active.")
            :QStringLiteral("Project opened.");
    }
    ++m12ProductOwnershipEpoch_;
    clear_error();
}

core::Result<project::ProjectSnapshot>
ProjectSessionViewModel::capture_private_m12_search_baseline(bool require_owned_replacement)
{
    using R=core::Result<project::ProjectSnapshot>;
    const auto fail=[](const char* code){
        return R::failure(core::Error{core::ErrorCode::InvalidState,code});
    };
    if(QThread::currentThread()!=thread()||m18ProjectOperationActive_||
       isCommittingProjectOpen_||degraded_||
       !m18ProjectOpenBatchBegin_||!m18ProjectSaveBatchBegin_||
       (opened_&&!sessionChainMaterialized_)||
       !masteringChainState_||!previewController_||
       !source_||!source_->source_resource()||!gold_||!gold_->gold_resource())
        return fail("L3_M12_BASELINE_NO_CURRENT_PRODUCT_OWNER");
    M18ScopedProjectOperation operation{&m18ProjectOperationActive_};
    const auto lease=begin_m18_project_save();
    if(!lease||!*lease.value())return fail("L3_M12_BASELINE_EPOCH_UNAVAILABLE");
    const auto heldLease=*lease.value();
    Q_UNUSED(heldLease);
    auto stem=opened_?QString::fromStdString(opened_->document().displayName):
        QFileInfo(source_->display_name()).completeBaseName();
    if(stem.isEmpty())stem=QStringLiteral("Untitled");
    auto live=current_snapshot(stem);
    if(!live||!require_owned_replacement)return live;
    namespace m=l3_automatch::m12_frozen;
    const auto linked=m::assess_current_product_ref_linked_open_safety(live.value()->document());
    if(linked==m::ProjectOpenLinkedSafety::ordinary_project_without_ref_linked)return live;
    // Exact live receipt OR an independently rehydrated Project readback
    // can be replaced. Reopen alone never creates a new receipt or history.
    const auto ticket=previewController_->private_m12_product_preview_ticket();
    const bool currentReceipt=!m12UndoHistory_.empty()&&m12UndoHistory_.back().receipt&&
        m12UndoHistory_.back().afterTicket==ticket&&
        m::m12_exact_project_snapshot_equal(m12UndoHistory_.back().after,*live.value());
    const bool exactReopen=m12UndoHistory_.empty()&&m12ExactReplayReadback_&&
        m::m12_exact_project_snapshot_equal(*m12ExactReplayReadback_,*live.value());
    if(opened_&&ticket&&ticket->replay&&(currentReceipt||exactReopen)&&
       m::m12_exact_project_snapshot_equal(*opened_,*live.value())&&
       m::m12_reopened_persisted_replay_matches_exact_ticket(live.value()->document(),*ticket->replay))
        return live;
    // Dependency replacement is not controller retirement. Under the
    // same physical M18 owner lease preserve the previous exact graph
    // as suspended/STALE, then admit a NEW search against the new Fixed.
    const auto retired=ticket&&ticket->replay?ticket->replay:m12RetiredReplayIdentity_;
    const auto& provider=previewController_->snapshot_provider();
    const auto prepared=provider?provider():nullptr;
    if(!opened_||!retired||!prepared)
        return fail("L3_M12_BASELINE_PRIOR_OWNER_NOT_CURRENT");
    auto rebound=m::prepare_m12_stale_rebind(*opened_,*live.value(),*retired,
        *masteringChainState_,prepared);
    if(!rebound)return R::failure(*rebound.error());
    const auto previous=*opened_;
    const auto& newTicket=rebound.value()->ticket;
    opened_.emplace(rebound.value()->suspended);
    previewController_->set_private_m12_product_preview_ticket(newTicket);
    auto readback=current_snapshot(stem);
    if(!readback||!m::m12_exact_project_snapshot_equal(*readback.value(),*opened_)||
       previewController_->private_m12_product_preview_ticket()!=newTicket||
       !provider||provider()!=prepared||
       masteringChainState_->chain().revision()!=newTicket->exact_manual_chain_revision||
       masteringChainState_->execution_bindings()!=newTicket->exact_manual_bindings) {
        opened_.emplace(previous);
        previewController_->set_private_m12_product_preview_ticket(ticket);
        return fail("L3_M12_STALE_REBIND_READBACK_ROLLED_BACK");
    }
    m12UndoHistory_.clear();m12RedoHistory_.clear();
    m12RetiredReplayIdentity_.reset();
    m12ExactReplayReadback_.emplace(*readback.value());
    ++m12ProductOwnershipEpoch_;
    statusText_=QStringLiteral("STALE MATCH / SUSPENDED");
    clear_error();
    return readback;
}

core::Result<l3_automatch::m12_frozen::M12ProductMaterializationReceipt>
ProjectSessionViewModel::apply_private_m12_current_native_search(
    const l3_automatch::NativeAutoMatchWorkerResult& worker,
    const l3_automatch::m12_frozen::AcceptedPlanAndOwner& accepted,
    const l3_automatch::m10_frozen::P2NativeProductWorkerScope& owners,
    const std::string& exactGeneration,
    const project::ProjectSnapshot& preSearchProject,
    core::Uuid linkedChainId,
    const l3_automatch::m12_frozen::M12ProjectTransactionExpected& expected)
{
    namespace m=l3_automatch::m12_frozen;
    using R=core::Result<m::M12ProductMaterializationReceipt>;
    if(QThread::currentThread()!=thread()||m18ProjectOperationActive_||
       isCommittingProjectOpen_||degraded_||
       !gold_||gold_->selection_revision()!=owners.current_owner.reference_selection_revision)
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "L3_M12_PRODUCT_SEARCH_HANDOFF_STALE"});
    auto draft=m::prepare_current_native_search_transaction(worker,accepted,owners,
        exactGeneration,preSearchProject,linkedChainId,expected);
    if(!draft)return R::failure(*draft.error());
    // Existing M12 exclusively leases, rechecks the real current Project
    // and PREPARED owner, publishes atomically, then independently reads back.
    return apply_private_m12_materialization(*draft.value(),
        owners.frozen_registry,owners.current_prepared);
}

core::Result<l3_automatch::m12_frozen::M12ProductMaterializationReceipt>
ProjectSessionViewModel::apply_private_m12_materialization(
    const l3_automatch::m12_frozen::M12PreparedProjectTransaction& draft,
    std::shared_ptr<const dsp::ModuleRegistry> executionRegistry,
    std::shared_ptr<const render::RenderResult> preparedOwner)
{
    namespace m=l3_automatch::m12_frozen;
    using R=core::Result<m::M12ProductMaterializationReceipt>;
    const auto fail=[](const char* code) {
        return R::failure(core::Error{core::ErrorCode::InvalidState,code});
    };
    // A caller-held "accepted" boolean is never an exclusive epoch.
    // Reuse the already installed real Product Open+Save single-writer
    // fanout. No physical lease => no model publication, no receipt.
    if(QThread::currentThread()!=thread()||
       m18ProjectOperationActive_||isCommittingProjectOpen_||
       !m18ProjectOpenBatchBegin_||!m18ProjectSaveBatchBegin_||
       degraded_||(opened_&&!sessionChainMaterialized_)||
       !draft.exact_precommit_project||
       !previewController_||!masteringChainState_||
       !source_||!source_->source_resource()||
       !gold_||!gold_->gold_resource()||
       !projectId_||!sourceId_||!referenceId_||
       !executionRegistry||!preparedOwner)
        return fail("L3_M12_PRODUCT_COMMIT_NO_EXCLUSIVE_CURRENT_OWNER");
    M18ScopedProjectOperation operation{&m18ProjectOperationActive_};
    const auto lease=begin_m18_project_save();
    if(!lease||!*lease.value())
        return fail("L3_M12_PRODUCT_COMMIT_EPOCH_UNAVAILABLE");
    // The physical Product M18 lease must stay alive through actual
    // memory installation, fresh readback, receipt and history adoption.
    const auto heldLease=*lease.value();
    Q_UNUSED(heldLease);
    const auto stem=QString::fromStdString(
        draft.exact_precommit_project->document().displayName);
    auto live=current_snapshot(stem);
    if(!live||
       live.value()->document().projectId!=*projectId_||
       live.value()->document().sourceResourceId!=*sourceId_||
       live.value()->document().references.activeReferenceId!=referenceId_||
       !draft.matches_exact_precommit(*live.value()))
        return fail("L3_M12_PRODUCT_COMMIT_PRECOMMIT_PROJECT_STALE");
    const auto& snapshotProvider=previewController_->snapshot_provider();
    if(!snapshotProvider||snapshotProvider()!=preparedOwner)
        return fail("L3_M12_PRODUCT_COMMIT_PREPARED_OWNER_CHANGED");
    const auto beforeTicket=
        previewController_->private_m12_product_preview_ticket();
    const bool beforeHasLinked=m::assess_current_product_ref_linked_open_safety(
        live.value()->document())!=
        m::ProjectOpenLinkedSafety::ordinary_project_without_ref_linked;
    if(beforeHasLinked) {
        // Existing live receipt or exact independently rehydrated replay;
        // arbitrary imported/foreign or modified REF graphs still fail.
        const bool currentReceipt=!m12UndoHistory_.empty()&&m12UndoHistory_.back().receipt&&
            m12UndoHistory_.back().afterTicket==beforeTicket&&
            m::m12_exact_project_snapshot_equal(m12UndoHistory_.back().after,*live.value());
        const bool exactReopen=m12UndoHistory_.empty()&&m12ExactReplayReadback_&&
            m::m12_exact_project_snapshot_equal(*m12ExactReplayReadback_,*live.value());
        if(!opened_||!m::m12_exact_project_snapshot_equal(*opened_,*live.value())||
           !beforeTicket||!beforeTicket->replay||
           (!currentReceipt&&!exactReopen)||
           !m::m12_reopened_persisted_replay_matches_exact_ticket(live.value()->document(),*beforeTicket->replay))
            return fail("L3_M12_PRODUCT_COMMIT_PRIOR_OWNER_NOT_CURRENT");
    } else if(beforeTicket)
        return fail("L3_M12_PRODUCT_COMMIT_ORPHANED_PREVIEW_OWNER");
    const auto bindings=masteringChainState_->execution_bindings();
    auto ticket=m::prepare_m12_product_prepublication_preview_ticket(
        *live.value(),draft,std::move(executionRegistry),
        masteringChainState_->chain_id(),masteringChainState_->chain(),
        bindings,preparedOwner,true);
    if(!ticket)return R::failure(*ticket.error());
    auto afterTicket=std::make_shared<const m::M12ImmutableProductPreviewTicket>(
        std::move(*ticket.value()));
    M12MaterializationHistoryEntry entry{
        *live.value(),draft.candidate_not_yet_committed,
        beforeTicket,afterTicket,std::nullopt};
    // Allocate history capacity before the one coherent publication.
    // A failed allocation may never leave half of REF in the Product.
    m12UndoHistory_.reserve(m12UndoHistory_.size()+1U);
    // Recheck the exact owner and source metadata IMMEDIATELY before
    // publication, not merely at the start of the review/preflight.
    auto second=current_snapshot(stem);
    if(!second||!m::m12_exact_project_snapshot_equal(
        *second.value(),entry.before)||
       previewController_->private_m12_product_preview_ticket()!=
           beforeTicket||
       !snapshotProvider||snapshotProvider()!=preparedOwner||
       masteringChainState_->execution_bindings()!=bindings)
        return fail("L3_M12_PRODUCT_COMMIT_RACED_BEFORE_PUBLICATION");

    // Preserve the exact previous in-memory carrier (including absence
    // for an unsaved session) until physical publication/readback passes.
    auto beforeOpened=opened_;
    const bool beforeMaterialized=sessionChainMaterialized_;
    // Atomic Qt owner state: one complete ProjectDocument replaces the
    // previous snapshot. No in-place REF node insertion or manual merge.
    opened_.emplace(entry.after);
    sessionChainMaterialized_=true;
    previewController_->set_private_m12_product_preview_ticket(afterTicket);
    // Read back via the ACTUAL Product projection. Merely inspecting the
    // caller's draft or the preview ticket would be false positive proof.
    auto readback=current_snapshot(stem);
    const auto audited=readback
        ?m::audit_exact_m12_postcommit_project_readback(
            entry.before,draft,*readback.value(),true)
        :m::M12PostCommitReadbackAudit{};
    if(!audited.exact_after_state_observed()||
       previewController_->private_m12_product_preview_ticket()!=
           afterTicket||
       !snapshotProvider||snapshotProvider()!=preparedOwner||
       !m::m12_exact_project_snapshot_equal(
           *opened_,entry.after)) {
        // Readback failure cannot emit a successful materialization.
        // Recover the entire prior Project and its exact preview owner.
        opened_=std::move(beforeOpened);
        sessionChainMaterialized_=beforeMaterialized;
        previewController_->set_private_m12_product_preview_ticket(
            beforeTicket);
        return fail("L3_M12_PRODUCT_COMMIT_READBACK_REJECTED_ROLLED_BACK");
    }
    const auto& adopted=opened_->document();
    if(!adopted.references.activeReferenceId) {
        opened_=std::move(beforeOpened);
        sessionChainMaterialized_=beforeMaterialized;
        previewController_->set_private_m12_product_preview_ticket(
            beforeTicket);
        return fail("L3_M12_PRODUCT_COMMIT_REFERENCE_LOST");
    }
    m::M12ProductMaterializationReceipt receipt{
        adopted.projectId,adopted.sourceResourceId,
        *adopted.references.activeReferenceId,draft.plan_id,
        draft.controller_id,draft.ref_linked_chain_id,
        draft.accepted_plan_checksum};
    entry.receipt.emplace(receipt);
    m12RedoHistory_.clear();
    m12UndoHistory_.push_back(std::move(entry));
    ++m12ProductOwnershipEpoch_;
    statusText_=QStringLiteral("MATERIALIZED / NOT_VERIFIED");
    clear_error();
    return R::success(std::move(receipt));
}

core::Result<l3_automatch::m13_frozen::
    M13ImmutableProductFreshProbeRequest>
ProjectSessionViewModel::capture_private_m13_fresh_postmaterialization_probe(
    double independent_m16_safety_ceiling_db_tp)
{
    namespace m12=l3_automatch::m12_frozen;
    namespace m13=l3_automatch::m13_frozen;
    using R=core::Result<m13::M13ImmutableProductFreshProbeRequest>;
    const auto fail=[](const char* why){
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    if(QThread::currentThread()!=thread()||m18ProjectOperationActive_||
       isCommittingProjectOpen_||!m18ProjectOpenBatchBegin_||
       !m18ProjectSaveBatchBegin_||!opened_||
       !previewController_||!masteringChainState_||
       !source_||!source_->source_resource()||
       !gold_||!gold_->gold_resource()||
       !projectId_||!sourceId_||!referenceId_||
       !std::isfinite(independent_m16_safety_ceiling_db_tp))
        return fail("L3_M13_PRODUCT_FRESH_NO_CURRENT_M12_OWNER");
    if(private_m12_match_state()!=M12CurrentMatchState::active)
        return fail("L3_M13_PRODUCT_FRESH_MATCH_NOT_ACTIVE");
    M18ScopedProjectOperation operation{&m18ProjectOperationActive_};
    // A read-only capture must also respect an active M18 worker's
    // physical Project mutation epoch. The lease ends with this method;
    // the numerical worker owns only frozen PCM and DSP snapshots.
    auto lease=begin_m18_project_save();
    if(!lease||!*lease.value())
        return fail("L3_M13_PRODUCT_FRESH_EPOCH_UNAVAILABLE");
    const auto exclusiveOwner=*lease.value();
    Q_UNUSED(exclusiveOwner);
    const auto ticket=
        previewController_->private_m12_product_preview_ticket();
    if(!ticket||!ticket->replay||!ticket->exact_prepared_owner)
        return fail("L3_M13_PRODUCT_FRESH_LINKED_REPLAY_ABSENT");
    const auto receiptFromCurrentSession=
        !m12UndoHistory_.empty()&&
        m12UndoHistory_.back().receipt.has_value()&&
        m12UndoHistory_.back().afterTicket==ticket&&
        m12::m12_exact_project_snapshot_equal(
            *opened_,m12UndoHistory_.back().after);
    const auto reopenedWithExactOwner=
        m12ExactReplayReadback_&&
        m12::m12_exact_project_snapshot_equal(
            *opened_,*m12ExactReplayReadback_);
    if(!receiptFromCurrentSession&&!reopenedWithExactOwner)
        return fail("L3_M13_PRODUCT_FRESH_NO_M12_RECEIPT_OR_REOPEN_PROOF");
    const auto& doc=opened_->document();
    if(doc.projectId!=*projectId_||
       doc.sourceResourceId!=*sourceId_||
       doc.references.activeReferenceId!=referenceId_||
       !m12::m12_reopened_persisted_replay_matches_exact_ticket(
           doc,*ticket->replay))
        return fail("L3_M13_PRODUCT_FRESH_PERSISTED_REF_OWNER_STALE");
    const auto& provider=previewController_->snapshot_provider();
    if(!provider||provider()!=ticket->exact_prepared_owner)
        return fail("L3_M13_PRODUCT_FRESH_PREPARED_OWNER_CHANGED");
    const auto& actualChain=masteringChainState_->chain();
    const auto actualBindings=masteringChainState_->execution_bindings();
    const auto accepted=m12::prepare_m12_immutable_product_preview_ticket(
        ticket->replay,ticket->exact_prepared_owner,
        actualChain,actualBindings);
    if(!accepted||
       accepted.value()->exact_manual_chain_revision!=
           ticket->exact_manual_chain_revision||
       accepted.value()->exact_manual_nodes!=ticket->exact_manual_nodes||
       accepted.value()->exact_manual_bindings!=
           ticket->exact_manual_bindings)
        return fail("L3_M13_PRODUCT_FRESH_PROTECTED_MANUAL_CHANGED");
    const auto stem=QString::fromStdString(doc.displayName);
    const auto live=current_snapshot(stem);
    if(!live||!m12::m12_exact_project_snapshot_equal(
        *live.value(),*opened_)||
       previewController_->private_m12_product_preview_ticket()!=
           ticket||!provider||
       provider()!=ticket->exact_prepared_owner||
       masteringChainState_->execution_bindings()!=actualBindings)
        return fail("L3_M13_PRODUCT_FRESH_EPOCH_RACED");
    return R::success(m13::M13ImmutableProductFreshProbeRequest{
        ticket,independent_m16_safety_ceiling_db_tp,
        m12ProductOwnershipEpoch_});
}

bool ProjectSessionViewModel::private_m13_fresh_probe_still_current(
    const l3_automatch::m13_frozen::
        M13ImmutableProductFreshProbeRequest& completed_request)
{
    namespace m12=l3_automatch::m12_frozen;
    if(QThread::currentThread()!=thread()||m18ProjectOperationActive_||
       isCommittingProjectOpen_||!m18ProjectOpenBatchBegin_||
       !m18ProjectSaveBatchBegin_||!opened_||
       !source_||!source_->source_resource()||
       !gold_||!gold_->gold_resource()||
       !previewController_||!masteringChainState_||
       !std::isfinite(completed_request.ceilingDbTp_)||
       private_m12_match_state()!=M12CurrentMatchState::active)
        return false;
    M18ScopedProjectOperation operation{&m18ProjectOperationActive_};
    const auto lease=begin_m18_project_save();
    if(!lease||!*lease.value())return false;
    const auto held=*lease.value();
    Q_UNUSED(held);
    const auto ticket=previewController_->
        private_m12_product_preview_ticket();
    if(!ticket||ticket!=completed_request.ticket_||
       completed_request.exactProductEpoch_!=
           m12ProductOwnershipEpoch_||
       !ticket->replay||!ticket->exact_prepared_owner)
        return false;
    const auto committedHere=!m12UndoHistory_.empty()&&
        m12UndoHistory_.back().receipt.has_value()&&
        m12UndoHistory_.back().afterTicket==ticket&&
        m12::m12_exact_project_snapshot_equal(
            *opened_,m12UndoHistory_.back().after);
    const auto reopenedCurrent=m12ExactReplayReadback_&&
        m12::m12_exact_project_snapshot_equal(
            *opened_,*m12ExactReplayReadback_);
    if(!committedHere&&!reopenedCurrent)return false;
    const auto& doc=opened_->document();
    if(!projectId_||!sourceId_||!referenceId_||
       doc.projectId!=*projectId_||
       doc.sourceResourceId!=*sourceId_||
       doc.references.activeReferenceId!=referenceId_||
       !m12::m12_reopened_persisted_replay_matches_exact_ticket(
           doc,*ticket->replay))
        return false;
    const auto& provider=previewController_->snapshot_provider();
    const auto& chain=masteringChainState_->chain();
    const auto instances=chain.instances();
    if(!provider||provider()!=ticket->exact_prepared_owner||
       instances.size()!=ticket->exact_manual_nodes.size()||
       chain.revision()!=ticket->exact_manual_chain_revision||
       !std::equal(instances.begin(),instances.end(),
                   ticket->exact_manual_nodes.begin())||
       masteringChainState_->execution_bindings()!=
           ticket->exact_manual_bindings)
        return false;
    const auto stem=QString::fromStdString(doc.displayName);
    auto live=current_snapshot(stem);
    return live&&m12::m12_exact_project_snapshot_equal(
        *live.value(),*opened_)&&
        previewController_->private_m12_product_preview_ticket()==
            ticket&&provider&&provider()==ticket->exact_prepared_owner;
}

l3_automatch::m13_frozen::M13ProvisionalAssessment
ProjectSessionViewModel::assess_private_m13_completed_fresh_worker(
    const l3_automatch::m13_frozen::
        M13ImmutableProductFreshProbeRequest& request,
    const l3_automatch::m13_frozen::M13FreshProductMasterProbe& result,
    const l3_automatch::m13_frozen::M13TwentySevenConformance& native,
    const l3_automatch::m13_frozen::FullM08Comparison& proximity,
    bool independent_reference_and_scopes_current,
    l3_automatch::m13_frozen::MandatoryHardPredicate m11_protected,
    l3_automatch::m13_frozen::MandatoryHardPredicate other_hard_safety)
{
    // Revalidate on the REAL Product owner, after the worker finished.
    // If Undo->Redo reproduces the original PCM, the independent M12
    // publication epoch still marks the worker as STALE (no ABA).
    const bool current=private_m13_fresh_probe_still_current(request);
    return l3_automatch::m13_frozen::
        assess_provisional_m13_product_fresh_evidence(
            request,result,native,proximity,current,
            independent_reference_and_scopes_current,
            m11_protected,other_hard_safety);
}

core::Result<l3_automatch::m13_frozen::
    M13ProvisionalDiagnosticEnvelope>
ProjectSessionViewModel::seal_private_m13_provisional_diagnostic(
    const l3_automatch::m13_frozen::
        M13ImmutableProductFreshProbeRequest& request,
    const l3_automatch::m13_frozen::M13FreshProductMasterProbe& result,
    const l3_automatch::m13_frozen::M13TwentySevenConformance& native,
    const l3_automatch::m13_frozen::FullM08Comparison& proximity,
    bool independent_reference_and_scopes_current,
    l3_automatch::m13_frozen::MandatoryHardPredicate m11_protected,
    l3_automatch::m13_frozen::MandatoryHardPredicate other_hard_safety)
{
    namespace m13=l3_automatch::m13_frozen;
    using R=core::Result<m13::M13ProvisionalDiagnosticEnvelope>;
    if(!private_m13_fresh_probe_still_current(request))
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "L3_M13_DIAGNOSTIC_NO_CURRENT_PRODUCT_OWNER"});
    // A report may truthfully show analysis or profile UNAVAILABLE.
    // It cannot be issued for an expired M12 Materialization epoch.
    const auto diagnosis=m13::assess_provisional_m13_product_fresh_evidence(
        request,result,native,proximity,true,
        independent_reference_and_scopes_current,
        m11_protected,other_hard_safety);
    if(diagnosis.state==m13::VerificationState::stale)
        return R::failure(core::Error{core::ErrorCode::InvalidState,
            "L3_M13_DIAGNOSTIC_STALE_PRODUCT_OWNER"});
    const auto id=next_id();
    return m13::encode_m13_provisional_diagnostic(
        request,result,diagnosis,id);
}

core::Result<l3_automatch::m13_frozen::M13ProvisionalDiagnosticEnvelope>
ProjectSessionViewModel::seal_private_m13_actual_native_completion(
    const l3_automatch::m13_frozen::M13ActualNativeWorkerCompletion& completion)
{
    namespace m13=l3_automatch::m13_frozen;
    using R=core::Result<m13::M13ProvisionalDiagnosticEnvelope>;
    const auto fail=[](const char* why) {
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    const auto& fresh=completion.fresh();
    const auto& request=fresh.executed_request;
    if(!private_m13_fresh_probe_still_current(request))
        return fail("L3_M13_ACTUAL_COMPLETION_STALE_PRODUCT");
    const auto& owner=completion.source_reference_owner();
    const auto& p=source_->source_resource()->reference();
    const auto& g=gold_->gold_resource()->reference();
    if(!owner.populated()||
       owner.source_provider_id!=p.provider_id()||
       owner.source_locator!=p.locator()||
       owner.reference_provider_id!=g.provider_id()||
       owner.reference_locator!=g.locator()||
       owner.reference_selection_revision!=gold_->selection_revision())
        return fail("L3_M13_ACTUAL_COMPLETION_STALE_REFERENCE");
    if(completion.assessment().state==m13::VerificationState::stale)
        return fail("L3_M13_ACTUAL_COMPLETION_STALE_EVIDENCE");
    return m13::encode_m13_provisional_diagnostic(
        request,fresh,completion.assessment(),next_id());
}

core::Status ProjectSessionViewModel::apply_private_m12_fixed_only_match(
    const project::ProjectSnapshot& expected,core::Uuid controller_id,
    core::Uuid linked_id,const std::shared_ptr<const render::RenderResult>& prepared,
    bool suspend_for_zero_amount)
{
    namespace m=l3_automatch::m12_frozen;
    const auto fail=[](const char* code){return core::Status::failure(
        core::Error{core::ErrorCode::InvalidState,code});};
    if(QThread::currentThread()!=thread()||m18ProjectOperationActive_||
       isCommittingProjectOpen_||degraded_||!opened_||!sessionChainMaterialized_||
       !m18ProjectOpenBatchBegin_||!m18ProjectSaveBatchBegin_||
       !previewController_||!masteringChainState_||!prepared)
        return fail("L3_M12_CLEAR_NO_EXCLUSIVE_OWNER");
    M18ScopedProjectOperation operation{&m18ProjectOperationActive_};
    auto lease=begin_m18_project_save();
    if(!lease||!*lease.value())return fail("L3_M12_CLEAR_EPOCH_UNAVAILABLE");
    const auto held=*lease.value();
    Q_UNUSED(held);
    auto live=current_snapshot(QString::fromStdString(expected.document().displayName));
    const auto ticket=previewController_->private_m12_product_preview_ticket();
    const auto& provider=previewController_->snapshot_provider();
    const bool receipt=live&&!m12UndoHistory_.empty()&&m12UndoHistory_.back().receipt&&
        m12UndoHistory_.back().afterTicket==ticket&&
        m::m12_exact_project_snapshot_equal(m12UndoHistory_.back().after,*live.value());
    const bool reopened=live&&m12UndoHistory_.empty()&&m12ExactReplayReadback_&&
        m::m12_exact_project_snapshot_equal(*m12ExactReplayReadback_,*live.value());
    if(!live||!ticket||!ticket->replay||!provider||provider()!=prepared||
       ticket->exact_prepared_owner!=prepared||
       (!receipt&&!reopened)||ticket->replay->controller_id!=controller_id||
       ticket->replay->linked_chain_id!=linked_id||
       !m::m12_exact_project_snapshot_equal(*live.value(),expected)||
       !m::m12_exact_project_snapshot_equal(*opened_,expected)||
       !m::m12_reopened_persisted_replay_matches_exact_ticket(live.value()->document(),*ticket->replay))
        return fail("L3_M12_CLEAR_PRIOR_OWNER_STALE");
    // Stage the complete new owner, then publish through the same exact
    // rollback and history path as explicit positive no-correction.
    std::optional<project::ProjectSnapshot> staged;
    std::shared_ptr<const m::M12ImmutableProductPreviewTicket> afterTicket;
    if(suspend_for_zero_amount) {
        auto draft=m::prepare_m12_owned_controller_suspension(*live.value(),
            controller_id,linked_id,m::ControllerSuspensionCause::amount_zero,true);
        if(!draft)return core::Status::failure(*draft.error());
        staged.emplace(std::move(draft.value()->updated_project_not_committed));
        const auto decoded=m::decode_exact_m12_ref_linked_product_state(
            staged->document(),ticket->replay->registry_owner);
        if(!decoded)return core::Status::failure(*decoded.error());
        const auto bindings=masteringChainState_->execution_bindings();
        auto replay=m::prepare_restored_m12_native_replay_plan(staged->document(),
            *decoded.value(),masteringChainState_->chain_id(),
            masteringChainState_->chain(),bindings,true);
        if(!replay)return core::Status::failure(*replay.error());
        const auto next=m::prepare_m12_immutable_product_preview_ticket(
            std::make_shared<const m::RestoredM12NativeReplayPlan>(std::move(*replay.value())),
            prepared,masteringChainState_->chain(),bindings);
        if(!next)return core::Status::failure(*next.error());
        afterTicket=std::make_shared<const m::M12ImmutableProductPreviewTicket>(*next.value());
        // Never claim a suspended graph has fresh verification or remove
        // a stored DSP as a side effect of Amount zero.
        const auto beforeNodes=ticket->replay->reference_linked.instances();
        const auto afterNodes=afterTicket->replay->reference_linked.instances();
        if(beforeNodes.size()!=afterNodes.size()||
           ticket->replay->reference_bindings!=afterTicket->replay->reference_bindings||
           std::any_of(afterNodes.begin(),afterNodes.end(),[](const auto& node){
               return !node.controller_suspended()||node.active();
           }))
            return fail("L3_M12_SUSPEND_STORED_GRAPH_CHANGED");
    } else {
    auto doc=live.value()->document();
    const auto removed=std::erase_if(doc.chains,[&](const auto& chain){
        return chain.stage=="MASTER"&&chain.segment=="REF_LINKED"&&chain.chainId==linked_id;
    });
    if(removed!=1U)return fail("L3_M12_CLEAR_LINKED_ID_MISMATCH");
    auto metadata=QJsonDocument::fromJson(QByteArray::fromStdString(doc.referenceMatch.canonical_utf8()));
    if(!metadata.isObject())return fail("L3_M12_CLEAR_METADATA_INVALID");
    auto object=metadata.object();
    const auto retained=m::decode_m12_retained_controller(object);
    if(object.contains(QStringLiteral("retainedController"))&&
       (!retained||retained->controller_id!=controller_id||retained->linked_chain_id!=linked_id||
        retained->last_removed_revision>=ticket->replay->reference_linked.revision()))
        return fail("L3_M12_CLEAR_RETAINED_CONTROLLER_CONFLICT");
    object.insert(QStringLiteral("retainedController"),m::encode_m12_retained_controller(
        {controller_id,linked_id,ticket->replay->reference_linked.revision()}));
    object.remove(QStringLiteral("materialization"));
    auto canonical=project::OpaqueJsonValue::parse(QJsonDocument(object).toJson(QJsonDocument::Compact).toStdString());
    if(!canonical)return core::Status::failure(*canonical.error());
    doc.referenceMatch=*canonical.value();
    auto after=project::ProjectSnapshot::create(std::move(doc),
        {live.value()->optional_entries().begin(),live.value()->optional_entries().end()});
    if(!after)return core::Status::failure(*after.error());
    if(m::assess_current_product_ref_linked_open_safety(after.value()->document())!=
       m::ProjectOpenLinkedSafety::ordinary_project_without_ref_linked)
        return fail("L3_M12_CLEAR_FOREIGN_LINKED_CONTENT");
    staged.emplace(std::move(*after.value()));
    }
    M12MaterializationHistoryEntry entry{*live.value(),*staged,ticket,afterTicket,std::nullopt};
    m12UndoHistory_.reserve(m12UndoHistory_.size()+1U);
    const auto bindings=masteringChainState_->execution_bindings();
    opened_.emplace(entry.after);
    previewController_->set_private_m12_product_preview_ticket(afterTicket);
    const auto readback=current_snapshot(QString::fromStdString(expected.document().displayName));
    if(!readback||!m::m12_exact_project_snapshot_equal(*readback.value(),entry.after)||
       previewController_->private_m12_product_preview_ticket()!=afterTicket||provider()!=prepared||
       masteringChainState_->execution_bindings()!=bindings) {
        opened_.emplace(entry.before);
        previewController_->set_private_m12_product_preview_ticket(ticket);
        return fail("L3_M12_CLEAR_READBACK_ROLLED_BACK");
    }
    if(afterTicket) {
        const auto& replay=*afterTicket->replay;
        // A receipt attests actual publication/readback, not a new
        // calculated plan or a VERIFIED result. Suspension retains plan.
        m::M12ProductMaterializationReceipt suspensionReceipt{replay.project_id,replay.source_id,
            replay.reference_id,replay.plan_id,replay.controller_id,replay.linked_chain_id,
            replay.exact_plan_checksum};
        entry.receipt.emplace(std::move(suspensionReceipt));
    }
    m12UndoHistory_.push_back(std::move(entry));
    m12RedoHistory_.clear();
    ++m12ProductOwnershipEpoch_;
    statusText_=suspend_for_zero_amount?QStringLiteral("MATCH SUSPENDED / NOT_VERIFIED"):
        QStringLiteral("NO CORRECTION / MANUAL");
    clear_error();
    return core::Status::success();
}

core::Result<std::shared_ptr<const l3_automatch::ManualMatchTransferCapture>>
ProjectSessionViewModel::capture_private_manual_match_transfer(const core::Uuid& linked_instance)
{
    namespace a=l3_automatch;
    namespace m=a::m12_frozen;
    using R=core::Result<std::shared_ptr<const a::ManualMatchTransferCapture>>;
    const auto fail=[](const char* why){return R::failure(core::Error{core::ErrorCode::InvalidState,why});};
    if(linked_instance.is_nil()||QThread::currentThread()!=thread()||
       m18ProjectOperationActive_||isCommittingProjectOpen_||degraded_||
       !opened_||!sessionChainMaterialized_||!m18ProjectOpenBatchBegin_||
       !m18ProjectSaveBatchBegin_||!previewController_||!masteringChainState_||
       private_m12_match_state()!=M12CurrentMatchState::active)
        return fail("MANUAL_MATCH_TRANSFER_NO_CURRENT_PRODUCT_OWNER");
    M18ScopedProjectOperation operation{&m18ProjectOperationActive_};
    const auto lease=begin_m18_project_save();
    if(!lease||!*lease.value())return fail("MANUAL_MATCH_TRANSFER_EPOCH_UNAVAILABLE");
    const auto held=*lease.value();
    Q_UNUSED(held);
    const auto capturedEpoch=m12ProductOwnershipEpoch_;
    const auto ticket=previewController_->private_m12_product_preview_ticket();
    const auto& provider=previewController_->snapshot_provider();
    const auto registry=masteringChainState_->registry_snapshot();
    if(!ticket||!ticket->replay||!registry||!provider||
       provider()!=ticket->exact_prepared_owner||
       !previewController_->private_m12_linked_editor_owner_matches(*masteringChainState_))
        return fail("MANUAL_MATCH_TRANSFER_PRODUCT_OWNER_STALE");
    const auto live=current_snapshot(QString::fromStdString(opened_->document().displayName));
    const bool receipt=live&&!m12UndoHistory_.empty()&&m12UndoHistory_.back().receipt&&
        m12UndoHistory_.back().afterTicket==ticket&&
        m::m12_exact_project_snapshot_equal(m12UndoHistory_.back().after,*live.value());
    const bool reopened=live&&m12ExactReplayReadback_&&
        m::m12_exact_project_snapshot_equal(*m12ExactReplayReadback_,*live.value());
    if(!live||(!receipt&&!reopened)||
       !m::m12_exact_project_snapshot_equal(*opened_,*live.value()))
        return fail("MANUAL_MATCH_TRANSFER_PROJECT_NOT_CURRENT");
    const auto id=dsp::ModuleInstanceId::from_uuid(linked_instance);
    if(!id)return R::failure(*id.error());
    const auto bindings=masteringChainState_->execution_bindings();
    auto draft=a::prepare_manual_match_transfer(ticket,masteringChainState_->chain(),
        bindings,*registry,*id.value());
    if(!draft)return R::failure(*draft.error());
    auto projectDraft=a::prepare_manual_match_transfer_project(*live.value(),*draft.value());
    if(!projectDraft)return R::failure(*projectDraft.error());
    if(previewController_->private_m12_product_preview_ticket()!=ticket||
       provider()!=ticket->exact_prepared_owner||
       masteringChainState_->execution_bindings()!=bindings||
       masteringChainState_->chain().revision()!=ticket->exact_manual_chain_revision||
       m12ProductOwnershipEpoch_!=capturedEpoch)
        return fail("MANUAL_MATCH_TRANSFER_CAPTURE_RACED");
    return R::success(std::make_shared<const a::ManualMatchTransferCapture>(
        a::ManualMatchTransferCapture{capturedEpoch,
            std::move(*draft.value()),std::move(*projectDraft.value())}));
}

bool ProjectSessionViewModel::private_manual_match_transfer_still_current(
    const l3_automatch::ManualMatchTransferCapture& capture)
{
    if(QThread::currentThread()!=thread()||capture.product_epoch!=m12ProductOwnershipEpoch_)
        return false;
    const auto fresh=capture_private_manual_match_transfer(capture.native.linked_instance.uuid());
    if(!fresh)return false;
    const auto& now=**fresh.value();
    return m12ProductOwnershipEpoch_==capture.product_epoch&&
        now.product_epoch==capture.product_epoch&&
        now.native.exact_prior_owner==capture.native.exact_prior_owner&&
        now.project.identity.sha256==capture.project.identity.sha256&&
        now.project.identity.canonical_content==capture.project.identity.canonical_content&&
        now.project.matches_before(capture.project.before)&&
        now.project.matches_after(capture.project.after);
}

core::Result<std::shared_ptr<const l3_automatch::ManualMatchTransferAdoptionDraft>>
ProjectSessionViewModel::prepare_private_manual_match_transfer_adoption(
    std::shared_ptr<const l3_automatch::ManualMatchTransferAudition> audition,
    const l3_automatch::ManualMatchTransferConfirmation& confirmation)
{
    namespace a=l3_automatch;
    using R=core::Result<std::shared_ptr<const a::ManualMatchTransferAdoptionDraft>>;
    const auto fail=[](const char* why){return R::failure(core::Error{
        core::ErrorCode::InvalidState,why});};
    if(!audition||QThread::currentThread()!=thread()||
       !a::manual_match_transfer_confirmation_matches(*audition,confirmation)||
       !private_manual_match_transfer_still_current(audition->capture()))
        return fail("MANUAL_MATCH_ADOPTION_CONFIRMATION_OR_OWNER_STALE");
    if(!opened_||!masteringChainState_||!previewController_)
        return fail("MANUAL_MATCH_ADOPTION_OWNER_ABSENT");
    M18ScopedProjectOperation operation{&m18ProjectOperationActive_};
    const auto lease=begin_m18_project_save();
    if(!lease||!*lease.value())return fail("MANUAL_MATCH_ADOPTION_EPOCH_UNAVAILABLE");
    const auto held=*lease.value();
    Q_UNUSED(held);
    const auto& capture=audition->capture();
    const auto& provider=previewController_->snapshot_provider();
    const auto ticket=capture.native.exact_prior_owner;
    const auto current=[&](){
        return ticket&&provider&&provider()==ticket->exact_prepared_owner&&
            capture.product_epoch==m12ProductOwnershipEpoch_&&
            previewController_->private_m12_product_preview_ticket()==ticket&&
            previewController_->private_m12_linked_editor_owner_matches(*masteringChainState_);
    };
    if(!current())return fail("MANUAL_MATCH_ADOPTION_OWNER_CHANGED");
    auto live=current_snapshot(QString::fromStdString(opened_->document().displayName));
    if(!live||!capture.project.matches_before(*opened_))
        return fail("MANUAL_MATCH_ADOPTION_PROJECT_CHANGED");
    auto draft=a::prepare_manual_match_transfer_adoption(
        *live.value(),*masteringChainState_,audition,confirmation);
    if(!draft)return R::failure(*draft.error());
    if(!current()||masteringChainState_->execution_bindings()!=ticket->exact_manual_bindings)
        return fail("MANUAL_MATCH_ADOPTION_STAGING_RACED");
    return R::success(std::make_shared<const a::ManualMatchTransferAdoptionDraft>(
        std::move(*draft.value())));
}

ProjectSessionViewModel::M12CurrentMatchState ProjectSessionViewModel::private_m12_match_state() const
{
    namespace m=l3_automatch::m12_frozen;
    using S=M12CurrentMatchState;
    if(QThread::currentThread()!=thread())return S::stale;
    const auto ticket=previewController_?previewController_->private_m12_product_preview_ticket():nullptr;
    const bool persisted=opened_&&m::assess_current_product_ref_linked_open_safety(opened_->document())!=
        m::ProjectOpenLinkedSafety::ordinary_project_without_ref_linked;
    if(!persisted&&!ticket)return S::none;
    if(!ticket||!ticket->replay||!persisted||!can_save_project())return S::stale;
    const auto meta=QJsonDocument::fromJson(QByteArray::fromStdString(
        opened_->document().referenceMatch.canonical_utf8()));
    if(!meta.isObject())return S::stale;
    const auto freshness=meta.object().value(QStringLiteral("materialization")).toObject()
        .value(QStringLiteral("freshnessState"));
    if(!freshness.isUndefined()&&freshness.toString()!=QStringLiteral("CURRENT"))return S::stale;
    const auto nodes=ticket->replay->reference_linked.instances();
    return !nodes.empty()&&std::all_of(nodes.begin(),nodes.end(),
        [](const auto& node){return node.controller_suspended();})?S::suspended:S::active;
}

bool ProjectSessionViewModel::can_undo_private_m12_materialization() const
{
    namespace m=l3_automatch::m12_frozen;
    if(QThread::currentThread()!=thread()||m18ProjectOperationActive_||
       isCommittingProjectOpen_||degraded_||!opened_||
       !m18ProjectSaveBatchBegin_||!m18ProjectOpenBatchBegin_||
       !previewController_||!masteringChainState_||m12UndoHistory_.empty())return false;
    const auto& last=m12UndoHistory_.back();
    const auto witness=last.afterTicket?last.afterTicket:last.beforeTicket;
    const auto& provider=previewController_->snapshot_provider();
    const auto& chain=masteringChainState_->chain();
    return witness&&witness->replay&&provider&&provider()==witness->exact_prepared_owner&&
        previewController_->private_m12_product_preview_ticket()==last.afterTicket&&
        m::m12_exact_project_snapshot_equal(*opened_,last.after)&&
        chain.revision()==witness->exact_manual_chain_revision&&
        std::equal(chain.instances().begin(),chain.instances().end(),
            witness->exact_manual_nodes.begin(),witness->exact_manual_nodes.end())&&
        masteringChainState_->execution_bindings()==witness->exact_manual_bindings;
}

bool ProjectSessionViewModel::can_redo_private_m12_materialization() const
{
    namespace m=l3_automatch::m12_frozen;
    if(QThread::currentThread()!=thread()||m18ProjectOperationActive_||
       isCommittingProjectOpen_||degraded_||!opened_||
       !m18ProjectSaveBatchBegin_||!m18ProjectOpenBatchBegin_||
       !previewController_||!masteringChainState_||m12RedoHistory_.empty())return false;
    const auto& next=m12RedoHistory_.back();
    const auto witness=next.afterTicket?next.afterTicket:next.beforeTicket;
    const auto& provider=previewController_->snapshot_provider();
    const auto& chain=masteringChainState_->chain();
    return witness&&witness->replay&&provider&&provider()==witness->exact_prepared_owner&&
        previewController_->private_m12_product_preview_ticket()==next.beforeTicket&&
        m::m12_exact_project_snapshot_equal(*opened_,next.before)&&
        chain.revision()==witness->exact_manual_chain_revision&&
        std::equal(chain.instances().begin(),chain.instances().end(),
            witness->exact_manual_nodes.begin(),witness->exact_manual_nodes.end())&&
        masteringChainState_->execution_bindings()==witness->exact_manual_bindings;
}

bool ProjectSessionViewModel::can_resume_private_m12_suspension() const
{
    namespace m=l3_automatch::m12_frozen;
    if(private_m12_match_state()!=M12CurrentMatchState::suspended||
       !can_undo_private_m12_materialization())return false;
    const auto& entry=m12UndoHistory_.back();
    if(!entry.beforeTicket||!entry.beforeTicket->replay||!entry.afterTicket||
       !entry.afterTicket->replay)return false;
    const auto& prior=*entry.beforeTicket->replay;
    const auto old=prior.reference_linked.instances();
    if(old.empty()||std::none_of(old.begin(),old.end(),
        [](const auto& node){return node.active();}))return false;
    const auto meta=QJsonDocument::fromJson(QByteArray::fromStdString(
        entry.before.document().referenceMatch.canonical_utf8()));
    if(!meta.isObject())return false;
    const auto freshness=meta.object().value(QStringLiteral("materialization")).toObject()
        .value(QStringLiteral("freshnessState"));
    if(!freshness.isUndefined()&&freshness.toString()!=QStringLiteral("CURRENT"))return false;
    const auto suspended=m::prepare_m12_owned_controller_suspension(entry.before,
        prior.controller_id,prior.linked_chain_id,m::ControllerSuspensionCause::amount_zero,true);
    return suspended&&m::m12_exact_project_snapshot_equal(
        suspended.value()->updated_project_not_committed,entry.after);
}
core::Status ProjectSessionViewModel::resume_private_m12_suspension()
{
    // This is precisely Undo of the last accepted state-only suspension,
    // not a new positive Amount or an assertion of fresh M13 evidence.
    if(!can_resume_private_m12_suspension())
        return core::Status::failure(core::Error{core::ErrorCode::InvalidState,
            "L3_M12_RESUME_REQUIRES_EXACT_CURRENT_SUSPENSION_HISTORY"});
    return undo_private_m12_materialization();
}

core::Status ProjectSessionViewModel::undo_private_m12_materialization()
{
    namespace m=l3_automatch::m12_frozen;
    const auto fail=[](const char* why) {
        return core::Status::failure(
            core::Error{core::ErrorCode::InvalidState,why});
    };
    if(!can_undo_private_m12_materialization())
        return fail("L3_M12_UNDO_NO_EXCLUSIVE_OWNER_OR_HISTORY");
    M18ScopedProjectOperation operation{&m18ProjectOperationActive_};
    auto lease=begin_m18_project_save();
    if(!lease||!*lease.value())
        return fail("L3_M12_UNDO_EPOCH_UNAVAILABLE");
    const auto heldLease=*lease.value();
    Q_UNUSED(heldLease);
    const auto& last=m12UndoHistory_.back();
    const auto witness=last.afterTicket?last.afterTicket:last.beforeTicket;
    auto live=current_snapshot(
        QString::fromStdString(last.after.document().displayName));
    const auto& provider=previewController_->snapshot_provider();
    if(!live||!m::m12_exact_project_snapshot_equal(
        *live.value(),last.after)||
       !m::m12_exact_project_snapshot_equal(*opened_,last.after)||
       previewController_->private_m12_product_preview_ticket()!=
           last.afterTicket||
       !witness||!provider||
       provider()!=witness->exact_prepared_owner)
        return fail("L3_M12_UNDO_CURRENT_PRODUCT_STATE_STALE");
    m12RedoHistory_.reserve(m12RedoHistory_.size()+1U);
    // Commit a FULL prior snapshot. A former linked controller's
    // binding is restored only from its exact historical ticket, never
    // inferred from a matching module name/type.
    opened_.emplace(last.before);
    previewController_->set_private_m12_product_preview_ticket(
        last.beforeTicket);
    auto readback=current_snapshot(
        QString::fromStdString(last.before.document().displayName));
    if(!readback||
       previewController_->private_m12_product_preview_ticket()!=
           last.beforeTicket||
       !provider||provider()!=witness->exact_prepared_owner||
       !m::m12_exact_project_snapshot_equal(
           *readback.value(),last.before)) {
        opened_.emplace(last.after);
        previewController_->set_private_m12_product_preview_ticket(
            last.afterTicket);
        return fail("L3_M12_UNDO_READBACK_FAILED_ROLLED_BACK");
    }
    m12RedoHistory_.push_back(std::move(m12UndoHistory_.back()));
    m12UndoHistory_.pop_back();
    ++m12ProductOwnershipEpoch_;
    statusText_=m12RedoHistory_.back().beforeTicket
        ?QStringLiteral("MATERIALIZED / NOT_VERIFIED")
        :QStringLiteral("M12 UNDO / MANUAL");
    clear_error();
    return core::Status::success();
}

core::Status ProjectSessionViewModel::redo_private_m12_materialization()
{
    namespace m=l3_automatch::m12_frozen;
    const auto fail=[](const char* why) {
        return core::Status::failure(
            core::Error{core::ErrorCode::InvalidState,why});
    };
    if(!can_redo_private_m12_materialization())
        return fail("L3_M12_REDO_NO_EXCLUSIVE_OWNER_OR_HISTORY");
    M18ScopedProjectOperation operation{&m18ProjectOperationActive_};
    auto lease=begin_m18_project_save();
    if(!lease||!*lease.value())
        return fail("L3_M12_REDO_EPOCH_UNAVAILABLE");
    const auto heldLease=*lease.value();
    Q_UNUSED(heldLease);
    const auto& next=m12RedoHistory_.back();
    const auto witness=next.afterTicket?next.afterTicket:next.beforeTicket;
    auto live=current_snapshot(
        QString::fromStdString(next.before.document().displayName));
    const auto& provider=previewController_->snapshot_provider();
    if(!live||!m::m12_exact_project_snapshot_equal(
        *live.value(),next.before)||
       !m::m12_exact_project_snapshot_equal(*opened_,next.before)||
       previewController_->private_m12_product_preview_ticket()!=
           next.beforeTicket||
       !witness||!witness->replay||!provider||
       provider()!=witness->exact_prepared_owner)
        return fail("L3_M12_REDO_CURRENT_PRODUCT_STATE_STALE");
    const auto bindings=masteringChainState_->execution_bindings();
    const auto replay=m::prepare_m12_immutable_product_preview_ticket(
        witness->replay,
        witness->exact_prepared_owner,
        masteringChainState_->chain(),bindings);
    if(!replay||
       replay.value()->exact_manual_nodes!=
           witness->exact_manual_nodes||
       replay.value()->exact_manual_bindings!=
           witness->exact_manual_bindings)
        return fail("L3_M12_REDO_MANUAL_OR_PREPARED_OWNER_STALE");
    m12UndoHistory_.reserve(m12UndoHistory_.size()+1U);
    opened_.emplace(next.after);
    previewController_->set_private_m12_product_preview_ticket(
        next.afterTicket);
    auto readback=current_snapshot(
        QString::fromStdString(next.after.document().displayName));
    if(!readback||
       previewController_->private_m12_product_preview_ticket()!=
           next.afterTicket||
       !provider||provider()!=witness->exact_prepared_owner||
       !m::m12_exact_project_snapshot_equal(
           *readback.value(),next.after)) {
        opened_.emplace(next.before);
        previewController_->set_private_m12_product_preview_ticket(
            next.beforeTicket);
        return fail("L3_M12_REDO_READBACK_FAILED_ROLLED_BACK");
    }
    m12UndoHistory_.push_back(std::move(m12RedoHistory_.back()));
    m12RedoHistory_.pop_back();
    ++m12ProductOwnershipEpoch_;
    statusText_=m12UndoHistory_.back().afterTicket
        ?QStringLiteral("MATERIALIZED / NOT_VERIFIED")
        :QStringLiteral("NO CORRECTION / MANUAL");
    clear_error();
    return core::Status::success();
}

void ProjectSessionViewModel::saveProjectAs(const QUrl& selectedFile)
{
    const bool m18Guarded=static_cast<bool>(m18ProjectOpenBatchBegin_
        ||m18ProjectMutationAdmission_||m18ProjectSaveAdmission_
        ||m18ProjectSaveBatchBegin_);
    if(m18Guarded &&QThread::currentThread()!=thread())return;
    if(m18Guarded &&m18ProjectOperationActive_)return;
    M18ScopedProjectOperation singleWriter{
        m18Guarded?&m18ProjectOperationActive_:nullptr};
    Q_UNUSED(singleWriter);
    // The fifth P2 DSP is now serialized by current_snapshot with strict
    // typed algorithm/schema identity, under the existing M18 SaveAs batch.
    // A malformed, absent or incoherent P2 module fails before file creation.
    // M12's calculated REF_LINKED ticket is a distinct audible DSP owner.
    // Until an atomic REF_LINKED Project serializer is installed, writing
    // only MANUAL here would irreversibly misrepresent the sound. Reject
    // BEFORE file creation or any SaveAs epoch/acquisition.
    namespace m=l3_automatch::m12_frozen;
    const auto linkedTicket=previewController_
        ?previewController_->private_m12_product_preview_ticket()
        :nullptr;
    const bool persistedLinked=opened_ &&
        m::assess_current_product_ref_linked_open_safety(
            opened_->document())!=
            m::ProjectOpenLinkedSafety::ordinary_project_without_ref_linked;
    const auto* materialized=(!m12UndoHistory_.empty()&&
        m12UndoHistory_.back().receipt.has_value()&&
        m12UndoHistory_.back().afterTicket==linkedTicket&&
        linkedTicket)
        ?&m12UndoHistory_.back():nullptr;
    // An imported Project may have a persisted MATERIALIZED record and
    // a complete rehydrated native REF replay, but it must not be given
    // a fresh receipt or an invented Undo entry by Project Open.
    const bool imported=(opened_&&m12ExactReplayReadback_&&linkedTicket&&
        linkedTicket->replay&&
        m::m12_exact_project_snapshot_equal(
            *opened_,*m12ExactReplayReadback_));
    if((linkedTicket||persistedLinked)&&
       (!opened_||(!materialized&&!imported)||
        (materialized&&
         !m::m12_exact_project_snapshot_equal(
            *opened_,materialized->after)))) {
        publish_error(core::Error{core::ErrorCode::InvalidState,
            "L3_M12_PROJECT_SAVE_REF_LINKED_SERIALIZER_NOT_YET_INTEGRATED"});
        return;
    }
    if (!selectedFile.isValid() || !selectedFile.isLocalFile() ||
        !selectedFile.toLocalFile().endsWith(QStringLiteral(".rgsml"), Qt::CaseInsensitive)) {
        publish_error(invalid("destination")); return;
    }
    // Before reading even the draft Project snapshot, refuse an M18
    // worker-owned epoch. This also covers an Open-batch-only policy,
    // which otherwise could allow an unguarded Project SaveAs.
    auto saveBatch=begin_m18_project_save();
    if(!saveBatch) {publish_error(*saveBatch.error());return;}
    // The opaque Product lease remains alive across physical output write,
    // checksum/readback and final in-memory Project metadata publication.
    std::shared_ptr<const void> heldSaveBatch=std::move(*saveBatch.value());
    Q_UNUSED(heldSaveBatch);
    const QFileInfo file{selectedFile.toLocalFile()};
    auto snapshot = current_snapshot(file.completeBaseName());
    if (!snapshot) { publish_error(*snapshot.error()); return; }
    if(materialized||imported) {
        // The native carrier is already in the Project model. Serialize
        // precisely that graph, never a MANUAL-only stand-in and never a
        // stale REF snapshot after a Source / DSP / parameter edit.
        const auto& provider=previewController_->snapshot_provider();
        const auto& ticket=materialized
            ?*materialized->afterTicket:*linkedTicket;
        const auto& expected=materialized
            ?materialized->after:*m12ExactReplayReadback_;
        const auto currentBindings=masteringChainState_
            ?masteringChainState_->execution_bindings()
            :std::vector<dsp::ModuleExecutionBinding>{};
        if(!m::m12_exact_project_snapshot_equal(
                *snapshot.value(),expected)||
           !provider||provider()!=ticket.exact_prepared_owner||
           !masteringChainState_||
           masteringChainState_->chain().revision()!=
               ticket.exact_manual_chain_revision||
           masteringChainState_->execution_bindings()!=
               ticket.exact_manual_bindings||
           currentBindings!=ticket.exact_manual_bindings||
           !ticket.replay||
           !m::m12_reopened_persisted_replay_matches_exact_ticket(
               snapshot.value()->document(),*ticket.replay)||
           masteringChainState_->chain().instances().size()!=
               ticket.exact_manual_nodes.size()||
           !std::equal(masteringChainState_->chain().instances().begin(),
                       masteringChainState_->chain().instances().end(),
                       ticket.exact_manual_nodes.begin())) {
            publish_error(core::Error{core::ErrorCode::InvalidState,
                "L3_M12_PROJECT_SAVE_CURRENT_REF_LINKED_OWNER_STALE"});
            return;
        }
    }
    const auto path = selectedFile.toLocalFile().toUtf8().toStdString();
    const auto name = file.fileName().toUtf8().toStdString();
    auto writeRef = platform::windows::WindowsResourceWriter::make_write_reference(path, name);
    if (!writeRef) { publish_error(*writeRef.error()); return; }
    auto writer = platform::windows::WindowsResourceWriter::open_create_new(
        std::move(*writeRef.value()));
    if (!writer) { publish_error(*writer.error()); return; }
    auto result = project::ProjectRepository::save_create_new(*snapshot.value(),
        std::move(*writer.value()), [path, name]() {
            auto ref = platform::windows::WindowsResourceReader::make_read_reference(path, name);
            if (!ref) return core::Result<std::unique_ptr<core::IResourceReader>>::failure(*ref.error());
            auto reader = platform::windows::WindowsResourceReader::open_read_only(
                std::move(*ref.value()));
            if (!reader) return core::Result<std::unique_ptr<core::IResourceReader>>::failure(*reader.error());
            return core::Result<std::unique_ptr<core::IResourceReader>>::success(
                std::move(*reader.value()));
        });
    if (!result) { publish_error(*result.error()); return; }
    const bool wasNewSession = !opened_;
    opened_.emplace(std::move(*snapshot.value()));
    if (wasNewSession || sessionChainMaterialized_) {
        sessionChainMaterialized_ = true;
    }
    clearedRegionId_.reset();
    projectDisplayName_ = QString::fromUtf8(opened_->document().displayName.data(),
        static_cast<qsizetype>(opened_->document().displayName.size()));
    statusText_ = degraded_ ? QStringLiteral("Project saved; unsupported processing remains inactive.") :
                             QStringLiteral("Project saved and verified.");
    clear_error();
}

void ProjectSessionViewModel::cancelProjectOpen() noexcept {}
void ProjectSessionViewModel::cancelProjectSave() noexcept {}

}  // namespace rgsml::app
