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
#include <functional>
#include <span>
#include <vector>

namespace rgsml::app {

class MasteringChainState final {
public:
    [[nodiscard]] static rgsml::core::Result<MasteringChainState> create_default(
        const dsp::ModuleRegistry& registry,
        rgsml::core::Uuid chain_id,
        dsp::ModuleInstanceId gain_id,
        dsp::ModuleInstanceId eq_id);

    [[nodiscard]] static rgsml::core::Result<MasteringChainState> create(
        const dsp::ModuleRegistry& registry,
        rgsml::core::Uuid chain_id,
        dsp::ModuleInstanceId gain_id,
        dsp::GainParameters gain_params,
        bool gain_bypassed,
        dsp::ModuleInstanceId eq_id,
        dsp::ParametricEqParameters eq_params,
        bool eq_bypassed);

    [[nodiscard]] const rgsml::core::Uuid& chain_id() const noexcept;
    [[nodiscard]] const dsp::ProcessingChain& chain() const noexcept;
    [[nodiscard]] std::span<const dsp::ModuleInstance> instances() const noexcept;
    [[nodiscard]] std::size_t module_count() const noexcept;

    [[nodiscard]] const dsp::ModuleInstanceId& gain_instance_id() const noexcept;
    [[nodiscard]] const dsp::ModuleInstanceId& eq_instance_id() const noexcept;

    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const dsp::ModuleInstance>> gain_instance() const;
    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const dsp::ModuleInstance>> eq_instance() const;

    [[nodiscard]] const dsp::GainParameters& gain_parameters() const noexcept;
    [[nodiscard]] rgsml::core::Status set_gain_parameters(const dsp::GainParameters& params);

    [[nodiscard]] const dsp::ParametricEqParameters& parametric_eq_parameters() const noexcept;
    [[nodiscard]] rgsml::core::Status set_parametric_eq_parameters(const dsp::ParametricEqParameters& params);

    [[nodiscard]] rgsml::core::Result<bool> is_bypassed(const dsp::ModuleInstanceId& instance_id) const;
    [[nodiscard]] rgsml::core::Status set_user_bypass(const dsp::ModuleInstanceId& instance_id, bool bypassed);

    [[nodiscard]] std::vector<dsp::ModuleExecutionBinding> execution_bindings() const;

    friend bool operator==(const MasteringChainState&, const MasteringChainState&) = default;

private:
    MasteringChainState(
        rgsml::core::Uuid chain_id,
        dsp::ProcessingChain chain,
        dsp::ModuleInstanceId gain_id,
        dsp::GainParameters gain_params,
        dsp::ModuleInstanceId eq_id,
        dsp::ParametricEqParameters eq_params) noexcept;

    rgsml::core::Uuid chain_id_;
    dsp::ProcessingChain chain_;
    dsp::ModuleInstanceId gain_id_;
    dsp::GainParameters gain_params_;
    dsp::ModuleInstanceId eq_id_;
    dsp::ParametricEqParameters eq_params_;
};

}  // namespace rgsml::app
