#pragma once
// L1-M21 Product result inbox: **no** worker owns or reads QObjects.
// All methods are Qt-owner only. A numerical worker may finish with a
// valid old snapshot, but Qt's real M18/source/Gold authorities decide
// if and when any result is current. No official Phase-6 publication
// or QML wiring is claimed by this private Product adapter.
#include "m21_product_owner_analysis.hpp"
#include "m21_metrics_comparison_candidate.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::internal {

class M21ProductPublicationInbox final {
public:
    using Product=M21ProductOwnerAnalysis;
    using Controllers=Product::Controllers;
    using Fanout=Product::Fanout;
    using Column=Product::Column;
    using Snapshot=m21_private::Snapshot;
    using Comparison=m21_private::Comparison;

    // Complete private Product acceptance of PREPARED/PROCESSED worker
    // results. No explicit or implicit UI publish; this inbox will only
    // display results through view_current() after live epoch checking.
    [[nodiscard]] bool accept_render(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const Product::Capture& captured,const Snapshot& result)
    {
        if(result.column==Column::gold
           ||!Product::still_current(product,fanout,selector,captured)
           ||result.generation_token!=captured.owner.epoch)
            return false;
        const auto render=result.column==Column::prepared
            ?captured.prepared:captured.processed;
        if(!render ||(result.column==Column::processed
           &&render->chain_revision()
                !=captured.owner.identity.chain_revision))
            return false;
        const auto view=render->view();
        const auto expectedIdentity=result.column==Column::prepared
            ?("prepared-source-generation:"+
                std::to_string(captured.owner.identity.source_generation)+
                "|"+captured.source.provider_id()+"|"+
                captured.source.locator())
            :captured.processed_realization_id
                ?("processed-realization:"+
                    std::to_string(captured.processed_realization_id->value)+
                    "|source-generation:"+
                    std::to_string(captured.owner.identity.source_generation))
                :std::string{};
        const bool full=view.absolute_start_frame().value()==0
            &&view.frame_count().value()==captured.source_full_frames;
        if(result.resource_identity!=expectedIdentity
           ||result.absolute_scope_begin!=static_cast<std::uint64_t>(
                view.absolute_start_frame().value())
           ||result.scope_frames!=static_cast<std::uint64_t>(
                view.frame_count().value())
           ||result.sample_rate_hz!=static_cast<std::uint32_t>(
                view.format().sample_rate().value())
           ||result.analysis_scope_label!=(full?"Full Track":"Region")
           ||!publisher_.expect(result.column,result.generation_token)
           ||!publisher_.publish(result))
            return false;
        if(result.column==Column::prepared)
            preparedCapture_=captured;
        else processedCapture_=captured;
        return true;
    }

    // Product epoch is an export/operation safety boundary, NOT the
    // identity of the measured audio. Independently reauthenticate each
    // immutable observation after an unrelated owner-epoch transition.
    //
    // PREPARED is SOURCE PCM: retain through DSP edits, REFERENCE changes,
    // playback-target selection and terminal M17 Prepare if the authentic
    // SOURCE generation, project and retained PREPARED PCM have not changed.
    //
    // PROCESSED is one exact manual realization: additionally require the
    // same processed owner/RealizationId AND unchanged MANUAL chain revision.
    // A new DSP revision must require a new PROCESSED analysis.
    //
    // REFERENCE is an independently selected WAV: retain across all of the
    // above only when GoldSelectionViewModel's monotonic revision and exact
    // WAV resource/format remain unchanged. A same-path re-selection is
    // a NEW selection and must be measured anew.
    //
    // The M18 global epoch, master generation or final output-rate revision
    // may advance for reasons unrelated to these measured PCM samples.
    // Never transfer tickets, export consent or a Phase-6/Gate-G claim.
    // Strict accept_render/accept_gold still performs final live owner,
    // sample scope, published generation and resource validation.
    [[nodiscard]] std::array<bool,3> rebind_unchanged_independent_scopes(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold)
    {
        std::array<bool,3> accepted{};
        const auto nowResult=Product::capture(product,fanout,selector);
        const auto refResult=Product::capture_gold(
            product,fanout,selector,gold);
        const auto previous=publisher_.view().columns;

        if(nowResult &&
           Product::still_current(product,fanout,selector,
                                  *nowResult.value())) {
            const auto& now=*nowResult.value();
            for(const auto column:{Column::prepared,Column::processed}) {
                const auto i=static_cast<std::size_t>(column);
                const auto& beforeCapture=column==Column::prepared
                    ?preparedCapture_:processedCapture_;
                const auto& old=previous[i];
                if(!beforeCapture || !old
                   ||old->column!=column
                   ||old->lifecycle!=m21_private::Lifecycle::current
                   ||old->generation_token!=beforeCapture->owner.epoch
                   ||beforeCapture->owner.epoch==0
                   ||beforeCapture->owner.epoch>=now.owner.epoch
                   ||!Product::same_render_observation(
                       *beforeCapture,now,column))
                    continue;
                auto reused=*old; // exact measured cells and genuine PSD
                reused.generation_token=now.owner.epoch;
                if(accept_render(product,fanout,selector,now,reused))
                    accepted[i]=true;
            }
        }

        // Gold is independent of whether PREPARED/PROCESSED have already
        // completed or whether a worker is currently retiring. Never gate
        // this conservation on another column's readiness.
        if(refResult &&goldCapture_ &&goldSelectionRevision_
           &&*goldSelectionRevision_==gold.selection_revision()
           &&gold.selection_revision()!=
               (std::numeric_limits<std::uint64_t>::max)()
           &&Product::gold_still_current(
                product,fanout,selector,gold,*refResult.value())) {
            const auto& now=*refResult.value();
            const auto& before=*goldCapture_;
            const auto& old=previous[static_cast<std::size_t>(Column::gold)];
            if(old
               &&old->column==Column::gold
               &&old->lifecycle==m21_private::Lifecycle::current
               &&old->generation_token==before.owner.epoch
               &&before.owner.epoch>0
               &&before.owner.epoch<now.owner.epoch
               &&Product::same_gold_observation(before,now)) {
                auto reused=*old;
                reused.generation_token=now.owner.epoch;
                reused.resource_identity="gold-file-generation:"+
                    std::to_string(now.owner.epoch)+"|"+
                    now.gold.provider_id()+"|"+now.gold.locator();
                if(accept_gold(product,fanout,selector,gold,now,reused))
                    accepted[static_cast<std::size_t>(Column::gold)]=true;
            }
        }
        return accepted;
    }

    // A numerical worker can finish after M17, a target switch, a
    // REFERENCE selection or a chain edit that advances the Product epoch.
    // If—and ONLY if—the independent resource measured by that worker is
    // still exactly the current resource, revalidate under the live owner
    // and publish its ORIGINAL numeric cells with the current token.
    // No second M16/M19/M20 run and no cross-realization PROCESSED reuse.
    [[nodiscard]] bool accept_completed_unchanged_render(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const Product::Capture& oldCapture,const Snapshot& completed)
    {
        if(completed.column==Column::gold
           ||completed.generation_token!=oldCapture.owner.epoch)
            return false;
        const auto now=Product::capture(product,fanout,selector);
        if(!now ||now.value()->owner.epoch<oldCapture.owner.epoch
           ||!Product::still_current(
               product,fanout,selector,*now.value())
           ||!Product::same_render_observation(
               oldCapture,*now.value(),completed.column))
            return false;
        auto reauthenticated=completed;
        reauthenticated.generation_token=now.value()->owner.epoch;
        return accept_render(
            product,fanout,selector,*now.value(),reauthenticated);
    }

    [[nodiscard]] bool accept_completed_unchanged_gold(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,
        const Product::GoldCapture& oldCapture,const Snapshot& completed)
    {
        if(completed.column!=Column::gold
           ||completed.generation_token!=oldCapture.owner.epoch)
            return false;
        const auto now=Product::capture_gold(
            product,fanout,selector,gold);
        if(!now||now.value()->owner.epoch<oldCapture.owner.epoch
           ||!Product::gold_still_current(
               product,fanout,selector,gold,*now.value())
           ||!Product::same_gold_observation(
               oldCapture,*now.value()))
            return false;
        auto reauthenticated=completed;
        reauthenticated.generation_token=now.value()->owner.epoch;
        reauthenticated.resource_identity="gold-file-generation:"+
            std::to_string(now.value()->owner.epoch)+"|"+
            now.value()->gold.provider_id()+"|"+
            now.value()->gold.locator();
        return accept_gold(
            product,fanout,selector,gold,
            *now.value(),reauthenticated);
    }

    // GOLD is only accepted from a distinct selected WAV resource
    // captured on the true Qt owner. Measure on worker, revalidate on
    // return; never align Gold file frames with SOURCE by guesswork.
    [[nodiscard]] bool accept_gold(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,
        const Product::GoldCapture& captured,const Snapshot& result)
    {
        if(result.column!=Column::gold
           ||!Product::gold_still_current(
                product,fanout,selector,gold,captured)
           ||result.generation_token!=captured.owner.epoch
           ||result.absolute_scope_begin!=0U
           ||result.scope_frames!=static_cast<std::uint64_t>(
                captured.full_frames)
           ||result.sample_rate_hz!=captured.sample_rate_hz
           ||result.analysis_scope_label!="Full Track"
           ||result.resource_identity!=(
                "gold-file-generation:"+
                std::to_string(captured.owner.epoch)+"|"+
                captured.gold.provider_id()+"|"+captured.gold.locator())
           ||!publisher_.expect(result.column,result.generation_token)
           ||!publisher_.publish(result))
            return false;
        goldCapture_=captured;
        goldSelectionRevision_=gold.selection_revision();
        return true;
    }

    // This is the ONLY way a future UI may see this private inbox.
    // It dynamically marks previously published records STALE after any
    // SOURCE, DSP, Project, M17, Gold or owner change, even if the
    // asynchronous worker finished and its numbers still look valid.
    // Never return publisher_.view() directly to the GUI.
    [[nodiscard]] Comparison view_current(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold) const
    {
        auto latest=publisher_.view();
        retire_stale(latest.columns,product,fanout,selector,gold);
        return m21_private::compose(latest.columns);
    }

    // Read-only Qt status path. A lightweight typed receipt contains ONLY
    // resource identity, source scope, lifecycle and quality flags.
    // All three roles undergo the SAME live M18/Gold Qt-owner retirement
    // as view_current(), but no PSD, tonal curve, scalar or delta is copied.
    [[nodiscard]] std::array<
        std::optional<m21_private::ReadinessSnapshot>,3>
    view_current_readiness(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold) const
    {
        auto latest=publisher_.readiness_columns();
        retire_stale(latest,product,fanout,selector,gold);
        return latest;
    }

private:
    // One and only one Qt-owner stale gate serves BOTH full private
    // comparison evidence and the numeric-free readiness receipt.
    // This prevents a status-only shortcut from calling obsolete evidence
    // CURRENT when a SOURCE/REFERENCE/chain/epoch changes.
    template<class T>
    void retire_stale(std::array<std::optional<T>,3>& latest,
                      const Controllers& product,const Fanout& fanout,
                      const AuditionSourceSelector& selector,
                      const GoldSelectionViewModel& gold) const
    {
        // Source captures include BOTH manual PCM pointers. Calling the
        // old all-or-nothing Product::still_current for PREPARED would
        // falsely stale it whenever only PROCESSED was replaced (even
        // when this occurs within the same Product epoch). Readiness and
        // numerical readouts must use the SAME per-column identity gate.
        const auto currentRender=Product::capture(product,fanout,selector);
        const auto currentGold=Product::capture_gold(
            product,fanout,selector,gold);
        const auto renderCurrent=[&](const std::optional<Product::Capture>& old,
                                      Column column) {
            return old &&currentRender
                &&old->owner.epoch==currentRender.value()->owner.epoch
                &&Product::same_render_observation(
                    *old,*currentRender.value(),column);
        };
        const auto goldCurrent=goldCapture_&&currentGold
            &&goldCapture_->owner.epoch==currentGold.value()->owner.epoch
            &&Product::same_gold_observation(
                *goldCapture_,*currentGold.value());
        if(latest[static_cast<std::size_t>(Column::prepared)]
           &&!renderCurrent(preparedCapture_,Column::prepared))
            latest[static_cast<std::size_t>(Column::prepared)]
                ->lifecycle=m21_private::Lifecycle::stale;
        if(latest[static_cast<std::size_t>(Column::gold)]&&!goldCurrent)
            latest[static_cast<std::size_t>(Column::gold)]
                ->lifecycle=m21_private::Lifecycle::stale;
        if(latest[static_cast<std::size_t>(Column::processed)]
           &&!renderCurrent(processedCapture_,Column::processed))
            latest[static_cast<std::size_t>(Column::processed)]
                ->lifecycle=m21_private::Lifecycle::stale;
    }

    m21_private::PrivatePublicationGuard publisher_{};
    std::optional<Product::Capture> preparedCapture_{};
    std::optional<Product::Capture> processedCapture_{};
    std::optional<Product::GoldCapture> goldCapture_{};
    std::optional<std::uint64_t> goldSelectionRevision_{};
};

} // namespace rgsml::app::internal
