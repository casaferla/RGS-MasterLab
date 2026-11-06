#pragma once
// M18 PRIVATE / NOT INVOKED FROM MAIN OR QML.
// First actual typed wiring of existing controller admissions to ONE
// Product-owned Qt-free mutation fanout. The full Product gate still needs
// Project Open/SaveAs batches, candidate-gate invalidation, playback
// retirement and verified M17 consent before this can be enabled in main.
#include "audition_region_view_model.hpp"
#include "audition_source_selector.hpp"
#include "gold_selection_view_model.hpp"
#include "mastering_chain_state.hpp"
#include "master_output_rate_state.hpp"
#include "project_session_view_model.hpp"
#include "source_selection_view_model.hpp"
#include "terminal_limiter_state.hpp"
#include "../../../platform/windows/src/internal/m18_owner_mutation_fanout.hpp"

namespace rgsml::app::internal {

struct M18ProductControllerBinding final {
    using Fanout=platform::windows::internal::M18OwnerMutationFanout;
    static void bind_audition_region_gold(
        AuditionSourceSelector& selector,
        AuditionRegionViewModel& region,
        GoldSelectionViewModel& gold,
        const Fanout& fanout)
    {
        // ONE epoch at the selector; GOLD's outer ViewModel delegates to
        // selector_->set_gold/clear_gold. Installing both gates would be
        // ambiguous and correctly fail closed at the GOLD ViewModel.
        (void)gold;
        selector.set_private_m18_mutation_admission(
            fanout.status_admission());
        region.set_private_m18_region_mutation_admission(
            fanout.status_admission());
    }

    static void bind_source_chain_rate(
        SourceSelectionViewModel& source,
        MasteringChainState& chain,
        MasterOutputRateState& rate,
        const Fanout& fanout)
    {
        source.set_private_m18_source_mutation_admission(
            fanout.status_admission());
        chain.set_private_m18_chain_mutation_admission(
            fanout.status_admission());
        rate.set_private_m18_rate_mutation_admission(
            fanout.rate_admission());
    }

    static void bind_chain_and_terminal(
        MasteringChainState& chain,
        TerminalLimiterState& terminal,
        const Fanout& fanout)
    {
        // Both direct DSP mutators receive the SAME epoch stream.
        // The terminal editor must delegate to TerminalLimiterState.
        chain.set_private_m18_chain_mutation_admission(
            fanout.status_admission());
        bind_terminal_parameters(terminal,fanout);
    }

    static void bind_project_save_only(
        ProjectSessionViewModel& project,const Fanout& fanout)
    {
        // Safe intermediate Product wiring. SaveAs owns one real
        // exclusive epoch through file readback. Open Project is
        // intentionally FAIL-CLOSED until its full downstream batch
        // is wired; never mistake this for complete Project support.
        project.set_private_m18_project_save_batch_begin(
            fanout.project_save_batch());
    }

    static void bind_terminal_parameters(
        TerminalLimiterState& terminal,const Fanout& fanout)
    {
        // Do not separately install TerminalLimiterViewModel's gate:
        // the real TerminalLimiterState will admit once for direct and
        // editor-routed updates.
        terminal.set_private_m18_mutation_admission(
            fanout.status_admission());
    }
};

} // namespace rgsml::app::internal
