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
        if(!render)return false;
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

    // A reference-only Product epoch transition does NOT change any PCM
    // samples in immutable PREPARED/PROCESSED RenderResults. Reuse their
    // previously measured M19/M20 numbers only after a complete and
    // exceptionally narrow Qt-owner proof:
    //   - a DIFFERENT authenticated Gold WAV is selected;
    //   - exactly ONE Product epoch and ONE master generation advanced;
    //   - every other Product identity field is bit-for-bit unchanged;
    //   - authentic SOURCE, full scope and BOTH PCM owners/realization ID
    //     are identical to those that produced the old numeric evidence;
    //   - old reference and both numeric records shared the same epoch.
    // No previous Gold proof, intermediate DSP edit, source replacement,
    // altered channel/rate/region, or uncertain authority => no reuse.
    // Re-publication goes through the SAME strict inbox acceptance as a
    // worker result; no M18 ticket/consent or qualified Gate G is minted.
    [[nodiscard]] std::array<bool,3> rebind_after_reference_only_change(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold)
    {
        std::array<bool,3> rebounded{};
        if(!goldCapture_)return rebounded;
        const auto replacement=Product::capture_gold(
            product,fanout,selector,gold);
        const auto current=Product::capture(product,fanout,selector);
        if(!replacement||!current
           ||!Product::gold_still_current(product,fanout,selector,gold,
                                          *replacement.value())
           ||!Product::still_current(product,fanout,selector,
                                     *current.value()))
            return rebounded;
        const auto& oldRef=*goldCapture_;
        const auto& now=*current.value();
        if(oldRef.owner.epoch==0
           ||oldRef.owner.epoch==
                 (std::numeric_limits<std::uint64_t>::max)()
           ||oldRef.owner.identity.master_generation==
                 (std::numeric_limits<std::uint64_t>::max)()
           ||oldRef.gold.same_resource_identity(replacement.value()->gold)
           ||oldRef.owner.epoch+1!=now.owner.epoch
           ||replacement.value()->owner.epoch!=now.owner.epoch
           ||replacement.value()->owner.identity!=now.owner.identity)
            return rebounded;
        auto exclusivelyGold=oldRef.owner.identity;
        ++exclusivelyGold.master_generation;
        if(now.owner.identity!=exclusivelyGold)return rebounded;

        const auto previous=publisher_.view().columns;
        for(const auto column:{Column::prepared,Column::processed}) {
            const auto i=static_cast<std::size_t>(column);
            const auto& oldCapture=column==Column::prepared
                ?preparedCapture_:processedCapture_;
            const auto& oldSnapshot=previous[i];
            if(!oldCapture||!oldSnapshot
               ||oldCapture->owner.epoch!=oldRef.owner.epoch
               ||oldCapture->owner.identity!=oldRef.owner.identity
               ||oldSnapshot->lifecycle!=m21_private::Lifecycle::current
               ||oldSnapshot->column!=column
               ||oldSnapshot->generation_token!=oldRef.owner.epoch
               ||!oldCapture->source.same_resource_identity(now.source)
               ||oldCapture->source_full_frames!=now.source_full_frames
               ||oldCapture->prepared!=now.prepared
               ||oldCapture->processed!=now.processed
               ||oldCapture->processed_realization_id
                    !=now.processed_realization_id)
                continue;
            auto reused=*oldSnapshot; // exact immutable measured evidence
            reused.generation_token=now.owner.epoch;
            if(accept_render(product,fanout,selector,now,reused))
                rebounded[i]=true;
        }
        return rebounded; // the REFERENCE itself is NEVER cached here
    }

    // Reuse only immutable PREPARED and REFERENCE measurements after an
    // EXACT one-epoch Product mutation with unchanged SOURCE, PREPARED
    // PCM and REFERENCE selection. This covers changes to MANUAL DSP
    // parameters (PROCESSSED must be recomputed) without assuming every
    // master-generation transition was a DSP edit.
    // In particular, GoldSelectionViewModel's monotonic selection
    // revision forbids reuse across a same-path reference replacement
    // that would be indistinguishable by ResourceReference alone.
    // M18's owner identity and all native export tickets remain untouched.
    [[nodiscard]] std::array<bool,3> rebind_unchanged_prepared_reference(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold)
    {
        std::array<bool,3> accepted{};
        if(!goldCapture_ ||!preparedCapture_ ||!goldSelectionRevision_
           ||*goldSelectionRevision_!=gold.selection_revision()
           ||gold.selection_revision()==
              (std::numeric_limits<std::uint64_t>::max)())
            return accepted;
        const auto current=Product::capture(product,fanout,selector);
        const auto currentGold=Product::capture_gold(
            product,fanout,selector,gold);
        if(!current||!currentGold
           ||!Product::still_current(product,fanout,selector,
                                     *current.value())
           ||!Product::gold_still_current(product,fanout,selector,gold,
                                          *currentGold.value()))
            return accepted;
        const auto& now=*current.value();
        const auto& newGold=*currentGold.value();
        const auto& before=*goldCapture_;
        const auto& oldPrepared=*preparedCapture_;
        if(before.owner.epoch==0
           ||before.owner.epoch==
                  (std::numeric_limits<std::uint64_t>::max)()
           ||before.owner.identity.master_generation==
                  (std::numeric_limits<std::uint64_t>::max)()
           ||before.owner.epoch+1!=now.owner.epoch
           ||newGold.owner.epoch!=now.owner.epoch
           ||newGold.owner.identity!=now.owner.identity
           ||!before.gold.same_resource_identity(newGold.gold)
           ||before.sample_rate_hz!=newGold.sample_rate_hz
           ||before.full_frames!=newGold.full_frames
           ||before.channels!=newGold.channels
           ||oldPrepared.owner!=before.owner
           ||!oldPrepared.source.same_resource_identity(now.source)
           ||oldPrepared.source_full_frames!=now.source_full_frames
           ||oldPrepared.prepared!=now.prepared)
            return accepted;
        auto onlyMaster=before.owner.identity;
        ++onlyMaster.master_generation;
        if(now.owner.identity!=onlyMaster)return accepted;
        const auto prior=publisher_.view().columns;
        const auto& oldPreparedRecord=prior[
            static_cast<std::size_t>(Column::prepared)];
        const auto& oldGoldRecord=prior[
            static_cast<std::size_t>(Column::gold)];
        const auto previousEpoch=before.owner.epoch;
        if(oldPreparedRecord
           &&oldPreparedRecord->lifecycle==m21_private::Lifecycle::current
           &&oldPreparedRecord->column==Column::prepared
           &&oldPreparedRecord->generation_token==previousEpoch) {
            auto recycled=*oldPreparedRecord;
            recycled.generation_token=now.owner.epoch;
            if(accept_render(product,fanout,selector,now,recycled))
                accepted[static_cast<std::size_t>(Column::prepared)]=true;
        }
        if(oldGoldRecord
           &&oldGoldRecord->lifecycle==m21_private::Lifecycle::current
           &&oldGoldRecord->column==Column::gold
           &&oldGoldRecord->generation_token==previousEpoch) {
            auto recycled=*oldGoldRecord;
            recycled.generation_token=now.owner.epoch;
            recycled.resource_identity="gold-file-generation:"+
                std::to_string(now.owner.epoch)+"|"+
                newGold.gold.provider_id()+"|"+newGold.gold.locator();
            if(accept_gold(product,fanout,selector,gold,newGold,recycled))
                accepted[static_cast<std::size_t>(Column::gold)]=true;
        }
        // The processed numerical column is NEVER admitted here.
        return accepted;
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
        if(latest[0] &&
           (!preparedCapture_ ||
            !Product::still_current(
                product,fanout,selector,*preparedCapture_)))
            latest[0]->lifecycle=m21_private::Lifecycle::stale;
        if(latest[1] &&
           (!goldCapture_ ||
            !Product::gold_still_current(
                product,fanout,selector,gold,*goldCapture_)))
            latest[1]->lifecycle=m21_private::Lifecycle::stale;
        if(latest[2] &&
           (!processedCapture_ ||
            !Product::still_current(
                product,fanout,selector,*processedCapture_)))
            latest[2]->lifecycle=m21_private::Lifecycle::stale;
    }

    m21_private::PrivatePublicationGuard publisher_{};
    std::optional<Product::Capture> preparedCapture_{};
    std::optional<Product::Capture> processedCapture_{};
    std::optional<Product::GoldCapture> goldCapture_{};
    std::optional<std::uint64_t> goldSelectionRevision_{};
};

} // namespace rgsml::app::internal
