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
};

} // namespace rgsml::app::internal
