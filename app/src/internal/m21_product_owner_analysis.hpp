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
#include <rgsml/analysis/streamed_reference_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/resource_reference.hpp>
#include <QThread>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
        std::int64_t source_full_frames{0};
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
        if(!prepared ||product.upstream.source.frame_count()<=0
           ||(static_cast<bool>(processed)!=id.has_value())
           ||(id&&id->value==0))
            return R::failure(failed("M21_CAPTURE_NO_AUTHENTIC_PREPARED_OR_PROCESSED"));
        if(prepared->view().frame_count().value()<=0 ||
           (processed&&processed->view().frame_count().value()<=0))
            return R::failure(failed("M21_CAPTURE_EMPTY_REAL_RENDER"));
        return R::success(Capture{
            *owner,source->reference(),product.upstream.source.frame_count(),
            std::move(prepared),std::move(processed),id});
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
            &&product.upstream.source.frame_count()==expected.source_full_frames
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

    // Pure I/O + numerical worker operation: NEVER touches Qt, SOURCE,
    // Project, a current M18 epoch, or the GUI. The GoldCapture was
    // obtained earlier under the real Qt owner. Its exact WAV identity,
    // sample rate, frame count and layout are checked again on OPEN.
    // This intentionally uses the existing WindowsResourceReader,
    // canonical WavReader, M19 and M20. It does not call the renderer,
    // create a Gold playback identity, or substitute SOURCE PCM.
    //
    // The current whole-resource M16 API requires an owning AudioBuffer:
    // until a bounded block-streaming implementation exists, refuse any
    // decode over 128 MiB rather than allocating unbounded track storage.
    // Never schedule this function on the UI thread for real programmes.
    // After worker completion the Qt owner MUST call gold_still_current
    // before publishing the returned Snapshot.
    [[nodiscard]] static core::Result<m21_private::Snapshot>
    measure_gold_worker(const GoldCapture& captured)
    {
        using R=core::Result<m21_private::Snapshot>;
        constexpr std::uint64_t kMaxWholePcmBytes=128ULL*1024ULL*1024ULL;
        const auto frameCount=captured.full_frames;
        const auto channels=captured.channels;
        if(!captured.gold.permissions().can_read()
           ||captured.owner.epoch==0
           ||!captured.owner.identity.valid()
           ||frameCount<=0||(channels!=1U&&channels!=2U)
           ||(captured.sample_rate_hz!=44100U
              &&captured.sample_rate_hz!=48000U
              &&captured.sample_rate_hz!=88200U
              &&captured.sample_rate_hz!=96000U
              &&captured.sample_rate_hz!=176400U
              &&captured.sample_rate_hz!=192000U))
            return R::failure(failed("M21_GOLD_INVALID_CAPTURE_OR_WAV_FORMAT"));
        // Keep the already validated original path for small WAVs. Use
        // bounded PCM only where old whole-resource storage was unsafe.
        // This is not a replacement M16 True Peak implementation.
        const bool streaming=
            static_cast<std::uint64_t>(frameCount)>
                kMaxWholePcmBytes/(sizeof(double)*channels);
        auto endpoint=platform::windows::WindowsResourceReader::
            open_read_only(captured.gold);
        if(!endpoint)return R::failure(*endpoint.error());
        std::unique_ptr<core::IResourceReader> reader=
            std::move(*endpoint.value());
        auto opened=audio::WavReader::open(std::move(reader));
        if(!opened)return R::failure(*opened.error());
        auto& wave=**opened.value();
        const auto& info=wave.info();
        if(info.frame_count().value()!=frameCount
           ||info.audio_format().sample_rate().value()!=
                static_cast<std::int64_t>(captured.sample_rate_hz)
           ||info.audio_format().channel_count()!=channels)
            return R::failure(failed("M21_GOLD_WAV_CHANGED_SINCE_OWNER_CAPTURE"));

        analysis::BasicLevelAnalysisResult level;
        analysis::TonalStereoAnalysisResult stereo;
        if(streaming){
            // Same independent WavReader across all absolute-frame passes.
            // The adapter checks each full read and never retains track PCM.
            auto measured=analysis::measure_streamed_reference_scope(
                info.audio_format(),info.frame_count(),
                [&wave](core::FrameIndex start,audio::MutableAudioBufferView view) {
                    return wave.read_frames(start,view);
                });
            if(!measured)return R::failure(*measured.error());
            level=std::move(measured.value()->level);
            stereo=std::move(measured.value()->tonal);
        }else{
            auto pcm=audio::AudioBuffer::create(
                info.audio_format(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
                core::FrameIndex{0},info.frame_count());
            if(!pcm)return R::failure(*pcm.error());
            const auto decoded=wave.read_frames(
                core::FrameIndex{0},pcm.value()->mutable_view());
            if(!decoded)return R::failure(*decoded.error());
            if(*decoded.value()!=info.frame_count())
                return R::failure(failed("M21_GOLD_WAV_SHORT_OR_PARTIAL_DECODE"));
            auto measuredLevel=analysis::measure_basic_level_full_resource(
                *pcm.value());
            if(!measuredLevel)return R::failure(*measuredLevel.error());
            auto measuredTonal=measure_tonal_with_existing_m19_gate(
                pcm.value()->view(),*measuredLevel.value());
            if(!measuredTonal)return R::failure(*measuredTonal.error());
            level=std::move(*measuredLevel.value());
            stereo=std::move(*measuredTonal.value());
        }
        const auto closed=wave.close();
        if(!closed)return R::failure(*closed.error());

        m21_private::ProductMetricsOwnerClaim claim;
        claim.column=Column::gold;
        claim.generation_token=captured.owner.epoch;
        claim.exact_realization_id="gold-file-generation:"+
            std::to_string(captured.owner.epoch)+"|"+
            captured.gold.provider_id()+"|"+captured.gold.locator();
        claim.scope_label="Full Track";
        claim.external_product_owner_confirmed=true;
        auto imported=m21_private::import_real_product_metrics(
            level,stereo,claim);
        if(!imported.snapshot)
            return R::failure(failed("M21_GOLD_TYPED_IMPORT_REFUSED"));
        return R::success(std::move(*imported.snapshot));
    }

    // Worker-safe numerical half of the authenticated PREPARED/PROCESSED
    // Product pipeline. It reads ONLY retained immutable shared render
    // owners and frozen SOURCE provenance from Capture; no Qt QObject,
    // fanout, mutex, playback or UI member is touched. An admitted
    // capture can become stale while the worker runs: this function
    // NEVER certifies that its output may be published. Recheck with
    // still_current() on the owner thread after completion.
    [[nodiscard]] static core::Result<m21_private::Snapshot>
    measure_render_worker(const Capture& captured,Column column)
    {
        using R=core::Result<m21_private::Snapshot>;
        if(column==Column::gold)
            return R::failure(failed("M21_GOLD_REQUIRES_INDEPENDENT_RESOURCE_OWNER"));
        if(captured.owner.epoch==0||!captured.owner.identity.valid()
           ||captured.source_full_frames<=0||!captured.prepared
           ||!captured.source.permissions().can_read()
           ||(static_cast<bool>(captured.processed)
                !=captured.processed_realization_id.has_value()))
            return R::failure(failed("M21_WORKER_REQUIRES_AUTHENTIC_RETAINED_CAPTURE"));
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
        const auto tonal=measure_tonal_with_existing_m19_gate(
            view,*level.value());
        if(!tonal)return R::failure(*tonal.error());
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
            &&view.frame_count().value()==captured.source_full_frames;
        claim.scope_label=full?"Full Track":"Region";
        claim.external_product_owner_confirmed=true;
        auto converted=m21_private::import_real_product_metrics(
            *level.value(),*tonal.value(),claim);
        if(!converted.snapshot)
            return R::failure(failed("M21_AUTHENTIC_PRODUCT_METRICS_PORT_REFUSED"));
        return R::success(std::move(*converted.snapshot));
    }

    // Synchronous Qt Product reference path for focused native tests.
    // Real application integration should dispatch measure_render_worker
    // outside Qt, then call still_current on the Qt owner before
    // publishing. This wrapper proves the same pre/post gate locally,
    // but MUST NOT be invoked in a GUI signal on long programmes.
    [[nodiscard]] static core::Result<m21_private::Snapshot>
    measure_current(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const Capture& captured,Column column)
    {
        using R=core::Result<m21_private::Snapshot>;
        if(!still_current(product,fanout,selector,captured))
            return R::failure(failed("M21_CAPTURE_STALE_BEFORE_ANALYSIS"));
        auto result=measure_render_worker(captured,column);
        if(!result)return result;
        if(!still_current(product,fanout,selector,captured))
            return R::failure(failed("M21_CAPTURE_STALE_AFTER_ANALYSIS"));
        return result;
    }
private:
    // The REAL existing M19 LUFS producer decides the P06 active union.
    // M20 consumes those exact intervals through its ALREADY published
    // TonalStereoTrustedSupport: no second K gate or PSD/FFT engine.
    // Do not claim a canonical spectral-frame mask or per-band support
    // from interval evidence; those separate inputs stay absent/masked.
    [[nodiscard]] static core::Result<analysis::TonalStereoAnalysisResult>
    measure_tonal_with_existing_m19_gate(
        const audio::AudioBufferView& view,
        const analysis::BasicLevelAnalysisResult& level)
    {
        std::vector<analysis::TonalStereoRetainedInterval> retained;
        retained.reserve(level.retained_block_union.size());
        for(const auto& interval:level.retained_block_union)
            retained.push_back({interval.begin,interval.end});
        const analysis::TonalStereoTrustedSupport supplied{
            false,{},level.retained_block_disclosure_complete,
            retained,{}};
        return analysis::measure_tonal_stereo_scope(
            view,view.absolute_start_frame(),view.frame_count(),
            view.timebase().frame_domain_id(),supplied);
    }

    [[nodiscard]] static core::Error failed(const char* msg) {
        return core::Error{core::ErrorCode::InvalidState,msg};
    }
};

} // namespace rgsml::app::internal
