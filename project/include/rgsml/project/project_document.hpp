#pragma once

#include <rgsml/core/uuid.hpp>
#include <rgsml/project/opaque_json_value.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rgsml::project {

struct LocatorHint final {
    std::string providerId;
    std::string locator;
    std::string displayName;
};

struct Resource final {
    core::Uuid resourceId;
    std::string resourceType{"AUDIO"};
    std::vector<LocatorHint> locatorHints;
    std::optional<OpaqueJsonValue> identity;
};

struct Reference final {
    core::Uuid referenceId;
    std::string referenceType{"GOLD"};
    core::Uuid resourceId;
};

struct References final {
    std::optional<core::Uuid> activeReferenceId;
    std::vector<Reference> items;
};

struct AuditionRegion final {
    core::Uuid regionId;
    core::Uuid resourceId;
    std::string frameDomain{"SOURCE"};
    std::int64_t startFrame{0};
    std::int64_t endFrameExclusive{0};
};

struct TimeSelections final {
    std::vector<AuditionRegion> auditionRegions;
    OpaqueJsonValue analysisScopes{OpaqueJsonValue::empty_array()};
    OpaqueJsonValue processingRegions{OpaqueJsonValue::empty_array()};
};

struct Module final {
    core::Uuid instanceId;
    std::string typeId;
    bool enabled{true};
    bool userBypass{false};
    bool controllerSuspended{false};
    bool domainSuspended{false};
    std::string provenance{"MANUAL"};
    std::string owner{"USER"};
    std::string linkState{"UNLINKED"};
    std::optional<core::Uuid> semanticNodeId;
    std::optional<std::string> algorithmVersion;
    std::optional<std::string> parameterSchemaId;
    OpaqueJsonValue parameters{OpaqueJsonValue::empty_object()};
};

struct Chain final {
    core::Uuid chainId;
    std::string stage;
    std::string segment;
    std::uint64_t revision{0};
    std::vector<Module> modules;
};

struct Pipeline final {
    std::optional<core::Uuid> repairChainId;
    std::optional<core::Uuid> conditioningChainId;
    std::optional<core::Uuid> masteringChainId;
};

struct ProjectDocument final {
    core::Uuid projectId;
    std::string displayName;
    std::vector<Resource> resources;
    core::Uuid sourceResourceId;
    References references;
    TimeSelections timeSelections;
    std::vector<Chain> chains;
    Pipeline pipeline;
    std::optional<OpaqueJsonValue> activeProfileApplication;
    OpaqueJsonValue analyses{OpaqueJsonValue::empty_array()};
    OpaqueJsonValue plans{OpaqueJsonValue::empty_array()};
    OpaqueJsonValue referenceMatch{OpaqueJsonValue::empty_object()};
    OpaqueJsonValue masteringDNA{OpaqueJsonValue::empty_object()};
    OpaqueJsonValue output{OpaqueJsonValue::empty_object()};
    OpaqueJsonValue derivedArtifacts{OpaqueJsonValue::empty_array()};
    OpaqueJsonValue history{OpaqueJsonValue::empty_array()};
    OpaqueJsonValue extensions{OpaqueJsonValue::empty_object()};
};

}  // namespace rgsml::project
