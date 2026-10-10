#include "mastering_chain_state.hpp"

#include <rgsml/core/error.hpp>

#include <QUuid>

#include <utility>

namespace rgsml::app {
namespace {

using rgsml::core::Error;
using rgsml::core::ErrorCode;
using rgsml::core::Result;
using rgsml::core::Status;
using rgsml::core::Uuid;
using namespace rgsml::dsp;

constexpr auto kGainTypeId = "rgsml.dsp.gain";
constexpr auto kEqTypeId = "rgsml.dsp.parametric-eq";
constexpr auto kCompressorTypeId = "rgsml.dsp.compressor";
constexpr auto kStereoMsTypeId = "rgsml.dsp.stereo-ms";

[[nodiscard]] Error chain_state_error(ErrorCode code, std::string category, std::string message)
{
    return Error{code, std::move(message), {{"category", std::move(category)}}};
}

}  // namespace

Result<MasteringChainState> MasteringChainState::create_default(
    const ModuleRegistry& registry,
    Uuid chain_id,
    ModuleInstanceId gain_id,
    ModuleInstanceId eq_id)
{
    const auto comp_uuid = *Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toLower().toStdString()).value();
    const auto comp_id = *ModuleInstanceId::from_uuid(comp_uuid).value();
    return create_default(registry, chain_id, gain_id, eq_id, comp_id);
}

Result<MasteringChainState> MasteringChainState::create_default(
    const ModuleRegistry& registry,
    Uuid chain_id,
    ModuleInstanceId gain_id,
    ModuleInstanceId eq_id,
    ModuleInstanceId compressor_id)
{
    auto default_gain = GainParameters::create(0.0);
    if (!default_gain) {
        return Result<MasteringChainState>::failure(*default_gain.error());
    }
    auto default_eq = ParametricEqParameters::create_legacy_default();
    if (!default_eq) {
        return Result<MasteringChainState>::failure(*default_eq.error());
    }
    auto default_comp = CompressorParameters::create_default();
    if (!default_comp) {
        return Result<MasteringChainState>::failure(*default_comp.error());
    }

    return create(
        registry,
        chain_id,
        gain_id,
        *default_gain.value(),
        false,
        eq_id,
        *default_eq.value(),
        false,
        compressor_id,
        *default_comp.value(),
        true);
}

Result<MasteringChainState> MasteringChainState::create_with_stereo_ms(
    const ModuleRegistry& registry,
    Uuid chain_id,
    ModuleInstanceId gain_id,
    ModuleInstanceId eq_id,
    ModuleInstanceId compressor_id,
    ModuleInstanceId stereo_ms_id,
    StereoMsParameters stereo_ms_params,
    bool stereo_ms_bypassed)
{
    if (stereo_ms_id.uuid().is_nil()
        || stereo_ms_id == gain_id
        || stereo_ms_id == eq_id
        || stereo_ms_id == compressor_id) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument, "DUPLICATE_MODULE_INSTANCE_ID",
            "Stereo/M-S requires a distinct non-nil module instance ID."));
    }
    auto candidate = create_default(registry, chain_id, gain_id, eq_id, compressor_id);
    if (!candidate) {
        return candidate;
    }
    auto& state = *candidate.value();
    auto status = state.chain_.add(
        stereo_ms_id, kStereoMsTypeId, state.chain_.instances().size());
    if (!status) {
        return Result<MasteringChainState>::failure(*status.error());
    }
    if (stereo_ms_bypassed) {
        status = state.chain_.set_user_bypass(stereo_ms_id, true);
        if (!status) {
            return Result<MasteringChainState>::failure(*status.error());
        }
    }
    state.stereo_ms_id_ = stereo_ms_id;
    state.stereo_ms_params_ = std::move(stereo_ms_params);
    return candidate;
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
    const auto comp_uuid = *Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toLower().toStdString()).value();
    const auto comp_id = *ModuleInstanceId::from_uuid(comp_uuid).value();
    const auto comp_params = *CompressorParameters::create_default().value();
    return create(
        registry,
        chain_id,
        gain_id,
        std::move(gain_params),
        gain_bypassed,
        eq_id,
        std::move(eq_params),
        eq_bypassed,
        comp_id,
        comp_params,
        true);
}

Result<MasteringChainState> MasteringChainState::create(
    const ModuleRegistry& registry,
    Uuid chain_id,
    ModuleInstanceId gain_id,
    GainParameters gain_params,
    bool gain_bypassed,
    ModuleInstanceId eq_id,
    ParametricEqParameters eq_params,
    bool eq_bypassed,
    ModuleInstanceId compressor_id,
    CompressorParameters compressor_params,
    bool compressor_bypassed)
{
    if (chain_id.is_nil()) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_CHAIN_ID",
            "MasteringChainState requires an explicit non-nil chain_id."));
    }
    if (gain_id.uuid().is_nil()) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_MODULE_INSTANCE_ID",
            "MasteringChainState requires an explicit non-nil gain_instance_id."));
    }
    if (eq_id.uuid().is_nil()) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_MODULE_INSTANCE_ID",
            "MasteringChainState requires an explicit non-nil eq_instance_id."));
    }
    if (compressor_id.uuid().is_nil()) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_MODULE_INSTANCE_ID",
            "MasteringChainState requires an explicit non-nil compressor_instance_id."));
    }
    if (gain_id == eq_id || gain_id == compressor_id || eq_id == compressor_id) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "DUPLICATE_MODULE_INSTANCE_ID",
            "Gain, Parametric EQ, and Compressor module instance IDs must be unique."));
    }

    auto reg_ptr = std::make_shared<const ModuleRegistry>(registry);

    auto chain_res = ProcessingChain::create(
        *reg_ptr,
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

    auto comp_add_status = chain.add(compressor_id, kCompressorTypeId, 2U);
    if (!comp_add_status) {
        return Result<MasteringChainState>::failure(*comp_add_status.error());
    }
    if (compressor_bypassed) {
        auto bypass_status = chain.set_user_bypass(compressor_id, true);
        if (!bypass_status) {
            return Result<MasteringChainState>::failure(*bypass_status.error());
        }
    }

    return Result<MasteringChainState>::success(MasteringChainState{
        std::move(reg_ptr),
        chain_id,
        std::move(chain),
        gain_id,
        std::move(gain_params),
        eq_id,
        std::move(eq_params),
        compressor_id,
        std::move(compressor_params)});
}

Result<MasteringChainState> MasteringChainState::restore(
    std::shared_ptr<const ModuleRegistry> registry,
    Uuid chain_id,
    ProcessingChain chain,
    ModuleInstanceId gain_id,
    GainParameters gain_params,
    ModuleInstanceId eq_id,
    ParametricEqParameters eq_params)
{
    const auto comp_uuid = *Uuid::parse(QUuid::createUuid().toString(QUuid::WithoutBraces).toLower().toStdString()).value();
    const auto comp_id = *ModuleInstanceId::from_uuid(comp_uuid).value();
    const auto comp_params = *CompressorParameters::create_default().value();
    return restore(
        std::move(registry),
        chain_id,
        std::move(chain),
        gain_id,
        std::move(gain_params),
        eq_id,
        std::move(eq_params),
        comp_id,
        comp_params);
}

Result<MasteringChainState> MasteringChainState::restore(
    std::shared_ptr<const ModuleRegistry> registry,
    Uuid chain_id,
    ProcessingChain chain,
    ModuleInstanceId gain_id,
    GainParameters gain_params,
    ModuleInstanceId eq_id,
    ParametricEqParameters eq_params,
    ModuleInstanceId compressor_id,
    CompressorParameters compressor_params)
{
    if (!registry) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "NO_REGISTRY",
            "MasteringChainState restore requires a valid module registry."));
    }
    if (chain_id.is_nil()) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_CHAIN_ID",
            "MasteringChainState requires an explicit non-nil chain_id."));
    }
    if (gain_id.uuid().is_nil()) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_MODULE_INSTANCE_ID",
            "MasteringChainState requires an explicit non-nil gain_instance_id."));
    }
    if (eq_id.uuid().is_nil()) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_MODULE_INSTANCE_ID",
            "MasteringChainState requires an explicit non-nil eq_instance_id."));
    }
    if (compressor_id.uuid().is_nil()) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_MODULE_INSTANCE_ID",
            "MasteringChainState requires an explicit non-nil compressor_instance_id."));
    }
    if (gain_id == eq_id || gain_id == compressor_id || eq_id == compressor_id) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "DUPLICATE_MODULE_INSTANCE_ID",
            "Gain, Parametric EQ, and Compressor module instance IDs must be unique."));
    }
    if (chain.context().stage != ProcessingStage::MASTER || chain.context().segment != ChainSegment::MANUAL) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_CHAIN_CONTEXT",
            "MasteringChainState requires a MASTER/MANUAL processing chain context."));
    }
    if (chain.instances().size() != 3U) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_MODULE_COUNT",
            "MasteringChainState requires exactly 3 module instances."));
    }
    if (chain.instances()[0].instance_id() != gain_id || chain.instances()[0].module_type_id() != kGainTypeId) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_GAIN_MODULE",
            "Module 0 must be Input Gain with matching instance_id."));
    }
    if (chain.instances()[1].instance_id() != eq_id || chain.instances()[1].module_type_id() != kEqTypeId) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_EQ_MODULE",
            "Module 1 must be Parametric EQ with matching instance_id."));
    }
    if (chain.instances()[2].instance_id() != compressor_id || chain.instances()[2].module_type_id() != kCompressorTypeId) {
        return Result<MasteringChainState>::failure(chain_state_error(
            ErrorCode::InvalidArgument,
            "INVALID_COMPRESSOR_MODULE",
            "Module 2 must be Compressor with matching instance_id."));
    }

    return Result<MasteringChainState>::success(MasteringChainState{
        std::move(registry),
        chain_id,
        std::move(chain),
        gain_id,
        std::move(gain_params),
        eq_id,
        std::move(eq_params),
        compressor_id,
        std::move(compressor_params)});
}

MasteringChainState::MasteringChainState(
    std::shared_ptr<const ModuleRegistry> registry,
    Uuid chain_id,
    ProcessingChain chain,
    ModuleInstanceId gain_id,
    GainParameters gain_params,
    ModuleInstanceId eq_id,
    ParametricEqParameters eq_params,
    ModuleInstanceId compressor_id,
    CompressorParameters compressor_params) noexcept
    : registry_(std::move(registry))
    , chain_id_(chain_id)
    , chain_(std::move(chain))
    , gain_id_(gain_id)
    , gain_params_(std::move(gain_params))
    , eq_id_(eq_id)
    , eq_params_(std::move(eq_params))
    , compressor_id_(compressor_id)
    , compressor_params_(std::move(compressor_params))
{
}

const Uuid& MasteringChainState::chain_id() const noexcept { return chain_id_; }
const ProcessingChain& MasteringChainState::chain() const noexcept { return chain_; }
std::span<const ModuleInstance> MasteringChainState::instances() const noexcept { return chain_.instances(); }
std::size_t MasteringChainState::module_count() const noexcept { return chain_.instances().size(); }

const ModuleInstanceId& MasteringChainState::gain_instance_id() const noexcept { return gain_id_; }
const ModuleInstanceId& MasteringChainState::eq_instance_id() const noexcept { return eq_id_; }
const ModuleInstanceId& MasteringChainState::compressor_instance_id() const noexcept { return compressor_id_; }
const std::optional<ModuleInstanceId>& MasteringChainState::stereo_ms_instance_id() const noexcept { return stereo_ms_id_; }

Result<std::reference_wrapper<const ModuleInstance>> MasteringChainState::gain_instance() const
{
    return chain_.find_instance(gain_id_);
}

Result<std::reference_wrapper<const ModuleInstance>> MasteringChainState::eq_instance() const
{
    return chain_.find_instance(eq_id_);
}

Result<std::reference_wrapper<const ModuleInstance>> MasteringChainState::compressor_instance() const
{
    return chain_.find_instance(compressor_id_);
}

Result<std::reference_wrapper<const ModuleInstance>> MasteringChainState::stereo_ms_instance() const
{
    if (!stereo_ms_id_) {
        return Result<std::reference_wrapper<const ModuleInstance>>::failure(
            chain_state_error(ErrorCode::InvalidState, "STEREO_MS_NOT_PRESENT",
                              "Stereo/M-S is not in this product chain."));
    }
    return chain_.find_instance(*stereo_ms_id_);
}

Result<std::reference_wrapper<const ModuleDescriptor>> MasteringChainState::find_descriptor(std::string_view type_id) const
{
    if (!registry_) {
        return Result<std::reference_wrapper<const ModuleDescriptor>>::failure(
            chain_state_error(ErrorCode::InvalidState, "NO_REGISTRY", "MasteringChainState has no valid registry."));
    }
    return registry_->find_descriptor(type_id);
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

const CompressorParameters& MasteringChainState::compressor_parameters() const noexcept { return compressor_params_; }

Status MasteringChainState::set_compressor_parameters(const CompressorParameters& params)
{
    compressor_params_ = params;
    return Status::success();
}

const std::optional<StereoMsParameters>& MasteringChainState::stereo_ms_parameters() const noexcept
{
    return stereo_ms_params_;
}

Status MasteringChainState::set_stereo_ms_parameters(const StereoMsParameters& params)
{
    if (!stereo_ms_id_ || !stereo_ms_params_) {
        return Status::failure(chain_state_error(ErrorCode::InvalidState,
            "STEREO_MS_NOT_PRESENT", "Cannot edit absent Stereo/M-S module."));
    }
    stereo_ms_params_ = params;
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
    std::vector<ModuleExecutionBinding> bindings{
        ModuleExecutionBinding{gain_id_, gain_params_},
        ModuleExecutionBinding{eq_id_, eq_params_},
        ModuleExecutionBinding{compressor_id_, compressor_params_}};
    if (stereo_ms_id_ && stereo_ms_params_) {
        bindings.emplace_back(*stereo_ms_id_, *stereo_ms_params_);
    }
    return bindings;
}

}  // namespace rgsml::app
