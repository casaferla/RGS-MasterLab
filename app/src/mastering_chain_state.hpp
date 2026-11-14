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
#include <rgsml/dsp/streaming_true_peak_limiter_parameters.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace rgsml::app {

class MasteringChainState final {
public:
    // M18 staging-only injection point, uninstalled in the application.
    // A real Product must share one epoch across every mutator, including
    // Project replacement and the terminal candidate gate. Fail closed.
    using M18MutationAdmission=std::function<core::Status(const char*)>;
    void set_private_m18_chain_mutation_admission(
        M18MutationAdmission gate);
    [[nodiscard]] bool m18_chain_admission_installed() const noexcept {
        return m18MutationAdmission_.callback
            &&static_cast<bool>(*m18MutationAdmission_.callback);
    }
    // Project open has its own preflight epoch admission BEFORE stopping
    // playback / replacing SOURCE. Preserve this runtime guard across the
    // once-authorized replacement of the project DSP snapshot.
    // This is NOT an independent admission; only the ProjectSession
    // transaction may call it, after checking its master epoch.
    void adopt_chain_after_authorized_project_open(
        MasteringChainState replacement);


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

    // New, unsaved product sessions contain M11 Gain -> M12 EQ -> M14 Compressor
    // -> M15 Stereo/M-S (initial BYPASS_IDENTITY). Historical 2/3-node project
    // restore and the legacy create_default() contract stay unchanged.
    [[nodiscard]] static rgsml::core::Result<MasteringChainState> create_product_default(
        const dsp::ModuleRegistry& registry,
        rgsml::core::Uuid chain_id,
        dsp::ModuleInstanceId gain_id,
        dsp::ModuleInstanceId eq_id,
        dsp::ModuleInstanceId compressor_id,
        dsp::ModuleInstanceId stereo_ms_id);

    // I2 preview-only optional fifth MANUAL DSP, never part of historical
    // project restore or automatic Product default. Fail closed if the
    // caller lacks the P2 opt-in registry or an M18 epoch is already active.
    [[nodiscard]] static core::Result<MasteringChainState>
    with_streaming_true_peak_preview(
        MasteringChainState baseline,
        dsp::ModuleInstanceId p2_id,
        dsp::StreamingTruePeakLimiterParameters parameters,
        bool bypassed);

    // Explicit typed construction, also used by the new product-default path.
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
        dsp::CompressorParameters compressor_params,
        // Both absent for legacy chains; both present only for an explicit
        // fourth Stereo/M-S instance, never inferred from defaults.
        std::optional<dsp::ModuleInstanceId> stereo_ms_id = std::nullopt,
        std::optional<dsp::StereoMsParameters> stereo_ms_params = std::nullopt);

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
    // Capture immutable registry provenance WITH the chain before scheduling
    // a render worker; do not use a separately rebuilt default registry.
    [[nodiscard]] std::shared_ptr<const dsp::ModuleRegistry> registry_snapshot() const noexcept;
    [[nodiscard]] std::span<const dsp::ModuleInstance> instances() const noexcept;
    [[nodiscard]] std::size_t module_count() const noexcept;

    [[nodiscard]] const dsp::ModuleInstanceId& gain_instance_id() const noexcept;
    [[nodiscard]] const dsp::ModuleInstanceId& eq_instance_id() const noexcept;
    [[nodiscard]] const dsp::ModuleInstanceId& compressor_instance_id() const noexcept;
    [[nodiscard]] const std::optional<dsp::ModuleInstanceId>& stereo_ms_instance_id() const noexcept;
    [[nodiscard]] const std::optional<dsp::ModuleInstanceId>& streaming_p2_preview_instance_id() const noexcept;
    [[nodiscard]] const std::optional<dsp::StreamingTruePeakLimiterParameters>& streaming_p2_preview_parameters() const noexcept;
    [[nodiscard]] core::Status set_streaming_p2_preview_parameters(
        const dsp::StreamingTruePeakLimiterParameters& parameters);

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
    [[nodiscard]] core::Status admit_m18_chain_mutation(
        const char* operation) const;
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
    // Ephemeral only; no project persistence/schema admission in I2 preview.
    std::optional<dsp::ModuleInstanceId> streaming_p2_preview_id_;
    std::optional<dsp::StreamingTruePeakLimiterParameters> streaming_p2_preview_params_;
    struct M18RuntimeGuard final {
        std::shared_ptr<M18MutationAdmission> callback;
        std::thread::id ownerThread{};
        mutable bool admitting{false};
        // This guard is an ephemeral authorizer, NOT DSP/project content.
        // Preserve the original value equality semantics of the state.
        friend bool operator==(const M18RuntimeGuard&,
                               const M18RuntimeGuard&) noexcept {return true;}
    };
    M18RuntimeGuard m18MutationAdmission_;
};

}  // namespace rgsml::app
