#include <rgsml/dsp/module_descriptor.hpp>

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace rgsml::dsp {
namespace {

using rgsml::core::Error;
using rgsml::core::ErrorCode;
using rgsml::core::Result;

[[nodiscard]] Error descriptor_error(std::string message)
{
    return Error{
        ErrorCode::InvalidArgument,
        std::move(message),
        {{"category", "MODULE_PLACEMENT_INVALID"}}};
}

[[nodiscard]] bool is_stable_ascii(std::string_view value) noexcept
{
    if (value.empty()) {
        return false;
    }
    return std::ranges::all_of(value, [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte >= 0x20U && byte <= 0x7eU;
    });
}

[[nodiscard]] bool is_type_id(std::string_view value) noexcept
{
    constexpr std::string_view prefix = "rgsml.dsp.";
    if (!value.starts_with(prefix) || value.size() == prefix.size()) {
        return false;
    }
    bool segmentHasCharacter = false;
    for (const char character : value.substr(prefix.size())) {
        const bool lower = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        if (lower || digit) {
            segmentHasCharacter = true;
            continue;
        }
        if (character != '-' || !segmentHasCharacter) {
            return false;
        }
        segmentHasCharacter = false;
    }
    return segmentHasCharacter;
}

struct UnsignedAsciiLess final {
    [[nodiscard]] bool operator()(const std::string& left, const std::string& right) const noexcept
    {
        return std::lexicographical_compare(
            left.begin(),
            left.end(),
            right.begin(),
            right.end(),
            [](char lhs, char rhs) {
                return static_cast<unsigned char>(lhs) < static_cast<unsigned char>(rhs);
            });
    }
};

template <typename Enum>
[[nodiscard]] bool canonicalize_enum_set(
    std::vector<Enum>& values,
    std::underlying_type_t<Enum> maximum) noexcept
{
    if (values.empty()) {
        return false;
    }
    for (const auto value : values) {
        const auto raw = static_cast<std::underlying_type_t<Enum>>(value);
        if (raw > maximum) {
            return false;
        }
    }
    std::ranges::sort(values, [](Enum left, Enum right) {
        return static_cast<std::underlying_type_t<Enum>>(left)
            < static_cast<std::underlying_type_t<Enum>>(right);
    });
    return std::adjacent_find(values.begin(), values.end()) == values.end();
}

[[nodiscard]] bool canonicalize_id_set(
    std::vector<std::string>& values,
    std::string_view self) noexcept
{
    for (const auto& value : values) {
        if (!is_type_id(value) || value == self) {
            return false;
        }
    }
    std::ranges::sort(values, UnsignedAsciiLess{});
    return std::adjacent_find(values.begin(), values.end()) == values.end();
}

template <typename T>
[[nodiscard]] bool contains(std::span<const T> values, T sought) noexcept
{
    return std::ranges::find(values, sought) != values.end();
}

[[nodiscard]] bool intersects(
    std::span<const std::string> left,
    std::span<const std::string> right) noexcept
{
    return std::ranges::any_of(left, [&right](const std::string& value) {
        return std::ranges::find(right, value) != right.end();
    });
}

}  // namespace

Result<ModuleDescriptor> ModuleDescriptor::create(ModuleDescriptorSpec spec)
{
    if (!is_type_id(spec.type_id)) {
        return Result<ModuleDescriptor>::failure(
            descriptor_error("Module type ID is not canonical."));
    }
    if (!is_stable_ascii(spec.display_name_key)) {
        return Result<ModuleDescriptor>::failure(
            descriptor_error("Module display-name key must be non-empty stable ASCII."));
    }
    if (!canonicalize_enum_set(
            spec.categories,
            static_cast<std::underlying_type_t<ModuleCategory>>(ModuleCategory::OUTPUT))
        || !canonicalize_enum_set(
            spec.allowed_stages,
            static_cast<std::underlying_type_t<ProcessingStage>>(ProcessingStage::MASTER))
        || !canonicalize_enum_set(
            spec.allowed_segments,
            static_cast<std::underlying_type_t<ChainSegment>>(ChainSegment::TERMINAL))) {
        return Result<ModuleDescriptor>::failure(
            descriptor_error("Descriptor enum sets must be valid, non-empty, and duplicate-free."));
    }

    if (spec.placement_class != PlacementClass::INLINE_CHAIN
        && spec.placement_class != PlacementClass::TERMINAL_SLOT) {
        return Result<ModuleDescriptor>::failure(
            descriptor_error("Descriptor placement class is invalid."));
    }
    if (spec.terminal_slot.has_value()
        && static_cast<std::underlying_type_t<TerminalSlot>>(*spec.terminal_slot)
            > static_cast<std::underlying_type_t<TerminalSlot>>(TerminalSlot::WAV_ENCODER)) {
        return Result<ModuleDescriptor>::failure(
            descriptor_error("Descriptor terminal slot is invalid."));
    }

    for (const auto segment : spec.allowed_segments) {
        const bool requiresRestore = segment == ChainSegment::REPAIR
            || segment == ChainSegment::PRE_MASTER_CONDITIONING;
        const auto requiredStage =
            requiresRestore ? ProcessingStage::RESTORE_PREP : ProcessingStage::MASTER;
        if (!contains<ProcessingStage>(spec.allowed_stages, requiredStage)) {
            return Result<ModuleDescriptor>::failure(
                descriptor_error("Descriptor segment is inconsistent with its allowed stages."));
        }
    }

    if (spec.placement_class == PlacementClass::TERMINAL_SLOT) {
        const bool exactStage = spec.allowed_stages.size() == 1U
            && spec.allowed_stages.front() == ProcessingStage::MASTER;
        const bool exactSegment = spec.allowed_segments.size() == 1U
            && spec.allowed_segments.front() == ChainSegment::TERMINAL;
        if (!exactStage || !exactSegment || !spec.terminal_slot.has_value()
            || spec.duplicable || !spec.single_active_instance) {
            return Result<ModuleDescriptor>::failure(
                descriptor_error("Terminal-slot descriptor invariants are not satisfied."));
        }
    } else if (spec.terminal_slot.has_value()) {
        return Result<ModuleDescriptor>::failure(
            descriptor_error("Inline descriptors cannot declare a terminal slot."));
    }

    if (!canonicalize_id_set(spec.must_precede, spec.type_id)
        || !canonicalize_id_set(spec.must_follow, spec.type_id)
        || !canonicalize_id_set(spec.recommended_before, spec.type_id)
        || !canonicalize_id_set(spec.recommended_after, spec.type_id)) {
        return Result<ModuleDescriptor>::failure(
            descriptor_error("Descriptor order sets contain an invalid, duplicate, or self ID."));
    }
    if (intersects(spec.must_precede, spec.must_follow)) {
        return Result<ModuleDescriptor>::failure(
            descriptor_error("Descriptor hard-order edges are contradictory."));
    }
    if ((spec.algorithm_version.has_value() && !is_stable_ascii(*spec.algorithm_version))
        || (spec.parameter_schema_id.has_value()
            && !is_stable_ascii(*spec.parameter_schema_id))) {
        return Result<ModuleDescriptor>::failure(
            descriptor_error("Descriptor version/schema tokens must be non-empty stable ASCII."));
    }

    return Result<ModuleDescriptor>::success(ModuleDescriptor{std::move(spec)});
}

ModuleDescriptor::ModuleDescriptor(ModuleDescriptorSpec canonical_spec)
    : spec_(std::move(canonical_spec))
{
}

std::string_view ModuleDescriptor::type_id() const noexcept { return spec_.type_id; }
std::string_view ModuleDescriptor::display_name_key() const noexcept
{
    return spec_.display_name_key;
}
std::span<const ModuleCategory> ModuleDescriptor::categories() const noexcept
{
    return spec_.categories;
}
std::span<const ProcessingStage> ModuleDescriptor::allowed_stages() const noexcept
{
    return spec_.allowed_stages;
}
std::span<const ChainSegment> ModuleDescriptor::allowed_segments() const noexcept
{
    return spec_.allowed_segments;
}
bool ModuleDescriptor::duplicable() const noexcept { return spec_.duplicable; }
bool ModuleDescriptor::bypassable() const noexcept { return spec_.bypassable; }
PlacementClass ModuleDescriptor::placement_class() const noexcept
{
    return spec_.placement_class;
}
std::optional<TerminalSlot> ModuleDescriptor::terminal_slot() const noexcept
{
    return spec_.terminal_slot;
}
std::span<const std::string> ModuleDescriptor::must_precede() const noexcept
{
    return spec_.must_precede;
}
std::span<const std::string> ModuleDescriptor::must_follow() const noexcept
{
    return spec_.must_follow;
}
std::span<const std::string> ModuleDescriptor::recommended_before() const noexcept
{
    return spec_.recommended_before;
}
std::span<const std::string> ModuleDescriptor::recommended_after() const noexcept
{
    return spec_.recommended_after;
}
bool ModuleDescriptor::must_be_last() const noexcept { return spec_.must_be_last; }
bool ModuleDescriptor::single_active_instance() const noexcept
{
    return spec_.single_active_instance;
}
std::optional<std::string_view> ModuleDescriptor::algorithm_version() const noexcept
{
    if (!spec_.algorithm_version.has_value()) {
        return std::nullopt;
    }
    return *spec_.algorithm_version;
}
std::optional<std::string_view> ModuleDescriptor::parameter_schema_id() const noexcept
{
    if (!spec_.parameter_schema_id.has_value()) {
        return std::nullopt;
    }
    return *spec_.parameter_schema_id;
}

}  // namespace rgsml::dsp
