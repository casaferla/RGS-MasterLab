#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app {

class MasteringChainState final {
public:
    [[nodiscard]] static rgsml::core::Result<MasteringChainState> create_default(
        const dsp::ModuleRegistry& registry,
        rgsml::core::Uuid chain_id,
        dsp::ModuleInstanceId gain_id,
        dsp::ModuleInstanceId eq_id,
        dsp::ModuleInstanceId compressor_id);

    [[nodiscard]] static rgsml::core::Result<MasteringChainState> create_default(
        const dsp::ModuleRegistry& registry,
        rgsml::core::Uuid chain_id,
        dsp::ModuleInstanceId gain_id,
        dsp::ModuleInstanceId eq_id);

    // Explicit opt-in only: legacy three-module product state remains unchanged.
    [[nodiscard]] static rgsml::core::Result<MasteringChainState> create_with_stereo_ms(
        const dsp::ModuleRegistry& registry,
        rgsml::core::Uuid chain_id,
        dsp::ModuleInstanceId gain_id,
        dsp::ModuleInstanceId eq_id,
        dsp::ModuleInstanceId compressor_id,
        dsp::ModuleInstanceId stereo_ms_id,
        dsp::StereoMsParameters stereo_ms_params,
        bool stereo_ms_bypassed);

    [[nodiscard]] static rgsml::core::Result<MasteringChainState> create(
        const dsp::ModuleRegistry& registry,
        rgsml::core::Uuid chain_id,
        dsp::ModuleInstanceId gain_id,
        dsp::GainParameters gain_params,
        bool gain_bypassed,
        dsp::ModuleInstanceId eq_id,
        dsp::ParametricEqParameters eq_params,
        bool eq_bypassed,
        dsp::ModuleInstanceId compressor_id,
        dsp::CompressorParameters compressor_params,
        bool compressor_bypassed);

    [[nodiscard]] static rgsml::core::Result<MasteringChainState> create(
        const dsp::ModuleRegistry& registry,
        rgsml::core::Uuid chain_id,
        dsp::ModuleInstanceId gain_id,
        dsp::GainParameters gain_params,
        bool gain_bypassed,
        dsp::ModuleInstanceId eq_id,
        dsp::ParametricEqParameters eq_params,
        bool eq_bypassed);

    [[nodiscard]] static rgsml::core::Result<MasteringChainState> restore(
        std::shared_ptr<const dsp::ModuleRegistry> registry,
        rgsml::core::Uuid chain_id,
        dsp::ProcessingChain chain,
        dsp::ModuleInstanceId gain_id,
        dsp::GainParameters gain_params,
        dsp::ModuleInstanceId eq_id,
        dsp::ParametricEqParameters eq_params,
        dsp::ModuleInstanceId compressor_id,
        dsp::CompressorParameters compressor_params);

    [[nodiscard]] static rgsml::core::Result<MasteringChainState> restore(
        std::shared_ptr<const dsp::ModuleRegistry> registry,
        rgsml::core::Uuid chain_id,
        dsp::ProcessingChain chain,
        dsp::ModuleInstanceId gain_id,
        dsp::GainParameters gain_params,
        dsp::ModuleInstanceId eq_id,
        dsp::ParametricEqParameters eq_params);

    [[nodiscard]] const rgsml::core::Uuid& chain_id() const noexcept;
    [[nodiscard]] const dsp::ProcessingChain& chain() const noexcept;
    [[nodiscard]] std::span<const dsp::ModuleInstance> instances() const noexcept;
    [[nodiscard]] std::size_t module_count() const noexcept;

    [[nodiscard]] const dsp::ModuleInstanceId& gain_instance_id() const noexcept;
    [[nodiscard]] const dsp::ModuleInstanceId& eq_instance_id() const noexcept;
    [[nodiscard]] const dsp::ModuleInstanceId& compressor_instance_id() const noexcept;
    [[nodiscard]] const std::optional<dsp::ModuleInstanceId>& stereo_ms_instance_id() const noexcept;

    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const dsp::ModuleInstance>> gain_instance() const;
    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const dsp::ModuleInstance>> eq_instance() const;
    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const dsp::ModuleInstance>> compressor_instance() const;
    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const dsp::ModuleInstance>> stereo_ms_instance() const;
    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const dsp::ModuleDescriptor>> find_descriptor(std::string_view type_id) const;

    [[nodiscard]] const dsp::GainParameters& gain_parameters() const noexcept;
    [[nodiscard]] rgsml::core::Status set_gain_parameters(const dsp::GainParameters& params);

    [[nodiscard]] const dsp::ParametricEqParameters& parametric_eq_parameters() const noexcept;
    [[nodiscard]] rgsml::core::Status set_parametric_eq_parameters(const dsp::ParametricEqParameters& params);

    [[nodiscard]] const dsp::CompressorParameters& compressor_parameters() const noexcept;
    [[nodiscard]] rgsml::core::Status set_compressor_parameters(const dsp::CompressorParameters& params);
    [[nodiscard]] const std::optional<dsp::StereoMsParameters>& stereo_ms_parameters() const noexcept;
    [[nodiscard]] rgsml::core::Status set_stereo_ms_parameters(const dsp::StereoMsParameters& params);

    [[nodiscard]] rgsml::core::Result<bool> is_bypassed(const dsp::ModuleInstanceId& instance_id) const;
    [[nodiscard]] rgsml::core::Status set_user_bypass(const dsp::ModuleInstanceId& instance_id, bool bypassed);

    [[nodiscard]] std::vector<dsp::ModuleExecutionBinding> execution_bindings() const;

    friend bool operator==(const MasteringChainState&, const MasteringChainState&) = default;

private:
    MasteringChainState(
        std::shared_ptr<const dsp::ModuleRegistry> registry,
        rgsml::core::Uuid chain_id,
        dsp::ProcessingChain chain,
        dsp::ModuleInstanceId gain_id,
        dsp::GainParameters gain_params,
        dsp::ModuleInstanceId eq_id,
        dsp::ParametricEqParameters eq_params,
        dsp::ModuleInstanceId compressor_id,
        dsp::CompressorParameters compressor_params) noexcept;

    std::shared_ptr<const dsp::ModuleRegistry> registry_;
    rgsml::core::Uuid chain_id_;
    dsp::ProcessingChain chain_;
    dsp::ModuleInstanceId gain_id_;
    dsp::GainParameters gain_params_;
    dsp::ModuleInstanceId eq_id_;
    dsp::ParametricEqParameters eq_params_;
    dsp::ModuleInstanceId compressor_id_;
    dsp::CompressorParameters compressor_params_;
    std::optional<dsp::ModuleInstanceId> stereo_ms_id_;
    std::optional<dsp::StereoMsParameters> stereo_ms_params_;
};

}  // namespace rgsml::app
