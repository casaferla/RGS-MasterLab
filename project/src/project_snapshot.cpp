#include <rgsml/project/project_snapshot.hpp>

#include "internal/raw_zip_validator.hpp"

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <array>
#include <set>
#include <string_view>

namespace rgsml::project {
namespace {

[[nodiscard]] core::Result<ProjectSnapshot> invalid(std::string_view field)
{
    return core::Result<ProjectSnapshot>::failure(core::Error{
        core::ErrorCode::InvalidArgument, "Invalid project snapshot",
        {{"field", std::string(field)}}});
}

[[nodiscard]] bool safe_optional_path(std::string_view path)
{
    constexpr std::array<std::string_view, 3> roots{
        "artifacts/", "definitions/", "thumbnails/"};
    if (path.empty() || path.size() > 512 ||
        std::none_of(roots.begin(), roots.end(),
            [path](std::string_view root) { return path.starts_with(root); })) {
        return false;
    }
    std::size_t begin = 0;
    while (begin < path.size()) {
        const auto end = path.find('/', begin);
        const auto segment = path.substr(begin, end == std::string_view::npos ? end : end - begin);
        if (segment.empty() || segment == "." || segment == "..") {
            return false;
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    if (path.back() == '/') {
        return false;
    }
    return internal::safe_package_path(path) &&
        std::none_of(path.begin(), path.end(), [](unsigned char c) {
        return c == '\\' || c == ':' || c < 0x20U || c == 0x7fU;
    });
}

}  // namespace

core::Result<ProjectSnapshot> ProjectSnapshot::create(
    ProjectDocument document, std::vector<OptionalEntry> optionalEntries)
{
    if (document.projectId.is_nil() || document.sourceResourceId.is_nil()) {
        return invalid("projectId/source.resourceId");
    }
    if (document.displayName.empty()) {
        return invalid("project.displayName");
    }
    std::set<core::Uuid> resourceIds;
    for (const auto& resource : document.resources) {
        if (resource.resourceId.is_nil() || resource.resourceType != "AUDIO" ||
            resource.locatorHints.empty() || !resourceIds.insert(resource.resourceId).second) {
            return invalid("resources");
        }
        for (const auto& hint : resource.locatorHints) {
            if (hint.providerId.empty() || hint.locator.empty()) {
                return invalid("resources.locatorHints");
            }
        }
    }
    if (!resourceIds.contains(document.sourceResourceId)) {
        return invalid("source.resourceId");
    }
    std::set<core::Uuid> referenceIds;
    for (const auto& reference : document.references.items) {
        if (reference.referenceId.is_nil() || reference.referenceType != "GOLD" ||
            !resourceIds.contains(reference.resourceId) ||
            reference.resourceId == document.sourceResourceId ||
            !referenceIds.insert(reference.referenceId).second) {
            return invalid("references.items");
        }
    }
    if (document.references.activeReferenceId &&
        !referenceIds.contains(*document.references.activeReferenceId)) {
        return invalid("references.activeReferenceId");
    }
    std::set<core::Uuid> regionIds;
    for (const auto& region : document.timeSelections.auditionRegions) {
        if (region.regionId.is_nil() || !regionIds.insert(region.regionId).second ||
            region.resourceId != document.sourceResourceId || region.frameDomain != "SOURCE" ||
            region.startFrame < 0 || region.endFrameExclusive <= region.startFrame) {
            return invalid("timeSelections.auditionRegions");
        }
    }
    std::set<core::Uuid> chainIds;
    std::set<core::Uuid> instanceIds;
    constexpr std::array<std::string_view, 2> stages{"RESTORE_PREP", "MASTER"};
    constexpr std::array<std::string_view, 6> segments{
        "REPAIR", "PRE_MASTER_CONDITIONING", "MANUAL", "DNA_LINKED", "REF_LINKED", "TERMINAL"};
    for (const auto& chain : document.chains) {
        if (chain.chainId.is_nil() || !chainIds.insert(chain.chainId).second ||
            std::find(stages.begin(), stages.end(), chain.stage) == stages.end() ||
            std::find(segments.begin(), segments.end(), chain.segment) == segments.end()) {
            return invalid("chains");
        }
        for (const auto& module : chain.modules) {
            if (module.instanceId.is_nil() || !instanceIds.insert(module.instanceId).second ||
                module.typeId.empty()) {
                return invalid("chains.modules");
            }
        }
    }
    for (const auto& id : {document.pipeline.repairChainId,
                           document.pipeline.conditioningChainId,
                           document.pipeline.masteringChainId}) {
        if (id && !chainIds.contains(*id)) {
            return invalid("pipeline");
        }
    }
    std::set<std::string> paths;
    std::uint64_t aggregate = 0;
    for (const auto& entry : optionalEntries) {
        if (!safe_optional_path(entry.path) || !paths.insert(entry.path).second ||
            entry.mediaType.empty() || entry.bytes.size() > 64U * 1024U * 1024U ||
            aggregate > 256U * 1024U * 1024U - entry.bytes.size()) {
            return invalid("optionalEntries");
        }
        aggregate += entry.bytes.size();
    }
    return core::Result<ProjectSnapshot>::success(
        ProjectSnapshot{std::move(document), std::move(optionalEntries)});
}

}  // namespace rgsml::project
