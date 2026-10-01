#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/processing_chain.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::dsp {

class MasteringChainState final {
public:
    [[nodiscard]] static rgsml::core::Result<MasteringChainState> create_default(
        const ModuleRegistry& registry,
        rgsml::core::Uuid chain_id = {},
        std::optional<ModuleInstanceId> gain_id = std::nullopt,
        std::optional<ModuleInstanceId> eq_id = std::nullopt);

    [[nodiscard]] static rgsml::core::Result<MasteringChainState> create(
        const ModuleRegistry& registry,
        rgsml::core::Uuid chain_id,
        ModuleInstanceId gain_id,
        GainParameters gain_params,
        bool gain_bypassed,
        ModuleInstanceId eq_id,
        ParametricEqParameters eq_params,
        bool eq_bypassed);

    [[nodiscard]] const rgsml::core::Uuid& chain_id() const noexcept;
    [[nodiscard]] const ProcessingChain& chain() const noexcept;
    [[nodiscard]] std::span<const ModuleInstance> instances() const noexcept;
    [[nodiscard]] std::size_t module_count() const noexcept;

    [[nodiscard]] const ModuleInstanceId& gain_instance_id() const noexcept;
    [[nodiscard]] const ModuleInstanceId& eq_instance_id() const noexcept;

    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const ModuleInstance>> gain_instance() const;
    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const ModuleInstance>> eq_instance() const;

    [[nodiscard]] const GainParameters& gain_parameters() const noexcept;
    [[nodiscard]] rgsml::core::Status set_gain_parameters(const GainParameters& params);

    [[nodiscard]] const ParametricEqParameters& parametric_eq_parameters() const noexcept;
    [[nodiscard]] rgsml::core::Status set_parametric_eq_parameters(const ParametricEqParameters& params);

    [[nodiscard]] rgsml::core::Result<bool> is_bypassed(const ModuleInstanceId& instance_id) const;
    [[nodiscard]] rgsml::core::Status set_user_bypass(const ModuleInstanceId& instance_id, bool bypassed);

    [[nodiscard]] std::vector<ModuleExecutionBinding> execution_bindings() const;

    friend bool operator==(const MasteringChainState&, const MasteringChainState&) = default;

private:
    MasteringChainState(
        rgsml::core::Uuid chain_id,
        ProcessingChain chain,
        ModuleInstanceId gain_id,
        GainParameters gain_params,
        ModuleInstanceId eq_id,
        ParametricEqParameters eq_params) noexcept;

    rgsml::core::Uuid chain_id_;
    ProcessingChain chain_;
    ModuleInstanceId gain_id_;
    GainParameters gain_params_;
    ModuleInstanceId eq_id_;
    ParametricEqParameters eq_params_;
};

}  // namespace rgsml::dsp
