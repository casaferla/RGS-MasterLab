#pragma once
// Qt owner-thread boundary: accepted Product and audible transport must agree.
#include "l3_linked_editor_snapshot.hpp"
#include "audition_source_selector.hpp"
#include "playback_transport_view_model.hpp"
#include <rgsml/render/linked_activity_playback_projection.hpp>
#include <QVariantMap>
#include <QVariantList>

namespace rgsml::app::l3_automatch {
[[nodiscard]] inline render::linked_activity::LinkedActivityWindow resolve_linked_activity(
    const LinkedEditorSnapshot& snapshot,const MasteringPreviewController& preview,
    const AuditionSourceSelector& selector,const PlaybackTransportViewModel& transport)
{
    if(QThread::currentThread()!=preview.thread()||
       QThread::currentThread()!=selector.thread()||
       QThread::currentThread()!=transport.thread()||
       !snapshot.exact_owner||!snapshot.exact_owner->replay||
       preview.private_m12_product_preview_ticket()!=snapshot.exact_owner||
       !preview.published_preview_matches_current_bindings()||
       selector.active_target()!=AuditionTarget::PROCESSED||
       selector.prepared_realization_snapshot()!=snapshot.exact_owner->exact_prepared_owner)
        return {};
    const auto pcm=selector.processed_realization_snapshot();
    const auto id=selector.processed_realization_id();
    const auto rate=transport.linked_activity_source_rate();
    const auto audible=transport.playback_snapshot();
    if(!pcm||!id||!rate||!audible)return {};
    const auto& replay=*snapshot.exact_owner->replay;
    return render::linked_activity::project_linked_activity(*pcm,
        {*id,replay.exact_plan_checksum,replay.reference_linked.revision(),
         audio::FrameDomainId::SOURCE_PROCESSING_RATE,*rate,true,snapshot.instance.active()},
        *audible.value(),snapshot.instance.instance_id());
}

[[nodiscard]] inline QVariantMap project_linked_activity_qt(
    const render::linked_activity::LinkedActivityWindow& window)
{
    constexpr std::uint64_t safe=9007199254740991ULL;
    if(!window.available()||window.realization_id.value==0||
       window.audible_frame>safe||window.sample_rate_hz==0||
       window.sample_rate_hz>192000U||
       window.dynamic.size()+window.transient.size()>4096U||
       (!window.dynamic.empty()&&!window.transient.empty()))return {};
    QVariantList buckets;
    const auto valid_range=[&](auto begin,auto end){
        return begin<end&&end<=window.audible_frame;
    };
    for(const auto& b:window.transient) {
        if(!valid_range(b.frame_begin,b.frame_end_exclusive))return {};
        buckets.append(QVariantMap{
            {"frameBegin",double(b.frame_begin)},{"frameEndExclusive",double(b.frame_end_exclusive)},
            {"attackMinDb",b.attack_min_db},{"attackMaxDb",b.attack_max_db},{"attackLastDb",b.attack_last_db},
            {"sustainMinDb",b.sustain_min_db},{"sustainMaxDb",b.sustain_max_db},{"sustainLastDb",b.sustain_last_db}});
    }
    int band_count=0;
    for(const auto& b:window.dynamic) {
        if(!valid_range(b.frame_begin,b.frame_end_exclusive)||b.band_count==0||b.band_count>6)return {};
        if(band_count!=0&&band_count!=static_cast<int>(b.band_count))return {};
        band_count=static_cast<int>(b.band_count);
        QVariantList bands;
        for(std::size_t i=0;i<b.band_count;++i) {
            const auto& v=b.bands[i];
            bands.append(QVariantMap{{"bandId",QString::fromStdString(v.band_id.to_string())},
                {"gainMinDb",v.gain_min_db},{"gainMaxDb",v.gain_max_db},{"gainLastDb",v.gain_last_db},
                {"effectiveGainLastDb",v.effective_gain_last_db},{"signedTargetLastDb",v.signed_target_last_db}});
        }
        buckets.append(QVariantMap{{"frameBegin",double(b.frame_begin)},
            {"frameEndExclusive",double(b.frame_end_exclusive)},{"bands",bands}});
    }
    return {{"available",true},{"buckets",buckets},{"bandCount",band_count},
        {"audibleFrame",double(window.audible_frame)},{"sampleRateHz",int(window.sample_rate_hz)},
        {"realizationId",QString::number(window.realization_id.value)},
        {"stateText",window.paused?QStringLiteral("PAUSED"):QStringLiteral("PLAYBACK SYNCED")}};
}
} // namespace rgsml::app::l3_automatch
