#pragma once

#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/dsp/module_instance.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::tests::dsp_support {

[[nodiscard]] inline rgsml::dsp::ModuleInstanceId make_id(std::string_view text)
{
    auto uuid = rgsml::core::Uuid::parse(text);
    auto id = rgsml::dsp::ModuleInstanceId::from_uuid(*uuid.value());
    return *id.value();
}

[[nodiscard]] inline rgsml::dsp::ModuleDescriptor make_descriptor(
    std::string type_id,
    rgsml::dsp::ProcessingStage stage = rgsml::dsp::ProcessingStage::MASTER,
    rgsml::dsp::ChainSegment segment = rgsml::dsp::ChainSegment::MANUAL,
    bool duplicable = true,
    bool bypassable = true,
    bool single_active = false,
    std::vector<std::string> must_precede = {},
    std::vector<std::string> must_follow = {},
    std::vector<std::string> recommended_before = {},
    bool must_be_last = false)
{
    const auto display_name_key = type_id + ".display-name";
    auto descriptor = rgsml::dsp::ModuleDescriptor::create(
        rgsml::dsp::ModuleDescriptorSpec{
            std::move(type_id),
            display_name_key,
            {rgsml::dsp::ModuleCategory::UTILITY},
            {stage},
            {segment},
            duplicable,
            bypassable,
            rgsml::dsp::PlacementClass::INLINE_CHAIN,
            std::nullopt,
            std::move(must_precede),
            std::move(must_follow),
            std::move(recommended_before),
            {},
            must_be_last,
            single_active,
            std::nullopt,
            std::nullopt});
    return std::move(*descriptor.value());
}

[[nodiscard]] inline std::string_view error_category(const rgsml::core::Error& error)
{
    for (const auto& detail : error.details()) {
        if (detail.key == "category") {
            return detail.value;
        }
    }
    return {};
}

}  // namespace rgsml::tests::dsp_support
