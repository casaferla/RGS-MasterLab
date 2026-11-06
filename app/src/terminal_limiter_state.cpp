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
    : chain_(std::move(chain)),id_(std::move(id)),params_(params),
      ownerThread_(std::this_thread::get_id()) {}
core::Status TerminalLimiterState::admit_m18_mutation(
    const char* operation) const
{
    if(!m18MutationAdmission_)return core::Status::success();
    if(std::this_thread::get_id()!=ownerThread_)
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_TERMINAL_MUTATION_WRONG_OWNER_THREAD"});
    if(m18MutationAdmitting_)
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_TERMINAL_MUTATION_REENTRANT_ADMISSION_DENIED"});
    // One stack-only flag, no worker wait or GUI event-loop pumping.
    // Reentrant state edits are rejected before they can change revisions.
    m18MutationAdmitting_=true;
    struct Reset final {
        bool* flag;
        ~Reset(){*flag=false;}
    } reset{&m18MutationAdmitting_};
    try{return m18MutationAdmission_(operation);}
    catch(...) {return core::Status::failure(core::Error{
        core::ErrorCode::InvalidState,
        "M18_TERMINAL_MUTATION_ADMISSION_THROWN"});}
}
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
    const auto allowed=admit_m18_mutation("terminal_ceiling");
    if(!allowed)return allowed;
    params_=*validated.value();
    ++parameterRevision_;
    return core::Status::success();
}
core::Status TerminalLimiterState::set_user_bypass(bool bypass)
{
    if(m18MutationAdmission_) {
        // A true no-op is not a new Product generation. Preserve legacy
        // M17 behavior when the optional Product epoch is not installed.
        if(bypassed()==bypass)return core::Status::success();
        const auto allowed=admit_m18_mutation("terminal_bypass");
        if(!allowed)return allowed;
    }
    return chain_.set_user_bypass(id_,bypass);
}
std::vector<dsp::ModuleExecutionBinding>
TerminalLimiterState::execution_bindings() const
{
    return {{id_,params_}};
}
} // namespace rgsml::app
