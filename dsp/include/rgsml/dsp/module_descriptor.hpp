#pragma once

#include <rgsml/core/result.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rgsml::dsp {

enum class ModuleCategory : std::uint8_t {
    RESTORATION,
    UTILITY,
    FILTER_EQ,
    DYNAMICS,
    SPATIAL,
    OUTPUT,
};

enum class ProcessingStage : std::uint8_t {
    RESTORE_PREP,
    MASTER,
};

enum class ChainSegment : std::uint8_t {
    REPAIR,
    PRE_MASTER_CONDITIONING,
    MANUAL,
    DNA_LINKED,
    REF_LINKED,
    TERMINAL,
};

enum class PlacementClass : std::uint8_t {
    INLINE_CHAIN,
    TERMINAL_SLOT,
};

enum class TerminalSlot : std::uint8_t {
    FINAL_OUTPUT_SRC,
    FINAL_TRUE_PEAK_LIMITER,
    DITHER,
    QUANTIZER,
    WAV_ENCODER,
};

struct ModuleDescriptorSpec final {
    std::string type_id;
    std::string display_name_key;
    std::vector<ModuleCategory> categories;
    std::vector<ProcessingStage> allowed_stages;
    std::vector<ChainSegment> allowed_segments;
    bool duplicable;
    bool bypassable;
    PlacementClass placement_class;
    std::optional<TerminalSlot> terminal_slot;
    std::vector<std::string> must_precede;
    std::vector<std::string> must_follow;
    std::vector<std::string> recommended_before;
    std::vector<std::string> recommended_after;
    bool must_be_last;
    bool single_active_instance;
    std::optional<std::string> algorithm_version;
    std::optional<std::string> parameter_schema_id;

    friend bool operator==(const ModuleDescriptorSpec&, const ModuleDescriptorSpec&) = default;
};

class ModuleDescriptor final {
public:
    [[nodiscard]] static rgsml::core::Result<ModuleDescriptor>
    create(ModuleDescriptorSpec spec);

    [[nodiscard]] std::string_view type_id() const noexcept;
    [[nodiscard]] std::string_view display_name_key() const noexcept;
    [[nodiscard]] std::span<const ModuleCategory> categories() const noexcept;
    [[nodiscard]] std::span<const ProcessingStage> allowed_stages() const noexcept;
    [[nodiscard]] std::span<const ChainSegment> allowed_segments() const noexcept;
    [[nodiscard]] bool duplicable() const noexcept;
    [[nodiscard]] bool bypassable() const noexcept;
    [[nodiscard]] PlacementClass placement_class() const noexcept;
    [[nodiscard]] std::optional<TerminalSlot> terminal_slot() const noexcept;
    [[nodiscard]] std::span<const std::string> must_precede() const noexcept;
    [[nodiscard]] std::span<const std::string> must_follow() const noexcept;
    [[nodiscard]] std::span<const std::string> recommended_before() const noexcept;
    [[nodiscard]] std::span<const std::string> recommended_after() const noexcept;
    [[nodiscard]] bool must_be_last() const noexcept;
    [[nodiscard]] bool single_active_instance() const noexcept;
    [[nodiscard]] std::optional<std::string_view> algorithm_version() const noexcept;
    [[nodiscard]] std::optional<std::string_view> parameter_schema_id() const noexcept;

    friend bool operator==(const ModuleDescriptor&, const ModuleDescriptor&) = default;

private:
    explicit ModuleDescriptor(ModuleDescriptorSpec canonical_spec);
    ModuleDescriptorSpec spec_;
};

}  // namespace rgsml::dsp
