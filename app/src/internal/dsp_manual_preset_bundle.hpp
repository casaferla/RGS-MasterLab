#pragma once
// User-owned DSP presets, independent of L3 M12 calculated REF plans.
// A preset is a REUSABLE copy of real MANUAL module settings, not an
// AutoMatch adoption decision, Project transaction, or M13 verification.
// Both single-module and whole-chain presets use the ONE registered
// parameter codec. No new DSP engine, no M17 WAV-export controls.
// A chain preset includes bypass/activation for EVERY existing MANUAL
// DSP, including modules that a future user-approved Match may replace.
// This file is read-only capture/validation; applying requires an
// explicit Product owner transaction and an atomic GUI history update.
#include <rgsml/dsp/module_parameter_codec.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/project/opaque_json_value.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QString>
#include <QByteArray>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::dsp_presets {
enum class Scope:std::uint8_t { module,manual_chain };
struct ModuleSettings final {
    std::string type_id;
    std::string algorithm_version;
    std::string parameter_schema_id;
    dsp::ModuleParameterPayload parameters;
    bool enabled{true};
    bool user_bypass{false};
};
struct ManualPreset final {
    Scope scope{Scope::manual_chain};
    std::vector<ModuleSettings> modules;
    [[nodiscard]] constexpr bool authorizes_automatch_overwrite()
        const noexcept{return false;}
    [[nodiscard]] constexpr bool authorizes_project_commit()
        const noexcept{return false;}
};
[[nodiscard]] inline core::Result<ManualPreset> capture(
    const dsp::ProcessingChain& manual,
    const std::vector<dsp::ModuleExecutionBinding>& bindings,
    const dsp::ModuleRegistry& registry,
    std::optional<std::size_t> exact_module_index=std::nullopt)
{
    using R=core::Result<ManualPreset>;
    const auto fail=[](const char* why){
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    const auto nodes=manual.instances();
    if(manual.context()!=dsp::ProcessingChainContext{
          dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL}||
       nodes.empty()||nodes.size()!=bindings.size()||
       (exact_module_index&&*exact_module_index>=nodes.size()))
        return fail("DSP_PRESET_MISSING_OR_INVALID_MANUAL_CHAIN");
    ManualPreset out;
    out.scope=exact_module_index?Scope::module:Scope::manual_chain;
    const auto from=exact_module_index?*exact_module_index:0U;
    const auto to=exact_module_index?from+1U:nodes.size();
    for(std::size_t i=from;i<to;++i) {
        const auto& n=nodes[i];
        if(n.instance_id()!=bindings[i].instance_id||
           n.provenance()!=dsp::ModuleProvenance::MANUAL||
           n.owner()!=dsp::ModuleOwner::USER||
           n.link_state()!=dsp::ModuleLinkState::UNLINKED||
           n.semantic_node_id().has_value()||
           n.controller_suspended()||n.domain_suspended())
            return fail("DSP_PRESET_PROTECTED_OR_STALE_MODULE");
        const auto descriptor=registry.find_descriptor(n.module_type_id());
        if(!descriptor)return R::failure(*descriptor.error());
        const auto& d=descriptor.value()->get();
        if(!d.algorithm_version()||!d.parameter_schema_id()||
           !registry.has_factory(n.module_type_id()))
            return fail("DSP_PRESET_FACTORY_OR_SCHEMA_UNAVAILABLE");
        const auto encoded=dsp::encode_module_parameters_json(
            bindings[i].parameters);
        if(!encoded)return R::failure(*encoded.error());
        const auto checked=dsp::decode_module_parameters_json(
            *d.parameter_schema_id(),*encoded.value());
        if(!checked)return R::failure(*checked.error());
        if(*checked.value()!=bindings[i].parameters)
            return fail("DSP_PRESET_TYPED_BINDING_NOT_CANONICAL");
        out.modules.push_back({
            std::string(n.module_type_id()),
            std::string(*d.algorithm_version()),
            std::string(*d.parameter_schema_id()),
            bindings[i].parameters,n.enabled(),n.user_bypass()});
    }
    return R::success(std::move(out));
}
[[nodiscard]] inline core::Result<std::string> encode_json(
    const ManualPreset& preset)
{
    using R=core::Result<std::string>;
    if(preset.modules.empty()||
       (preset.scope==Scope::module&&preset.modules.size()!=1U))
        return R::failure(core::Error{
            core::ErrorCode::InvalidArgument,"DSP_PRESET_INVALID_SCOPE"});
    QJsonArray modules;
    for(const auto& m:preset.modules) {
        const auto json=dsp::encode_module_parameters_json(m.parameters);
        if(!json)return R::failure(*json.error());
        QJsonParseError err;
        const auto d=QJsonDocument::fromJson(
            QByteArray::fromStdString(*json.value()),&err);
        if(err.error!=QJsonParseError::NoError||!d.isObject())
            return R::failure(core::Error{
                core::ErrorCode::InvalidState,
                "DSP_PRESET_PARAMETER_CODEC_NOT_OBJECT"});
        QJsonObject item;
        item.insert("typeId",QString::fromStdString(m.type_id));
        item.insert("algorithmVersion",
                    QString::fromStdString(m.algorithm_version));
        item.insert("parameterSchemaId",
                    QString::fromStdString(m.parameter_schema_id));
        item.insert("enabled",m.enabled);
        item.insert("userBypass",m.user_bypass);
        item.insert("parameters",d.object());
        modules.append(item);
    }
    QJsonObject root;
    root.insert("schemaId","rgsml.dsp.manual-preset/1.0.0");
    root.insert("scope",preset.scope==Scope::module?
                "MODULE":"MANUAL_CHAIN");
    root.insert("modules",modules);
    return R::success(
        QJsonDocument(root).toJson(QJsonDocument::Compact).toStdString());
}
[[nodiscard]] inline core::Result<ManualPreset> decode_json(
    const std::string& raw,const dsp::ModuleRegistry& registry)
{
    using R=core::Result<ManualPreset>;
    const auto fail=[](const char* why){
        return R::failure(core::Error{core::ErrorCode::InvalidArgument,why});
    };
    if(raw.empty()||raw.size()>4U*1024U*1024U)
        return fail("DSP_PRESET_FILE_SIZE_INVALID");
    QJsonParseError err;
    const auto json=QJsonDocument::fromJson(
        QByteArray::fromStdString(raw),&err);
    if(err.error!=QJsonParseError::NoError||!json.isObject())
        return fail("DSP_PRESET_JSON_INVALID");
    const auto root=json.object();
    if(root.size()!=3||
       root.value("schemaId").toString()!="rgsml.dsp.manual-preset/1.0.0"||
       !root.value("scope").isString()||
       !root.value("modules").isArray())
        return fail("DSP_PRESET_SCHEMA_OR_KEYS_INVALID");
    const auto label=root.value("scope").toString();
    if(label!="MODULE"&&label!="MANUAL_CHAIN")
        return fail("DSP_PRESET_SCOPE_INVALID");
    ManualPreset preset;
    preset.scope=label=="MODULE"?Scope::module:Scope::manual_chain;
    const auto modules=root.value("modules").toArray();
    if(modules.isEmpty()||modules.size()>48||
       (preset.scope==Scope::module&&modules.size()!=1))
        return fail("DSP_PRESET_MODULE_COUNT_INVALID");
    for(const auto& rawModule:modules) {
        if(!rawModule.isObject())return fail("DSP_PRESET_MODULE_INVALID");
        const auto obj=rawModule.toObject();
        if(obj.size()!=6||
           !obj.value("typeId").isString()||
           !obj.value("algorithmVersion").isString()||
           !obj.value("parameterSchemaId").isString()||
           !obj.value("enabled").isBool()||
           !obj.value("userBypass").isBool()||
           !obj.value("parameters").isObject())
            return fail("DSP_PRESET_MODULE_SCHEMA_INVALID");
        const auto type=obj.value("typeId").toString().toStdString();
        const auto desc=registry.find_descriptor(type);
        if(!desc)return R::failure(*desc.error());
        const auto& d=desc.value()->get();
        if(!d.algorithm_version()||!d.parameter_schema_id()||
           !registry.has_factory(type)||
           obj.value("algorithmVersion").toString().toStdString()!=
               *d.algorithm_version()||
           obj.value("parameterSchemaId").toString().toStdString()!=
               *d.parameter_schema_id())
            return fail("DSP_PRESET_MODULE_RUNTIME_VERSION_INCOMPATIBLE");
        const auto parameters=QJsonDocument(
            obj.value("parameters").toObject()).toJson(
                QJsonDocument::Compact).toStdString();
        const auto parsed=dsp::decode_module_parameters_json(
            *d.parameter_schema_id(),parameters);
        if(!parsed)return R::failure(*parsed.error());
        preset.modules.push_back({
            type,std::string(*d.algorithm_version()),
            std::string(*d.parameter_schema_id()),
            *parsed.value(),obj.value("enabled").toBool(),
            obj.value("userBypass").toBool()});
    }
    return R::success(std::move(preset));
}
// Strict preflight for a later, explicitly user-approved atomic
// restore. Match CURRENT DSP topology in order; never allocate new
// ModuleInstance IDs or adopt/replace linked, manual or terminal DSP.
[[nodiscard]] inline bool matches_exact_manual_topology(
    const ManualPreset& preset,
    const dsp::ProcessingChain& current,
    std::optional<std::size_t> module_index=std::nullopt)
{
    const auto nodes=current.instances();
    if(current.context()!=dsp::ProcessingChainContext{
          dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL})
        return false;
    if((preset.scope==Scope::module)!=(module_index.has_value()))
        return false;
    if(module_index&&(*module_index>=nodes.size()||
                      preset.modules.size()!=1U))
        return false;
    if(!module_index&&preset.modules.size()!=nodes.size())
        return false;
    for(std::size_t i=0;i<preset.modules.size();++i) {
        const auto& n=nodes[module_index?*module_index:i];
        const auto& p=preset.modules[i];
        if(n.module_type_id()!=p.type_id||
           n.provenance()!=dsp::ModuleProvenance::MANUAL||
           n.owner()!=dsp::ModuleOwner::USER||
           n.link_state()!=dsp::ModuleLinkState::UNLINKED)
            return false;
    }
    return true;
}
} // namespace rgsml::app::dsp_presets
