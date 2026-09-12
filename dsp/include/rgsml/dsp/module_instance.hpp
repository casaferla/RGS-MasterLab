#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/core/strong_id.hpp>
#include <rgsml/core/uuid.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rgsml::dsp {

struct ModuleInstanceIdTag final {
};
using ModuleInstanceId = rgsml::core::StrongId<ModuleInstanceIdTag>;

enum class ModuleProvenance : std::uint8_t {
    MANUAL,
};

enum class ModuleOwner : std::uint8_t {
    USER,
};

enum class ModuleLinkState : std::uint8_t {
    UNLINKED,
};

struct ModuleParameterState final {
    friend bool operator==(const ModuleParameterState&, const ModuleParameterState&) = default;
};

struct ModuleInstanceSpec final {
    ModuleInstanceId instance_id;
    std::string module_type_id;
    bool enabled;
    bool user_bypass;
    bool controller_suspended;
    bool domain_suspended;
    ModuleProvenance provenance;
    ModuleOwner owner;
    ModuleLinkState link_state;
    std::optional<rgsml::core::Uuid> semantic_node_id;
    ModuleParameterState parameter_state;

    friend bool operator==(const ModuleInstanceSpec&, const ModuleInstanceSpec&) = default;
};

class ModuleInstance final {
public:
    [[nodiscard]] static rgsml::core::Result<ModuleInstance>
    create_manual(ModuleInstanceId instance_id, std::string module_type_id);

    [[nodiscard]] static rgsml::core::Result<ModuleInstance>
    create(ModuleInstanceSpec spec);

    [[nodiscard]] const ModuleInstanceId& instance_id() const noexcept;
    [[nodiscard]] std::string_view module_type_id() const noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] bool user_bypass() const noexcept;
    [[nodiscard]] bool controller_suspended() const noexcept;
    [[nodiscard]] bool domain_suspended() const noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] ModuleProvenance provenance() const noexcept;
    [[nodiscard]] ModuleOwner owner() const noexcept;
    [[nodiscard]] ModuleLinkState link_state() const noexcept;
    [[nodiscard]] std::optional<rgsml::core::Uuid> semantic_node_id() const noexcept;
    [[nodiscard]] const ModuleParameterState& parameter_state() const noexcept;

    void set_enabled(bool enabled) noexcept;
    void set_user_bypass(bool bypassed) noexcept;
    void set_controller_suspended(bool suspended) noexcept;
    void set_domain_suspended(bool suspended) noexcept;

    [[nodiscard]] ModuleInstance duplicate_with_id(ModuleInstanceId new_id) const;

    friend bool operator==(const ModuleInstance&, const ModuleInstance&) = default;

private:
    explicit ModuleInstance(ModuleInstanceSpec spec);
    ModuleInstanceSpec spec_;
};

}  // namespace rgsml::dsp
