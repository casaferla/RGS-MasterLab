#include <rgsml/dsp/mastering_chain_state.hpp>

#include <rgsml/core/error.hpp>

#include <utility>

namespace rgsml::dsp {
namespace {

using rgsml::core::Error;
using rgsml::core::ErrorCode;
using rgsml::core::Result;
using rgsml::core::Status;
using rgsml::core::Uuid;

constexpr auto kGainTypeId = "rgsml.dsp.gain";
constexpr auto kEqTypeId = "rgsml.dsp.parametric-eq";

[[nodiscard]] Error chain_state_error(ErrorCode code, std::string category, std::string message)
{
    return Error{code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] Uuid default_chain_uuid()
{
    return *Uuid::parse("00000000-0000-4000-8000-000000000001").value();
}

[[nodiscard]] ModuleInstanceId default_gain_instance_id()
{
    return *ModuleInstanceId::from_uuid(*Uuid::parse("00000000-0000-4000-8000-000000000010").value()).value();
}

[[nodiscard]] ModuleInstanceId default_eq_instance_id()
{
    return *ModuleInstanceId::from_uuid(*Uuid::parse("00000000-0000-4000-8000-000000000020").value()).value();
}

}  // namespace

Result<MasteringChainState> MasteringChainState::create_default(
    const ModuleRegistry& registry,
    Uuid chain_id,
    std::optional<ModuleInstanceId> gain_id,
    std::optional<ModuleInstanceId> eq_id)
{
    if (chain_id == Uuid{}) {
        chain_id = default_chain_uuid();
    }
    const auto effective_gain_id = gain_id.value_or(default_gain_instance_id());
    const auto effective_eq_id = eq_id.value_or(default_eq_instance_id());

    auto default_gain = GainParameters::create(0.0);
    if (!default_gain) {
        return Result<MasteringChainState>::failure(*default_gain.error());
    }
    auto default_eq = ParametricEqParameters::create_legacy_default();
    if (!default_eq) {
        return Result<MasteringChainState>::failure(*default_eq.error());
    }

    return create(
        registry,
        chain_id,
        effective_gain_id,
        *default_gain.value(),
        false,
        effective_eq_id,
        *default_eq.value(),
        false);
}

Result<MasteringChainState> MasteringChainState::create(
    const ModuleRegistry& registry,
    Uuid chain_id,
    ModuleInstanceId gain_id,
    GainParameters gain_params,
    bool gain_bypassed,
    ModuleInstanceId eq_id,
    ParametricEqParameters eq_params,
    bool eq_bypassed)
{
    if (gain_id == eq_id) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "DUPLICATE_MODULE_INSTANCE_ID",
            "Gain and Parametric EQ module instance IDs must be unique."));
    }

    auto chain_res = ProcessingChain::create(
        registry,
        ProcessingChainContext{ProcessingStage::MASTER, ChainSegment::MANUAL});
    if (!chain_res) {
        return Result<MasteringChainState>::failure(*chain_res.error());
    }
    auto chain = std::move(*chain_res.value());

    auto gain_add_status = chain.add(gain_id, kGainTypeId, 0U);
    if (!gain_add_status) {
        return Result<MasteringChainState>::failure(*gain_add_status.error());
    }
    if (gain_bypassed) {
        auto bypass_status = chain.set_user_bypass(gain_id, true);
        if (!bypass_status) {
            return Result<MasteringChainState>::failure(*bypass_status.error());
        }
    }

    auto eq_add_status = chain.add(eq_id, kEqTypeId, 1U);
    if (!eq_add_status) {
        return Result<MasteringChainState>::failure(*eq_add_status.error());
    }
    if (eq_bypassed) {
        auto bypass_status = chain.set_user_bypass(eq_id, true);
        if (!bypass_status) {
            return Result<MasteringChainState>::failure(*bypass_status.error());
        }
    }

    return Result<MasteringChainState>::success(MasteringChainState{
        chain_id,
        std::move(chain),
        gain_id,
        std::move(gain_params),
        eq_id,
        std::move(eq_params)});
}

MasteringChainState::MasteringChainState(
    Uuid chain_id,
    ProcessingChain chain,
    ModuleInstanceId gain_id,
    GainParameters gain_params,
    ModuleInstanceId eq_id,
    ParametricEqParameters eq_params) noexcept
    : chain_id_(chain_id)
    , chain_(std::move(chain))
    , gain_id_(gain_id)
    , gain_params_(std::move(gain_params))
    , eq_id_(eq_id)
    , eq_params_(std::move(eq_params))
{
}

const Uuid& MasteringChainState::chain_id() const noexcept { return chain_id_; }
const ProcessingChain& MasteringChainState::chain() const noexcept { return chain_; }
std::span<const ModuleInstance> MasteringChainState::instances() const noexcept { return chain_.instances(); }
std::size_t MasteringChainState::module_count() const noexcept { return chain_.instances().size(); }

const ModuleInstanceId& MasteringChainState::gain_instance_id() const noexcept { return gain_id_; }
const ModuleInstanceId& MasteringChainState::eq_instance_id() const noexcept { return eq_id_; }

Result<std::reference_wrapper<const ModuleInstance>> MasteringChainState::gain_instance() const
{
    return chain_.find_instance(gain_id_);
}

Result<std::reference_wrapper<const ModuleInstance>> MasteringChainState::eq_instance() const
{
    return chain_.find_instance(eq_id_);
}

const GainParameters& MasteringChainState::gain_parameters() const noexcept { return gain_params_; }

Status MasteringChainState::set_gain_parameters(const GainParameters& params)
{
    gain_params_ = params;
    return Status::success();
}

const ParametricEqParameters& MasteringChainState::parametric_eq_parameters() const noexcept { return eq_params_; }

Status MasteringChainState::set_parametric_eq_parameters(const ParametricEqParameters& params)
{
    eq_params_ = params;
    return Status::success();
}

Result<bool> MasteringChainState::is_bypassed(const ModuleInstanceId& instance_id) const
{
    auto inst_res = chain_.find_instance(instance_id);
    if (!inst_res) {
        return Result<bool>::failure(*inst_res.error());
    }
    return Result<bool>::success(inst_res.value()->get().user_bypass());
}

Status MasteringChainState::set_user_bypass(const ModuleInstanceId& instance_id, bool bypassed)
{
    return chain_.set_user_bypass(instance_id, bypassed);
}

std::vector<ModuleExecutionBinding> MasteringChainState::execution_bindings() const
{
    return std::vector<ModuleExecutionBinding>{
        ModuleExecutionBinding{gain_id_, gain_params_},
        ModuleExecutionBinding{eq_id_, eq_params_}};
}

}  // namespace rgsml::dsp
