#pragma once
// M18 PRIVATE PREPRODUCTION. This is NOT installed by app/src/main.cpp.
// One Product-owned epoch fanout must protect BOTH the M17 Product session
// and the exact TerminalCandidateGate used by its renderer. Installing only
// one of the two would permit a void legacy M17 path to diverge while the
// Windows export worker retains a supposedly verified immutable Master.
#include "terminal_private_product_session.hpp"
#include "m18_product_terminal_candidate_binding.hpp"
#include <QThread>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>

namespace rgsml::app::internal {

struct M18ProductPrivateSessionBinding final {
    using Fanout=platform::windows::internal::M18OwnerMutationFanout;

    [[nodiscard]] static core::Status bind(
        TerminalPrivateProductSession& session,
        TerminalCandidateGate& candidates,
        const Fanout& fanout)
    {
        // Binding is an all-or-nothing owner-thread configuration step,
        // never callable by a detached worker or during the Qt signal
        // cascade. Refuse any pre-existing independent authority rather
        // than silently overwriting it with a second epoch stream.
        if(QThread::currentThread()!=session.thread()
           ||session.m18_private_mutation_admission_installed()
           ||candidates.m18_mutation_admission_installed())
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_PRIVATE_PRODUCT_BINDING_WRONG_OWNER_OR_ALREADY_GATED"});
        session.set_private_m18_mutation_admission(
            fanout.status_admission());
        M18ProductTerminalCandidateBinding::bind(candidates,fanout);
        if(!session.m18_private_mutation_admission_installed()
           ||!candidates.m18_mutation_admission_installed())
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_PRIVATE_PRODUCT_BINDING_COULD_NOT_INSTALL_BOTH_GATES"});
        return core::Status::success();
    }
};

} // namespace rgsml::app::internal
