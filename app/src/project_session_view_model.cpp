#include "project_session_view_model.hpp"

#include "audition_region_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "playback_transport_view_model.hpp"
#include "source_selection_view_model.hpp"

#include <rgsml/audio/source_resource.hpp>
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/platform/windows/windows_resource_identity.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/platform/windows/windows_resource_writer.hpp>
#include <rgsml/project/project_repository.hpp>

#include <QFileInfo>
#include <QUuid>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace rgsml::app {
namespace {

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
    }
    return "MANUAL";
}

[[nodiscard]] std::string owner_to_string(dsp::ModuleOwner o)
{
    switch (o) {
        case dsp::ModuleOwner::USER: return "USER";
    }
    return "USER";
}

[[nodiscard]] std::string link_state_to_string(dsp::ModuleLinkState l)
{
    switch (l) {
        case dsp::ModuleLinkState::UNLINKED: return "UNLINKED";
    }
    return "UNLINKED";
}

[[nodiscard]] bool has_future_semantics(const project::ProjectDocument& doc)
{
    if (doc.pipeline.repairChainId || doc.pipeline.conditioningChainId ||
        doc.activeProfileApplication ||
        doc.timeSelections.analysisScopes.canonical_utf8() != "[]" ||
        doc.timeSelections.processingRegions.canonical_utf8() != "[]" ||
        doc.analyses.canonical_utf8() != "[]" || doc.plans.canonical_utf8() != "[]" ||
        doc.referenceMatch.canonical_utf8() != "{}" ||
        doc.masteringDNA.canonical_utf8() != "{}" ||
        doc.output.canonical_utf8() != "{}" ||
        doc.derivedArtifacts.canonical_utf8() != "[]" ||
        doc.history.canonical_utf8() != "[]" ||
        doc.extensions.canonical_utf8() != "{}" ||
        doc.timeSelections.auditionRegions.size() > 1U) {
        return true;
    }

    if (doc.chains.empty()) {
        return false;
    }

    // A single mastering chain matching pipeline.masteringChainId is the supported B4 mastering chain topology
    if (doc.chains.size() == 1U && doc.pipeline.masteringChainId &&
        doc.chains[0].chainId == *doc.pipeline.masteringChainId &&
        doc.chains[0].stage == "MASTER" && doc.chains[0].segment == "MANUAL") {
        return false;
    }

    return true;
}

}  // namespace

ProjectSessionViewModel::ProjectSessionViewModel(
    SourceSelectionViewModel* source, GoldSelectionViewModel* gold,
    AuditionRegionViewModel* region, PlaybackTransportViewModel* playback,
    MasteringChainState* masteringChainState,
    UuidFactory uuidFactory, QObject* parent)
    : QObject(parent), source_(source), gold_(gold), region_(region),
      playback_(playback), masteringChainState_(masteringChainState),
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
}

bool ProjectSessionViewModel::can_save_project() const noexcept
{
    return source_ && source_->source_resource();
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

void ProjectSessionViewModel::on_source_changed()
{
    const auto* source = source_->source_resource();
    if (source && lastSource_ &&
        !lastSource_->same_resource_identity(source->reference())) sourceId_.reset();
    if (source) lastSource_ = source->reference();
    emit changed();
}

void ProjectSessionViewModel::on_gold_changed()
{
    const auto* gold = gold_->gold_resource();
    if (!gold) { goldId_.reset(); referenceId_.reset(); lastGold_.reset(); }
    else {
        if (lastGold_ && !lastGold_->same_resource_identity(gold->reference())) {
            goldId_.reset(); referenceId_.reset();
        }
        lastGold_ = gold->reference();
    }
    emit changed();
}

void ProjectSessionViewModel::on_region_changed()
{
    if (!region_->region() && regionId_) {
        clearedRegionId_ = regionId_;
        regionId_.reset();
    }
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
    if (opened_ && doc.sourceResourceId != *sourceId_ && has_future_semantics(doc)) {
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
            region->begin().value(), region->end().value()};
        if (found == doc.timeSelections.auditionRegions.end())
            doc.timeSelections.auditionRegions.push_back(std::move(value));
        else *found = std::move(value);
    }

    if (masteringChainState_) {
        // Enforce degraded/unsupported safety rule: if saving an opened project that has future/degraded semantics
        // or unsupported mastering chain topology, fail deterministically rather than overwriting or corrupting.
        if (opened_ && (degraded_ || has_future_semantics(doc))) {
            return core::Result<project::ProjectSnapshot>::failure(core::Error{
                core::ErrorCode::UnsupportedOperation,
                "Cannot save project with unsupported or degraded processing semantics"});
        }

        const auto chainId = masteringChainState_->chain_id();
        doc.pipeline.masteringChainId = chainId;

        project::Chain masteringChain;
        masteringChain.chainId = chainId;
        masteringChain.stage = "MASTER";
        masteringChain.segment = "MANUAL";
        masteringChain.revision = masteringChainState_->chain().revision();

        // Fail-closed helper for Module serialization
        const auto serialize_module = [&](const std::string_view typeId,
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

            if (!desc.algorithm_version() || desc.algorithm_version()->empty()) {
                return core::Result<project::Module>::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "Missing algorithmVersion in module descriptor during mastering chain serialization",
                    {{"typeId", std::string(typeId)}}});
            }

            if (!desc.parameter_schema_id() || desc.parameter_schema_id()->empty()) {
                return core::Result<project::Module>::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "Missing parameterSchemaId in module descriptor during mastering chain serialization",
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
            masteringChainState_->gain_instance(),
            dsp::encode_gain_parameters_json(masteringChainState_->gain_parameters()));
        if (!m0) return core::Result<project::ProjectSnapshot>::failure(*m0.error());
        masteringChain.modules.push_back(std::move(*m0.value()));

        // Module 1: Parametric EQ
        auto m1 = serialize_module("rgsml.dsp.parametric-eq",
            masteringChainState_->eq_instance(),
            dsp::encode_parametric_eq_parameters_json(masteringChainState_->parametric_eq_parameters()));
        if (!m1) return core::Result<project::ProjectSnapshot>::failure(*m1.error());
        masteringChain.modules.push_back(std::move(*m1.value()));

        auto chainIt = std::find_if(doc.chains.begin(), doc.chains.end(),
            [chainId](const project::Chain& c) { return c.chainId == chainId; });
        if (chainIt == doc.chains.end()) {
            doc.chains.push_back(std::move(masteringChain));
        } else {
            *chainIt = std::move(masteringChain);
        }
    }
    return project::ProjectSnapshot::create(std::move(doc), std::move(optional));
}

void ProjectSessionViewModel::openProject(const QUrl& selectedFile)
{
    auto reference = read_reference(selectedFile);
    if (!reference) { publish_error(*reference.error()); return; }
    auto reader = platform::windows::WindowsResourceReader::open_read_only(*reference.value());
    if (!reader) { publish_error(*reader.error()); return; }
    auto opened = project::ProjectRepository::open(std::move(*reader.value()));
    if (!opened) { publish_error(*opened.error()); return; }
    const auto& doc = opened.value()->document();
    auto sourceRef = external_reference(doc, doc.sourceResourceId);
    if (!sourceRef) { publish_error(*sourceRef.error()); return; }
    auto sourceCandidate = probe(*sourceRef.value());
    if (!sourceCandidate) { publish_error(*sourceCandidate.error()); return; }
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
    for (const auto& persisted : doc.timeSelections.auditionRegions) {
        if (persisted.resourceId != doc.sourceResourceId ||
            persisted.endFrameExclusive >
                sourceCandidate.value()->wav_info().frame_count().value()) {
            publish_error(invalid("timeSelections.auditionRegions")); return;
        }
    }
    if (!doc.timeSelections.auditionRegions.empty()) {
        const auto& persisted = doc.timeSelections.auditionRegions.front();
        auto range = core::FrameRange::create(core::FrameIndex{persisted.startFrame},
            core::FrameIndex{persisted.endFrameExclusive});
        if (!range) { publish_error(*range.error()); return; }
        regionCandidate = *range.value();
    }
    auto stopped = playback_->stop_and_clear();
    if (!stopped) { publish_error(*stopped.error()); return; }
    source_->adopt_probed_source(std::move(*sourceCandidate.value()));
    if (goldCandidate) {
        auto accepted = gold_->adopt_probed_gold(std::move(*goldCandidate));
        if (!accepted) { publish_error(*accepted.error()); return; }
    } else if (gold_->has_gold()) gold_->clearGold();
    if (regionCandidate) {
        auto accepted = region_->set_region(*regionCandidate);
        if (!accepted) { publish_error(*accepted.error()); return; }
    }
    const auto persistedProjectId = doc.projectId;
    const auto persistedSourceId = doc.sourceResourceId;
    const auto persistedReferenceId = doc.references.activeReferenceId;
    const auto persistedRegionId = doc.timeSelections.auditionRegions.empty() ?
        std::optional<core::Uuid>{} :
        std::optional<core::Uuid>{doc.timeSelections.auditionRegions.front().regionId};
    const auto persistedName = doc.displayName;
    const bool isDegraded = has_future_semantics(doc);
    opened_.emplace(std::move(*opened.value()));
    projectId_ = persistedProjectId;
    sourceId_ = persistedSourceId;
    referenceId_ = persistedReferenceId;
    goldId_ = goldResourceId;
    regionId_ = persistedRegionId;
    clearedRegionId_.reset();
    lastSource_ = source_->source_resource()->reference();
    lastGold_ = gold_->gold_resource() ?
        std::optional{gold_->gold_resource()->reference()} : std::nullopt;
    degraded_ = isDegraded;
    projectDisplayName_ = QString::fromUtf8(persistedName.data(),
        static_cast<qsizetype>(persistedName.size()));
    statusText_ = degraded_ ? QStringLiteral("Project opened in degraded mode; unsupported processing is not active.") :
                             QStringLiteral("Project opened.");
    clear_error();
}

void ProjectSessionViewModel::saveProjectAs(const QUrl& selectedFile)
{
    if (!selectedFile.isValid() || !selectedFile.isLocalFile() ||
        !selectedFile.toLocalFile().endsWith(QStringLiteral(".rgsml"), Qt::CaseInsensitive)) {
        publish_error(invalid("destination")); return;
    }
    const QFileInfo file{selectedFile.toLocalFile()};
    auto snapshot = current_snapshot(file.completeBaseName());
    if (!snapshot) { publish_error(*snapshot.error()); return; }
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
    opened_.emplace(std::move(*snapshot.value()));
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
