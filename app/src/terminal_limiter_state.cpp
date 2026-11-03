#include "terminal_limiter_state.hpp"
#include <utility>
#include <limits>
#include <rgsml/core/error.hpp>
namespace rgsml::app {
core::Result<TerminalLimiterState> TerminalLimiterState::create_default(
    const dsp::ModuleRegistry& registry, dsp::ModuleInstanceId id)
{
    auto params=dsp::TruePeakLimiterParameters::create_default();
    if(!params)return core::Result<TerminalLimiterState>::failure(*params.error());
    return create(registry,id,*params.value(),true);
}
core::Result<TerminalLimiterState> TerminalLimiterState::create(
    const dsp::ModuleRegistry& registry, dsp::ModuleInstanceId id,
    dsp::TruePeakLimiterParameters params, bool bypassed)
{
    auto chain=dsp::ProcessingChain::create(registry,
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL});
    if(!chain)return core::Result<TerminalLimiterState>::failure(*chain.error());
    auto added=chain.value()->add(id,"rgsml.dsp.true-peak-limiter",0);
    if(!added)return core::Result<TerminalLimiterState>::failure(*added.error());
    if(bypassed) {
        auto b=chain.value()->set_user_bypass(id,true);
        if(!b)return core::Result<TerminalLimiterState>::failure(*b.error());
    }
    return core::Result<TerminalLimiterState>::success(
        TerminalLimiterState{std::move(*chain.value()),id,params});
}
TerminalLimiterState::TerminalLimiterState(
    dsp::ProcessingChain chain,dsp::ModuleInstanceId id,
    dsp::TruePeakLimiterParameters params) noexcept
    : chain_(std::move(chain)),id_(std::move(id)),params_(params) {}
core::Status TerminalLimiterState::set_ceiling_db_tp(double value)
{
    auto validated=dsp::TruePeakLimiterParameters::create(value);
    if(!validated)return core::Status::failure(*validated.error());
    if (params_.ceiling_db_tp()==validated.value()->ceiling_db_tp())
        return core::Status::success();
    if(parameterRevision_==std::numeric_limits<std::uint64_t>::max())
        return core::Status::failure(core::Error{
            core::ErrorCode::IntegerOverflow,
            "M17 Ceiling parameter revision sequence exhausted."});
    params_=*validated.value();
    ++parameterRevision_;
    return core::Status::success();
}
core::Status TerminalLimiterState::set_user_bypass(bool bypass)
{
    return chain_.set_user_bypass(id_,bypass);
}
std::vector<dsp::ModuleExecutionBinding>
TerminalLimiterState::execution_bindings() const
{
    return {{id_,params_}};
}
} // namespace rgsml::app
