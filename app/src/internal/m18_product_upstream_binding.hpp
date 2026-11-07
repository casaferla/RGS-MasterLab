#pragma once
// M18 Product upstream gates installed by the COMPLETE binder in main.cpp.
// SOURCE/Project physical batches and direct MASTER/chain/terminal edits
// share one owner-thread mutation fanout. This binder alone cannot mint
// export consent: the M17 ProductSession/CandidateGate guards must also
// be installed atomically by M18ProductCompleteBinding.
#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "master_output_rate_state.hpp"
#include "mastering_chain_state.hpp"
#include "audition_source_selector.hpp"
#include "audition_region_view_model.hpp"
#include "gold_selection_view_model.hpp"
#include "terminal_limiter_state.hpp"
#include "../../../platform/windows/src/internal/m18_owner_mutation_fanout.hpp"
#include <QThread>
#include <rgsml/core/error.hpp>

namespace rgsml::app::internal {

struct M18ProductUpstreamBinding final {
    // Only the private complete Product binder may invoke an incomplete
    // installation rollback. Never make this public or use it for teardown.
    friend struct M18ProductCompleteBinding;
    using Fanout=platform::windows::internal::M18OwnerMutationFanout;
    struct Controllers {
        ProjectSessionViewModel& project;
        SourceSelectionViewModel& source;
        MasterOutputRateState& rate;
        MasteringChainState& chain;
        AuditionSourceSelector& selector;
        AuditionRegionViewModel& region;
        GoldSelectionViewModel& gold;
        TerminalLimiterState& terminal;
    };

    [[nodiscard]] static bool owner_thread_current(const Controllers& c) noexcept {
        const auto current=QThread::currentThread();
        return c.project.thread()==current &&c.source.thread()==current
            &&c.rate.thread()==current &&c.selector.thread()==current
            &&c.region.thread()==current &&c.gold.thread()==current
            &&c.terminal.m18_owner_thread_current();
    }
    [[nodiscard]] static bool unbound(const Controllers& c) noexcept {
        return !c.project.m18_open_batch_installed()
            &&!c.project.m18_save_batch_installed()
            &&!c.project.m18_project_one_shot_installed()
            &&!c.project.m18_save_one_shot_installed()
            &&!c.source.m18_source_batch_installed()
            &&!c.source.m18_source_one_shot_installed()
            &&!c.rate.m18_rate_mutation_admission_installed()
            &&!c.chain.m18_chain_admission_installed()
            &&!c.selector.m18_mutation_admission_installed()
            &&!c.gold.m18_gold_admission_installed()
            &&!c.region.m18_region_admission_installed()
            &&!c.terminal.m18_mutation_admission_installed();
    }
    [[nodiscard]] static bool installed(const Controllers& c) noexcept {
        return c.project.m18_open_batch_installed()
            &&c.project.m18_save_batch_installed()
            &&c.source.m18_source_batch_installed()
            &&c.source.m18_checked_source_committed_installed()
            &&c.rate.m18_rate_mutation_admission_installed()
            &&c.chain.m18_chain_admission_installed()
            &&c.selector.m18_mutation_admission_installed()
            &&!c.gold.m18_gold_admission_installed()
            &&c.region.m18_region_admission_installed()
            &&c.terminal.m18_mutation_admission_installed();
    }

    // A conservative preflight rejects a partial/foreign installation
    // BEFORE any callback is installed. GOLD ViewModel delegates its
    // actual edit to the guarded selector; two independent GOLD gates
    // would consume two different Product generations.
    [[nodiscard]] static core::Status bind(Controllers c,const Fanout& f) {
        if(!owner_thread_current(c)||!unbound(c)
           ||!c.source.m18_checked_source_committed_installed())
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_UPSTREAM_BIND_REQUIRES_ONE_CLEAN_OWNER_AND_CHECKED_SOURCE"});
        c.project.set_private_m18_project_open_batch_begin(f.project_open_batch());
        c.project.set_private_m18_project_save_batch_begin(f.project_save_batch());
        c.source.set_private_m18_source_batch_begin(f.source_commit_batch());
        c.rate.set_private_m18_rate_mutation_admission(f.rate_admission());
        c.chain.set_private_m18_chain_mutation_admission(f.status_admission());
        c.selector.set_private_m18_mutation_admission(f.status_admission());
        c.region.set_private_m18_region_mutation_admission(f.status_admission());
        c.terminal.set_private_m18_mutation_admission(f.status_admission());
        if(!installed(c)) {
            rollback_partial(c);
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M18_UPSTREAM_BIND_ROLLED_BACK_PARTIAL_INSTALL"});
        }
        return core::Status::success();
    }

private:
    // For an installation that has NOT returned a usable Product contract,
    // no valid worker ticket may have been minted. This is the ONLY safe
    // place to undo partial gates. A public unbind() would bypass BUSY and
    // violate an active M18 commit lease, so no such API exists.
    static void rollback_partial(Controllers c) {
        if(!owner_thread_current(c))return;
        c.terminal.set_private_m18_mutation_admission({});
        c.region.set_private_m18_region_mutation_admission({});
        c.selector.set_private_m18_mutation_admission({});
        c.chain.set_private_m18_chain_mutation_admission({});
        c.rate.set_private_m18_rate_mutation_admission({});
        c.source.set_private_m18_source_batch_begin({});
        c.project.set_private_m18_project_save_batch_begin({});
        c.project.set_private_m18_project_open_batch_begin({});
    }
};

} // namespace rgsml::app::internal
