#pragma once
// L3-M13: authentic new postmaterialization PCM -> existing M05/M06
// ORIGINAL native feature import. M13 does NOT create a new feature
// definition, extract a proxy from M21 GUI cells, reconstruct missing
// short-term observations or qualify a 27-binding M07 registry.
// The request originates only from the actual Qt Product M12 owner;
// numerical work is performed away from the GUI thread.
#include "l3_m13_product_fresh_postmaterialization_probe.hpp"
#include "l3_m05m06_real_source_bundle.hpp"
#include "l3_m05m06_original_prepared_reference_pair.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::l3_automatch::m13_frozen {
// Canonical fresh-Master identity, also used to resolve the original
// ten native binding families. Never reuse the accepted M10 identity.
[[nodiscard]] inline m05m06_real::SourceIdentity
m13_fresh_master_native_identity(
    const M13ImmutableProductFreshProbeRequest& admitted)
{
    const auto& owner=admitted.exact_materialized_preview_owner();
    if(!owner.replay||admitted.exact_product_ownership_epoch()==0U||
       owner.replay->project_id.is_nil()||owner.replay->source_id.is_nil()||
       owner.replay->reference_id.is_nil()||owner.replay->controller_id.is_nil()||
       owner.replay->plan_id.is_nil()||owner.replay->linked_chain_id.is_nil()||
       owner.replay->exact_plan_checksum.empty())
        return {};
    std::string exact_identity{"rgsml.l3.native.m13.fresh-master/1;"};
    const auto append=[&](const std::string& part){
        exact_identity+=std::to_string(part.size());
        exact_identity+=':';
        exact_identity+=part;
    };
    append(std::to_string(admitted.exact_product_ownership_epoch()));
    append(owner.replay->project_id.to_string());
    append(owner.replay->source_id.to_string());
    append(owner.replay->reference_id.to_string());
    append(owner.replay->controller_id.to_string());
    append(owner.replay->plan_id.to_string());
    append(owner.replay->exact_plan_checksum);
    append(owner.replay->linked_chain_id.to_string());
    return {std::move(exact_identity),
            admitted.exact_product_ownership_epoch(),true};
}
struct M13FreshOriginalNativeSource final {
    // Native M19/M20 source observations from the *actual new* DSP PCM.
    m05m06_real::SourceReadings fresh_full_m19_m20;
    // A full P04 series/bundle is independently optional; missing or
    // unqualified M05/M06 components must not be filled from proxies.
    std::optional<m05m06_real::ProductNativeFeatureBundle>
        original_native_additional_facts;
    [[nodiscard]] constexpr bool authorizes_m13_verified() const noexcept
    {return false;}
};
[[nodiscard]] inline core::Result<M13FreshOriginalNativeSource>
import_m13_product_fresh_original_native_source(
    const M13ImmutableProductFreshProbeRequest& admitted,
    const M13FreshProductMasterProbe& fresh,
    m05m06_real::CommonTonalBandwidth common_tonal,
    double exact_phase6_density_floor,
    std::int64_t phase6_active_frames,
    bool phase6_file_based_lra_trailing_attested,
    bool phase6_source_grid_and_config_attested)
{
    using R=core::Result<M13FreshOriginalNativeSource>;
    const auto fail=[](const char* reason) {
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,reason});
    };
    const auto& owner=admitted.exact_materialized_preview_owner();
    if(!owner.replay||!owner.exact_prepared_owner||
       owner.replay->project_id.is_nil()||
       owner.replay->source_id.is_nil()||
       owner.replay->reference_id.is_nil()||
       owner.replay->controller_id.is_nil()||
       owner.replay->plan_id.is_nil()||
       owner.replay->exact_plan_checksum.empty()||
       admitted.exact_product_ownership_epoch()==0U||
       !fresh.matches_executed_request(admitted)||
       fresh.logical_verification_evaluations!=1U||
       !m12_frozen::m12_native_stage_witness_matches_replay_plan(
           *owner.replay,fresh.stage_witness))
        return fail("L3_M13_FRESH_NATIVE_NO_AUTHENTIC_M12_EXECUTION");
    const auto& view=fresh.fresh_full_master_pcm.view();
    const auto rate=view.format().sample_rate().value();
    if(view.absolute_start_frame().value()!=0||
       view.timebase().frame_domain_id()!=
           audio::FrameDomainId::SOURCE_PROCESSING_RATE||
       view.frame_count().value()<=0||
       view.absolute_range()!=owner.exact_prepared_owner->view().
           absolute_range()||
       view.format()!=owner.exact_prepared_owner->view().format()||
       fresh.fresh_m16.sample_rate_hz()!=rate||
       fresh.fresh_m19.sample_rate_hz!=rate||
       fresh.fresh_m20.sample_rate_hz!=rate||
       fresh.fresh_m19.scope_frame_count!=view.frame_count().value()||
       fresh.fresh_m20.scope_frame_count!=view.frame_count().value()||
       fresh.fresh_m19.scope_begin_frame!=0||
       fresh.fresh_m20.scope_begin_frame!=0)
        return fail("L3_M13_FRESH_NATIVE_TIMELINE_OR_RATE_MISMATCH");
    auto exact_owner=m13_fresh_master_native_identity(admitted);
    if(!exact_owner.full_track_owner_confirmed)
        return fail("L3_M13_FRESH_NATIVE_IDENTITY_INVALID");
    auto observed=m05m06_real::import_actual_full_source(
        fresh.fresh_m19,fresh.fresh_m20,
        exact_owner);
    if(!observed.source)
        return fail("L3_M13_FRESH_NATIVE_ORIGINAL_M19_M20_UNAVAILABLE");
    M13FreshOriginalNativeSource output{
        std::move(*observed.source),std::nullopt};
    if(phase6_source_grid_and_config_attested) {
        if(phase6_active_frames<0||
           phase6_active_frames>view.frame_count().value()||
           !std::isfinite(common_tonal.low_hz)||
           !std::isfinite(common_tonal.high_hz)||
           !std::isfinite(exact_phase6_density_floor)||
           exact_phase6_density_floor<=0.0)
            return fail("L3_M13_FRESH_NATIVE_PHASE6_CONFIG_INVALID");
        output.original_native_additional_facts=
            m05m06_real::bundle_actual_product_m05m06(
                view,fresh.fresh_m19,fresh.fresh_m20,
                std::move(exact_owner),
                common_tonal,exact_phase6_density_floor,
                phase6_active_frames,
                phase6_file_based_lra_trailing_attested);
        // A missing P04 component remains unavailable; M19/M20
        // observations stay valid and are NEVER silently promoted.
    }
    return R::success(std::move(output));
}
struct M13FreshIndependentReferenceReadout final {
    m05m06_real::Pair actual_master_to_original_reference;
    [[nodiscard]] constexpr bool authorizes_m13_verified() const noexcept
    {return false;}
};
[[nodiscard]] inline core::Result<M13FreshIndependentReferenceReadout>
pair_m13_fresh_master_with_independent_original_reference(
    const M13FreshOriginalNativeSource& actual,
    const OriginalOwnedNativeSourceFacts& independent_reference,
    const OfflineProductOwnerStamp& original_reference_owner,
    bool exact_product_reference_scope_still_current)
{
    using R=core::Result<M13FreshIndependentReferenceReadout>;
    const auto fail=[](const char* why) {
        return R::failure(core::Error{core::ErrorCode::InvalidState,why});
    };
    if(!exact_product_reference_scope_still_current||
       !original_reference_owner.populated()||
       !authentic_product_source_identity(
           independent_reference.original_m19_m20.owner,
           original_reference_owner,OriginalProductRole::reference)||
       !actual.fresh_full_m19_m20.owner.full_track_owner_confirmed||
       actual.fresh_full_m19_m20.owner.resource_identity.empty()||
       actual.fresh_full_m19_m20.owner.resource_identity==
           independent_reference.original_m19_m20.owner.resource_identity)
        return fail("L3_M13_INDEPENDENT_REFERENCE_IDENTITY_NOT_CURRENT");
    auto pair=m05m06_real::pair_actual_sources(
        actual.fresh_full_m19_m20,
        independent_reference.original_m19_m20);
    if(!pair.pair)
        return fail("L3_M13_INDEPENDENT_REFERENCE_NATIVE_GEOMETRY_INVALID");
    return R::success(M13FreshIndependentReferenceReadout{
        std::move(*pair.pair)});
}
} // namespace rgsml::app::l3_automatch::m13_frozen
