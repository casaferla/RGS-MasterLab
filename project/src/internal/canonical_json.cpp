#include "canonical_json.hpp"

#include <nlohmann/json.hpp>

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace rgsml::project::internal {
namespace {

using Json = nlohmann::json;
constexpr std::size_t kProjectMax = 16U * 1024U * 1024U;
constexpr std::size_t kDepthMax = 64U;
constexpr std::size_t kEventsMax = 1'000'000U;

struct SchemaFailure final {
    std::string field;
};

[[noreturn]] void bad(std::string field) { throw SchemaFailure{std::move(field)}; }

class BoundedSax final : public nlohmann::json_sax<Json> {
public:
    bool null() override { return event(); }
    bool boolean(bool) override { return event(); }
    bool number_integer(number_integer_t) override { return event(); }
    bool number_unsigned(number_unsigned_t) override { return event(); }
    bool number_float(number_float_t value, const string_t& raw) override
    {
        return std::isfinite(value) && raw.find_first_of(".eE") != std::string::npos && event();
    }
    bool string(string_t& value) override { return value.size() <= kProjectMax && event(); }
    bool binary(binary_t&) override { return false; }
    bool start_object(std::size_t) override
    {
        if (!event() || stack_.size() >= kDepthMax) return false;
        stack_.emplace_back(std::unordered_set<std::string>{});
        return true;
    }
    bool key(string_t& value) override
    {
        return !stack_.empty() && stack_.back().has_value()
            && value.size() <= kProjectMax && stack_.back()->insert(value).second && event();
    }
    bool end_object() override
    {
        if (stack_.empty() || !stack_.back()) return false;
        stack_.pop_back();
        return true;
    }
    bool start_array(std::size_t) override
    {
        if (!event() || stack_.size() >= kDepthMax) return false;
        stack_.emplace_back(std::nullopt);
        return true;
    }
    bool end_array() override
    {
        if (stack_.empty() || stack_.back()) return false;
        stack_.pop_back();
        return true;
    }
    bool parse_error(std::size_t, const std::string&,
                     const nlohmann::detail::exception&) override { return false; }

private:
    bool event() { return ++events_ <= kEventsMax; }
    std::size_t events_{0};
    std::vector<std::optional<std::unordered_set<std::string>>> stack_;
};

[[nodiscard]] core::Result<Json> strict_parse(std::string_view bytes, std::size_t maxBytes)
{
    if (bytes.size() > maxBytes) {
        return core::Result<Json>::failure(core::Error{core::ErrorCode::OutOfRange,
            "JSON size limit", {{"phase", "json_sax"}}});
    }
    if (bytes.size() >= 3U && bytes.substr(0, 3) == "\xEF\xBB\xBF") {
        return core::Result<Json>::failure(core::Error{core::ErrorCode::ParseFailure,
            "JSON BOM forbidden", {{"phase", "json_sax"}}});
    }
    BoundedSax sax;
    if (!Json::sax_parse(bytes.begin(), bytes.end(), &sax,
                         Json::input_format_t::json, true, false)) {
        return core::Result<Json>::failure(core::Error{core::ErrorCode::ParseFailure,
            "Strict JSON SAX validation failed", {{"phase", "json_sax"}}});
    }
    try {
        return core::Result<Json>::success(
            Json::parse(bytes.begin(), bytes.end(), nullptr, true, false));
    } catch (const Json::exception&) {
        return core::Result<Json>::failure(core::Error{core::ErrorCode::ParseFailure,
            "JSON DOM parse failed", {{"phase", "json_dom"}}});
    }
}

[[nodiscard]] bool finite(const Json& j)
{
    if (j.is_number_float()) return std::isfinite(j.get<double>());
    if (j.is_array()) {
        for (const auto& item : j) if (!finite(item)) return false;
    } else if (j.is_object()) {
        for (auto it = j.begin(); it != j.end(); ++it) if (!finite(it.value())) return false;
    }
    return true;
}

[[nodiscard]] std::string dump(const Json& j)
{
    if (!finite(j)) bad("nonfinite");
    return j.dump(-1, ' ', false, Json::error_handler_t::strict);
}

void keys(const Json& value, std::initializer_list<std::string_view> expected,
          std::string_view field)
{
    if (!value.is_object() || value.size() != expected.size()) bad(std::string(field));
    for (const auto key : expected) if (!value.contains(std::string(key))) bad(std::string(field));
}

[[nodiscard]] const Json& at(const Json& j, std::string_view key, std::string_view field)
{
    if (!j.is_object() || !j.contains(std::string(key))) bad(std::string(field));
    return j.at(std::string(key));
}

[[nodiscard]] std::string str(const Json& j, std::string_view field)
{
    if (!j.is_string()) bad(std::string(field));
    return j.get<std::string>();
}

[[nodiscard]] std::string token(const Json& j, std::string_view expected, std::string_view field)
{
    const auto value = str(j, field);
    if (value != expected) bad(std::string(field));
    return value;
}

[[nodiscard]] bool boolean(const Json& j, std::string_view field)
{
    if (!j.is_boolean()) bad(std::string(field));
    return j.get<bool>();
}

[[nodiscard]] core::Uuid uuid(const Json& j, std::string_view field)
{
    const auto text = str(j, field);
    const auto parsed = core::Uuid::parse(text);
    if (!parsed || parsed.value()->is_nil() || parsed.value()->to_string() != text) {
        bad(std::string(field));
    }
    return *parsed.value();
}

[[nodiscard]] std::optional<core::Uuid> optional_uuid(const Json& j, std::string_view field)
{
    if (j.is_null()) return std::nullopt;
    return uuid(j, field);
}

[[nodiscard]] std::optional<std::string> optional_string(const Json& j, std::string_view field)
{
    if (j.is_null()) return std::nullopt;
    return str(j, field);
}

[[nodiscard]] std::int64_t i64(const Json& j, std::string_view field)
{
    if (!j.is_number_integer() || j.is_number_float()) bad(std::string(field));
    if (j.is_number_unsigned()) {
        const auto n = j.get<std::uint64_t>();
        if (n > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            bad(std::string(field));
        }
        return static_cast<std::int64_t>(n);
    }
    return j.get<std::int64_t>();
}

[[nodiscard]] std::uint64_t u64(const Json& j, std::string_view field)
{
    if (!j.is_number_integer() || j.is_number_float()) bad(std::string(field));
    if (j.is_number_unsigned()) return j.get<std::uint64_t>();
    const auto n = j.get<std::int64_t>();
    if (n < 0) bad(std::string(field));
    return static_cast<std::uint64_t>(n);
}

[[nodiscard]] OpaqueJsonValue opaque(const Json& j)
{
    const auto parsed = OpaqueJsonValue::parse(dump(j));
    if (!parsed) bad("opaque");
    return *parsed.value();
}

[[nodiscard]] Json from_opaque(const OpaqueJsonValue& value)
{
    auto parsed = strict_parse(value.canonical_utf8(), kProjectMax);
    if (!parsed) bad("opaque");
    return std::move(*parsed.value());
}

[[nodiscard]] Json id_json(const std::optional<core::Uuid>& id)
{
    return id ? Json(id->to_string()) : Json(nullptr);
}

[[nodiscard]] ProjectDocument read_document(const Json& root)
{
    keys(root, {"format", "project", "resources", "source", "references",
        "timeSelections", "chains", "pipeline", "preparation", "analyses", "plans",
        "referenceMatch", "masteringDNA", "output", "derivedArtifacts", "history",
        "extensions"}, "root");
    const auto& format = at(root, "format", "format");
    keys(format, {"formatId", "containerVersion", "schemaId", "schemaVersion"}, "format");
    (void)token(at(format, "formatId", "format.formatId"), "rgsml-project", "format.formatId");
    if (u64(at(format, "containerVersion", "format.containerVersion"),
            "format.containerVersion") != 1U) bad("format.containerVersion");
    (void)token(at(format, "schemaId", "format.schemaId"),
        "urn:rgsml:schema:project:1.0.0", "format.schemaId");
    (void)token(at(format, "schemaVersion", "format.schemaVersion"),
        "1.0.0", "format.schemaVersion");
    ProjectDocument doc;
    const auto& project = at(root, "project", "project");
    keys(project, {"projectId", "displayName"}, "project");
    doc.projectId = uuid(at(project, "projectId", "project.projectId"), "project.projectId");
    doc.displayName = str(at(project, "displayName", "project.displayName"),
        "project.displayName");
    const auto& resources = at(root, "resources", "resources");
    if (!resources.is_array()) bad("resources");
    for (const auto& item : resources) {
        if (!item.is_object() || (item.size() != 3U && item.size() != 4U) ||
            !item.contains("resourceId") || !item.contains("resourceType") ||
            !item.contains("locatorHints") ||
            (item.size() == 4U && !item.contains("identity"))) bad("resources[]");
        Resource resource;
        resource.resourceId = uuid(at(item, "resourceId", "resources.resourceId"),
            "resources.resourceId");
        resource.resourceType = token(at(item, "resourceType", "resources.resourceType"),
            "AUDIO", "resources.resourceType");
        const auto& hints = at(item, "locatorHints", "resources.locatorHints");
        if (!hints.is_array()) bad("resources.locatorHints");
        for (const auto& h : hints) {
            keys(h, {"providerId", "locator", "displayName"}, "locatorHints[]");
            resource.locatorHints.push_back({
                str(at(h, "providerId", "locatorHints.providerId"), "locatorHints.providerId"),
                str(at(h, "locator", "locatorHints.locator"), "locatorHints.locator"),
                str(at(h, "displayName", "locatorHints.displayName"), "locatorHints.displayName")});
        }
        if (item.contains("identity")) resource.identity = opaque(item.at("identity"));
        doc.resources.push_back(std::move(resource));
    }
    const auto& source = at(root, "source", "source");
    keys(source, {"resourceId"}, "source");
    doc.sourceResourceId = uuid(at(source, "resourceId", "source.resourceId"), "source.resourceId");
    const auto& references = at(root, "references", "references");
    keys(references, {"activeReferenceId", "items"}, "references");
    doc.references.activeReferenceId = optional_uuid(
        at(references, "activeReferenceId", "references.activeReferenceId"),
        "references.activeReferenceId");
    const auto& refs = at(references, "items", "references.items");
    if (!refs.is_array()) bad("references.items");
    for (const auto& r : refs) {
        keys(r, {"referenceId", "referenceType", "resourceId"}, "references.items[]");
        doc.references.items.push_back({
            uuid(at(r, "referenceId", "referenceId"), "referenceId"),
            token(at(r, "referenceType", "referenceType"), "GOLD", "referenceType"),
            uuid(at(r, "resourceId", "resourceId"), "resourceId")});
    }
    const auto& time = at(root, "timeSelections", "timeSelections");
    keys(time, {"auditionRegions", "analysisScopes", "processingRegions"}, "timeSelections");
    const auto& regions = at(time, "auditionRegions", "auditionRegions");
    if (!regions.is_array()) bad("auditionRegions");
    for (const auto& r : regions) {
        keys(r, {"regionId", "resourceId", "frameDomain", "startFrame", "endFrameExclusive"},
             "auditionRegions[]");
        doc.timeSelections.auditionRegions.push_back({
            uuid(at(r, "regionId", "regionId"), "regionId"),
            uuid(at(r, "resourceId", "resourceId"), "resourceId"),
            token(at(r, "frameDomain", "frameDomain"), "SOURCE", "frameDomain"),
            i64(at(r, "startFrame", "startFrame"), "startFrame"),
            i64(at(r, "endFrameExclusive", "endFrameExclusive"), "endFrameExclusive")});
    }
    if (!at(time, "analysisScopes", "analysisScopes").is_array() ||
        !at(time, "processingRegions", "processingRegions").is_array()) bad("timeSelections");
    doc.timeSelections.analysisScopes = opaque(time.at("analysisScopes"));
    doc.timeSelections.processingRegions = opaque(time.at("processingRegions"));
    const auto& chains = at(root, "chains", "chains");
    if (!chains.is_array()) bad("chains");
    for (const auto& c : chains) {
        keys(c, {"chainId", "stage", "segment", "revision", "modules"}, "chains[]");
        Chain chain;
        chain.chainId = uuid(at(c, "chainId", "chainId"), "chainId");
        chain.stage = str(at(c, "stage", "stage"), "stage");
        chain.segment = str(at(c, "segment", "segment"), "segment");
        chain.revision = u64(at(c, "revision", "revision"), "revision");
        const auto& modules = at(c, "modules", "modules");
        if (!modules.is_array()) bad("modules");
        for (const auto& m : modules) {
            keys(m, {"instanceId", "typeId", "enabled", "userBypass",
                "controllerSuspended", "domainSuspended", "provenance", "owner",
                "linkState", "semanticNodeId", "algorithmVersion", "parameterSchemaId",
                "parameters"}, "modules[]");
            Module module;
            module.instanceId = uuid(at(m, "instanceId", "instanceId"), "instanceId");
            module.typeId = str(at(m, "typeId", "typeId"), "typeId");
            module.enabled = boolean(at(m, "enabled", "enabled"), "enabled");
            module.userBypass = boolean(at(m, "userBypass", "userBypass"), "userBypass");
            module.controllerSuspended = boolean(
                at(m, "controllerSuspended", "controllerSuspended"), "controllerSuspended");
            module.domainSuspended = boolean(
                at(m, "domainSuspended", "domainSuspended"), "domainSuspended");
            module.provenance = str(at(m, "provenance", "provenance"), "provenance");
            module.owner = str(at(m, "owner", "owner"), "owner");
            module.linkState = str(at(m, "linkState", "linkState"), "linkState");
            module.semanticNodeId = optional_uuid(
                at(m, "semanticNodeId", "semanticNodeId"), "semanticNodeId");
            module.algorithmVersion = optional_string(
                at(m, "algorithmVersion", "algorithmVersion"), "algorithmVersion");
            module.parameterSchemaId = optional_string(
                at(m, "parameterSchemaId", "parameterSchemaId"), "parameterSchemaId");
            module.parameters = opaque(at(m, "parameters", "parameters"));
            chain.modules.push_back(std::move(module));
        }
        doc.chains.push_back(std::move(chain));
    }
    const auto& pipeline = at(root, "pipeline", "pipeline");
    keys(pipeline, {"restore", "master"}, "pipeline");
    const auto& restore = at(pipeline, "restore", "pipeline.restore");
    keys(restore, {"repairChainId", "conditioningChainId"}, "pipeline.restore");
    const auto& master = at(pipeline, "master", "pipeline.master");
    keys(master, {"masteringChainId"}, "pipeline.master");
    doc.pipeline.repairChainId = optional_uuid(
        at(restore, "repairChainId", "repairChainId"), "repairChainId");
    doc.pipeline.conditioningChainId = optional_uuid(
        at(restore, "conditioningChainId", "conditioningChainId"), "conditioningChainId");
    doc.pipeline.masteringChainId = optional_uuid(
        at(master, "masteringChainId", "masteringChainId"), "masteringChainId");
    const auto& prep = at(root, "preparation", "preparation");
    keys(prep, {"activeProfileApplication"}, "preparation");
    if (!prep.at("activeProfileApplication").is_null()) {
        doc.activeProfileApplication = opaque(prep.at("activeProfileApplication"));
    }
    if (!root.at("analyses").is_array() || !root.at("plans").is_array() ||
        !root.at("referenceMatch").is_object() || !root.at("masteringDNA").is_object() ||
        !root.at("output").is_object() || !root.at("derivedArtifacts").is_array() ||
        !root.at("history").is_array() || !root.at("extensions").is_object()) bad("futureSections");
    doc.analyses = opaque(root.at("analyses"));
    doc.plans = opaque(root.at("plans"));
    doc.referenceMatch = opaque(root.at("referenceMatch"));
    doc.masteringDNA = opaque(root.at("masteringDNA"));
    doc.output = opaque(root.at("output"));
    doc.derivedArtifacts = opaque(root.at("derivedArtifacts"));
    doc.history = opaque(root.at("history"));
    doc.extensions = opaque(root.at("extensions"));
    return doc;
}

[[nodiscard]] Json write_document(const ProjectDocument& doc)
{
    Json resources = Json::array();
    for (const auto& r : doc.resources) {
        Json hints = Json::array();
        for (const auto& h : r.locatorHints) {
            hints.push_back({{"providerId", h.providerId}, {"locator", h.locator},
                             {"displayName", h.displayName}});
        }
        Json item = {{"resourceId", r.resourceId.to_string()},
                     {"resourceType", r.resourceType}, {"locatorHints", std::move(hints)}};
        if (r.identity) item["identity"] = from_opaque(*r.identity);
        resources.push_back(std::move(item));
    }
    Json references = Json::array();
    for (const auto& r : doc.references.items) {
        references.push_back({{"referenceId", r.referenceId.to_string()},
                              {"referenceType", r.referenceType},
                              {"resourceId", r.resourceId.to_string()}});
    }
    Json regions = Json::array();
    for (const auto& r : doc.timeSelections.auditionRegions) {
        regions.push_back({{"regionId", r.regionId.to_string()},
                           {"resourceId", r.resourceId.to_string()},
                           {"frameDomain", r.frameDomain},
                           {"startFrame", r.startFrame},
                           {"endFrameExclusive", r.endFrameExclusive}});
    }
    Json chains = Json::array();
    for (const auto& c : doc.chains) {
        Json modules = Json::array();
        for (const auto& m : c.modules) {
            modules.push_back({
                {"instanceId", m.instanceId.to_string()}, {"typeId", m.typeId},
                {"enabled", m.enabled}, {"userBypass", m.userBypass},
                {"controllerSuspended", m.controllerSuspended},
                {"domainSuspended", m.domainSuspended}, {"provenance", m.provenance},
                {"owner", m.owner}, {"linkState", m.linkState},
                {"semanticNodeId", id_json(m.semanticNodeId)},
                {"algorithmVersion", m.algorithmVersion ? Json(*m.algorithmVersion) : Json(nullptr)},
                {"parameterSchemaId", m.parameterSchemaId ? Json(*m.parameterSchemaId) : Json(nullptr)},
                {"parameters", from_opaque(m.parameters)}});
        }
        chains.push_back({{"chainId", c.chainId.to_string()}, {"stage", c.stage},
                          {"segment", c.segment}, {"revision", c.revision},
                          {"modules", std::move(modules)}});
    }
    return Json{
        {"format", {{"formatId", "rgsml-project"}, {"containerVersion", 1},
                    {"schemaId", "urn:rgsml:schema:project:1.0.0"},
                    {"schemaVersion", "1.0.0"}}},
        {"project", {{"projectId", doc.projectId.to_string()},
                     {"displayName", doc.displayName}}},
        {"resources", std::move(resources)},
        {"source", {{"resourceId", doc.sourceResourceId.to_string()}}},
        {"references", {{"activeReferenceId", id_json(doc.references.activeReferenceId)},
                         {"items", std::move(references)}}},
        {"timeSelections", {{"auditionRegions", std::move(regions)},
                             {"analysisScopes", from_opaque(doc.timeSelections.analysisScopes)},
                             {"processingRegions", from_opaque(doc.timeSelections.processingRegions)}}},
        {"chains", std::move(chains)},
        {"pipeline", {{"restore", {{"repairChainId", id_json(doc.pipeline.repairChainId)},
                                    {"conditioningChainId", id_json(doc.pipeline.conditioningChainId)}}},
                      {"master", {{"masteringChainId", id_json(doc.pipeline.masteringChainId)}}}}},
        {"preparation", {{"activeProfileApplication",
            doc.activeProfileApplication ? from_opaque(*doc.activeProfileApplication) : Json(nullptr)}}},
        {"analyses", from_opaque(doc.analyses)}, {"plans", from_opaque(doc.plans)},
        {"referenceMatch", from_opaque(doc.referenceMatch)},
        {"masteringDNA", from_opaque(doc.masteringDNA)},
        {"output", from_opaque(doc.output)},
        {"derivedArtifacts", from_opaque(doc.derivedArtifacts)},
        {"history", from_opaque(doc.history)}, {"extensions", from_opaque(doc.extensions)}};
}

}  // namespace

core::Result<std::string> canonicalize_json(std::string_view bytes, std::size_t maxBytes)
{
    auto parsed = strict_parse(bytes, maxBytes);
    if (!parsed) return core::Result<std::string>::failure(*parsed.error());
    try {
        return core::Result<std::string>::success(dump(*parsed.value()));
    } catch (const SchemaFailure&) {
        return core::Result<std::string>::failure(core::Error{
            core::ErrorCode::ParseFailure, "Non-finite JSON value"});
    } catch (const Json::exception&) {
        return core::Result<std::string>::failure(core::Error{
            core::ErrorCode::ParseFailure, "Invalid JSON string encoding"});
    }
}

core::Result<ProjectDocument> parse_project_json(std::string_view bytes)
{
    auto parsed = strict_parse(bytes, kProjectMax);
    if (!parsed) return core::Result<ProjectDocument>::failure(*parsed.error());
    try {
        return core::Result<ProjectDocument>::success(read_document(*parsed.value()));
    } catch (const SchemaFailure& failure) {
        return core::Result<ProjectDocument>::failure(core::Error{
            core::ErrorCode::ParseFailure, "Project schema invalid", {{"field", failure.field}}});
    } catch (const Json::exception&) {
        return core::Result<ProjectDocument>::failure(core::Error{
            core::ErrorCode::ParseFailure, "Project JSON mapping failed"});
    }
}

core::Result<std::string> write_project_json(const ProjectDocument& document)
{
    try {
        auto encoded = dump(write_document(document));
        if (encoded.size() > kProjectMax) {
            return core::Result<std::string>::failure(core::Error{
                core::ErrorCode::OutOfRange, "Project JSON size limit"});
        }
        auto checked = strict_parse(encoded, kProjectMax);
        if (!checked) return core::Result<std::string>::failure(*checked.error());
        return core::Result<std::string>::success(std::move(encoded));
    } catch (const SchemaFailure& failure) {
        return core::Result<std::string>::failure(core::Error{
            core::ErrorCode::InvalidArgument, "Invalid opaque JSON", {{"field", failure.field}}});
    } catch (const Json::exception&) {
        return core::Result<std::string>::failure(core::Error{
            core::ErrorCode::InvalidArgument, "Project JSON serialization failed"});
    }
}

}  // namespace rgsml::project::internal

namespace rgsml::project {

core::Result<OpaqueJsonValue> OpaqueJsonValue::parse(std::string_view utf8)
{
    auto canonical = internal::canonicalize_json(utf8, 16U * 1024U * 1024U);
    if (!canonical) return core::Result<OpaqueJsonValue>::failure(*canonical.error());
    return core::Result<OpaqueJsonValue>::success(OpaqueJsonValue{std::move(*canonical.value())});
}

}  // namespace rgsml::project
