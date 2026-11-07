#pragma once
// L1-M19..M21 Product owner bridge (read-only, Qt owner thread).
// Capture ONLY actual current PREPARED/PROCESSED RenderResult shared owners
// from AuditionSourceSelector under the already installed M18 Product
// mutation epoch. Never manufacture RAW/GOLD/PROCESSED PCM, invent an M16
// qualified True Peak, access mutable Qt controllers on a worker, or
// promote this private report to a Phase-6 official FeatureStatus.
#include "m18_product_complete_binding.hpp"
#include "m21_real_product_metrics_port.hpp"
#include "audition_source_selector.hpp"
#include "gold_selection_view_model.hpp"
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/resource_reference.hpp>
#include <QThread>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::internal {

class M21ProductOwnerAnalysis final {
public:
    using Binding=M18ProductCompleteBinding;
    using Controllers=Binding::Controllers;
    using Fanout=Binding::Fanout;
    using Column=m21_private::Column;

    struct Capture final {
        Fanout::OwnerSnapshot owner;
        core::ResourceReference source;
        std::shared_ptr<const render::RenderResult> prepared;
        std::shared_ptr<const render::RenderResult> processed;
        std::optional<core::RealizationId> processed_realization_id;
    };

    [[nodiscard]] static core::Result<Capture> capture(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector)
    {
        using R=core::Result<Capture>;
        if(!M18ProductCompleteBinding::installed(product)
           ||!M18ProductCompleteBinding::Upstream::
                owner_thread_current(product.upstream)
           ||QThread::currentThread()!=selector.thread()
           ||QThread::currentThread()!=product.session.thread()
           ||!fanout.can_install_full_product_guards())
            return R::failure(failed("M21_CAPTURE_REQUIRES_QT_OWNER_AND_M18_GATES"));
        const auto owner=fanout.current_owner_snapshot();
        const auto* source=product.upstream.source.source_resource();
        if(!owner||!owner->identity.valid()
           ||!source||!source->reference().permissions().can_read()
           ||!product.upstream.rate.has_supported_master_rate()
           ||owner->identity.master_rate_hz!=
                static_cast<std::uint32_t>(
                    product.upstream.rate.master_rate_hz())
           ||owner->identity.master_rate_revision!=
                    product.upstream.rate.rate_revision()
           ||owner->identity.chain_revision!=
                    product.upstream.chain.chain().revision())
            return R::failure(failed("M21_CAPTURE_SOURCE_RATE_CHAIN_EPOCH_STALE"));
        auto prepared=selector.prepared_realization_snapshot();
        auto processed=selector.processed_realization_snapshot();
        auto id=selector.processed_realization_id();
        if(!prepared ||(static_cast<bool>(processed)!=id.has_value())
           ||(id&&id->value==0))
            return R::failure(failed("M21_CAPTURE_NO_AUTHENTIC_PREPARED_OR_PROCESSED"));
        if(prepared->view().frame_count().value()<=0 ||
           (processed&&processed->view().frame_count().value()<=0))
            return R::failure(failed("M21_CAPTURE_EMPTY_REAL_RENDER"));
        return R::success(Capture{
            *owner,source->reference(),std::move(prepared),
            std::move(processed),id});
    }

    // A value-copy of the Product epoch and retained immutable PCM is
    // NOT itself a lifetime authority: owner pointer/realization and
    // SOURCE must be rechecked on the Qt thread before publication.
    [[nodiscard]] static bool still_current(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const Capture& expected) noexcept
    {
        if(!Binding::installed(product)
           ||!Binding::Upstream::owner_thread_current(product.upstream)
           ||QThread::currentThread()!=selector.thread()
           ||QThread::currentThread()!=product.session.thread()
           ||!fanout.can_install_full_product_guards())
            return false;
        const auto current=fanout.current_owner_snapshot();
        const auto* source=product.upstream.source.source_resource();
        return current &&current->epoch==expected.owner.epoch
            &&current->identity==expected.owner.identity
            &&source&&source->reference().same_resource_identity(
                expected.source)
            &&selector.prepared_realization_snapshot()==expected.prepared
            &&selector.processed_realization_snapshot()==expected.processed
            &&selector.processed_realization_id()
                ==expected.processed_realization_id
            &&current->identity.chain_revision
                ==product.upstream.chain.chain().revision()
            &&current->identity.master_rate_revision
                ==product.upstream.rate.rate_revision()
            &&current->identity.master_rate_hz
                ==static_cast<std::uint32_t>(
                    product.upstream.rate.master_rate_hz());
    }

    struct GoldCapture final {
        Fanout::OwnerSnapshot owner;
        core::ResourceReference gold;
        std::uint32_t sample_rate_hz{0};
        std::int64_t full_frames{0};
        std::size_t channels{0};
    };

    // GOLD is an INDEPENDENT, read-only WAV resource owned by the
    // actual GoldSelectionViewModel. It is never a processed alias,
    // a SOURCE-derived replica or an implicitly time-aligned region.
    // Project-open GOLD which has not been reified by this ViewModel is
    // explicitly unavailable until a separate resource owner is bound.
    [[nodiscard]] static core::Result<GoldCapture> capture_gold(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold)
    {
        using R=core::Result<GoldCapture>;
        if(QThread::currentThread()!=gold.thread()
           ||QThread::currentThread()!=selector.thread()
           ||!Binding::installed(product)
           ||!Binding::Upstream::owner_thread_current(product.upstream)
           ||!fanout.can_install_full_product_guards()
           ||!selector.gold_available())
            return R::failure(failed("M21_GOLD_CAPTURE_REQUIRES_AUTHENTIC_QT_OWNER"));
        const auto* resource=gold.gold_resource();
        const auto productEpoch=fanout.current_owner_snapshot();
        if(!resource||!productEpoch||!productEpoch->identity.valid()
           ||!resource->reference().permissions().can_read())
            return R::failure(failed("M21_GOLD_NO_INDEPENDENT_READ_OWNER"));
        const auto& info=resource->wav_info();
        const auto fs=info.audio_format().sample_rate().value();
        const auto frames=info.frame_count().value();
        const auto channels=info.audio_format().channel_count();
        if(fs<=0||frames<=0||(channels!=1U&&channels!=2U))
            return R::failure(failed("M21_GOLD_INVALID_NATIVE_WAV_SCOPE"));
        return R::success(GoldCapture{
            *productEpoch,resource->reference(),
            static_cast<std::uint32_t>(fs),frames,channels});
    }

    [[nodiscard]] static bool gold_still_current(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,
        const GoldCapture& expected) noexcept
    {
        if(QThread::currentThread()!=gold.thread()
           ||QThread::currentThread()!=selector.thread()
           ||!Binding::installed(product)
           ||!Binding::Upstream::owner_thread_current(product.upstream)
           ||!fanout.can_install_full_product_guards()
           ||!selector.gold_available())
            return false;
        const auto current=fanout.current_owner_snapshot();
        const auto* resource=gold.gold_resource();
        if(!current||!resource
           ||current->epoch!=expected.owner.epoch
           ||current->identity!=expected.owner.identity
           ||!resource->reference().same_resource_identity(expected.gold))
            return false;
        const auto& info=resource->wav_info();
        return info.frame_count().value()==expected.full_frames
            &&info.audio_format().sample_rate().value()
                ==static_cast<std::int64_t>(expected.sample_rate_hz)
            &&info.audio_format().channel_count()==expected.channels;
    }

    // Synchronous, owner-thread-only real PCM analysis for now. No
    // event-loop pumping or write; when async scheduling is introduced,
    // move only immutable owners to workers and use still_current before
    // accepting results. GOLD needs independent WAV decode/owner proof
    // and is deliberately NOT substituted by PREPARED or PROCESSED.
    [[nodiscard]] static core::Result<m21_private::Snapshot>
    measure_current(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const Capture& captured,Column column)
    {
        using R=core::Result<m21_private::Snapshot>;
        if(column==Column::gold)
            return R::failure(failed("M21_GOLD_REQUIRES_INDEPENDENT_RESOURCE_OWNER"));
        if(!still_current(product,fanout,selector,captured))
            return R::failure(failed("M21_CAPTURE_STALE_BEFORE_ANALYSIS"));
        const auto render=column==Column::prepared
            ?captured.prepared:captured.processed;
        if(!render)
            return R::failure(failed("M21_COLUMN_OWNER_NOT_PUBLISHED"));
        const auto view=render->view();
        if(view.frame_count().value()<=0)
            return R::failure(failed("M21_RENDER_SCOPE_EMPTY"));
        const auto level=analysis::measure_basic_level_full_resource(
            render->buffer());
        if(!level)return R::failure(*level.error());
        const auto tonal=analysis::measure_tonal_stereo_scope(
            view,view.absolute_start_frame(),view.frame_count(),
            view.timebase().frame_domain_id());
        if(!tonal)return R::failure(*tonal.error());
        if(!still_current(product,fanout,selector,captured))
            return R::failure(failed("M21_CAPTURE_STALE_AFTER_ANALYSIS"));
        m21_private::ProductMetricsOwnerClaim claim;
        claim.column=column;
        claim.generation_token=captured.owner.epoch;
        // PREPARED has no independent RealizationId. Its TRUE Product
        // provenance is the committed SOURCE provider+locator and
        // monotonic source generation, not a forged Processed identity.
        if(column==Column::prepared)
            claim.exact_realization_id="prepared-source-generation:"+
                std::to_string(captured.owner.identity.source_generation)
                +"|"+captured.source.provider_id()
                +"|"+captured.source.locator();
        else {
            if(!captured.processed_realization_id)
                return R::failure(failed("M21_NO_ACCEPTED_PROCESSED_REALIZATION"));
            claim.exact_realization_id="processed-realization:"+
                std::to_string(captured.processed_realization_id->value)
                +"|source-generation:"+
                std::to_string(captured.owner.identity.source_generation);
        }
        const bool full=view.absolute_start_frame().value()==0
            &&view.frame_count().value()
                ==product.upstream.source.frame_count();
        claim.scope_label=full?"Full Track":"Region";
        claim.external_product_owner_confirmed=true;
        auto converted=m21_private::import_real_product_metrics(
            *level.value(),*tonal.value(),claim);
        if(!converted.snapshot)
            return R::failure(failed("M21_AUTHENTIC_PRODUCT_METRICS_PORT_REFUSED"));
        return R::success(std::move(*converted.snapshot));
    }
private:
    [[nodiscard]] static core::Error failed(const char* msg) {
        return core::Error{core::ErrorCode::InvalidState,msg};
    }
};

} // namespace rgsml::app::internal
