#pragma once
#include "l3_m09_native_family_seed_bank.hpp"

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeBandCoordinateAuthority final {
    IntentRole role;
    core::Uuid band_id;
    ExactBytes semantic_band_key;
    NativeFrequencyBounds bounds;
};
[[nodiscard]] inline std::string_view native_field_key(NativeField field) noexcept {
    switch(field) {
    case NativeField::gain_db:return "gainDb";
    case NativeField::frequency_hz:return "frequencyHz";
    case NativeField::q:return "q";
    case NativeField::shelf_slope:return "shelfSlope";
    case NativeField::range_db:return "rangeDb";
    case NativeField::threshold_dbfs:return "thresholdDbfs";
    case NativeField::ratio:return "ratio";
    case NativeField::knee_db:return "kneeDb";
    case NativeField::attack_ms:return "attackMs";
    case NativeField::release_ms:return "releaseMs";
    case NativeField::rms_ms:return "rmsMs";
    case NativeField::look_ahead_ms:return "lookAheadMs";
    case NativeField::mix_percent:return "mixPercent";
    case NativeField::attack_amount_db:return "attackAmountDb";
    case NativeField::sustain_amount_db:return "sustainAmountDb";
    case NativeField::mid_gain_db:return "midGainDb";
    case NativeField::side_gain_db:return "sideGainDb";
    case NativeField::low_width_percent:return "lowBandWidthPercent";
    default:return {};
    }
}
struct NativeRoleCoordinateAuthority final {
    IntentRole role;
    ExactBytes semantic_node_key;
};
// Semantic keys are owner-allocated, not persistence UUIDs or GUI text.
// Retain all active fields, including those whose neutral seed is zero.
// An over-budget inventory remains over budget; do not truncate it.
[[nodiscard]] inline std::optional<std::vector<NativeRoleCoordinateRequest>>
derive_native_family_coordinate_requests(const NativeFamilySeedBank& bank,
    const AbstractFamilyChoice& family,
    std::span<const NativeRoleCoordinateAuthority> role_keys,
    std::span<const NativeBandCoordinateAuthority> band_keys,std::int64_t rate)
{
    const auto roles=ordered_abstract_roles(family);
    if(roles.size()!=role_keys.size())return {};
    std::vector<NativeRoleCoordinateRequest> out;
    std::size_t consumed_bands=0;
    for(std::size_t i=0;i<roles.size();++i) {
        if(role_keys[i].role!=roles[i]||role_keys[i].semantic_node_key.empty())return {};
        const auto payload=native_family_role_payload(bank,family,roles[i]);
        if(!payload)return {};
        const auto add=[&](NativeField field,const NativeBandCoordinateAuthority* band=nullptr){
            const auto name=native_field_key(field);
            const std::optional<core::Uuid> id=band?std::optional<core::Uuid>{band->band_id}:std::nullopt;
            const std::optional<NativeFrequencyBounds> bounds=band&&field==NativeField::frequency_hz?
                std::optional<NativeFrequencyBounds>{band->bounds}:std::nullopt;
            if(name.empty()||!native_automatic_coordinate_rule(*payload,field,id,bounds,rate))return false;
            ExactBytes key;
            using namespace m10_frozen::p2_cache_detail;
            append_exact_field(key,"rgsml.l3.native-field/1");
            if(band)append_exact_byte_field(key,band->semantic_band_key);
            append_exact_field(key,name);
            out.push_back({roles[i],role_keys[i].semantic_node_key,std::move(key),field,id,bounds});
            return true;
        };
        const auto authority=[&](const core::Uuid& id)->const NativeBandCoordinateAuthority*{
            const auto match=[&](const auto& x){return x.role==roles[i]&&x.band_id==id;};
            const auto it=std::find_if(band_keys.begin(),band_keys.end(),match);
            if(it==band_keys.end()||it->semantic_band_key.empty()||
               std::count_if(band_keys.begin(),band_keys.end(),match)!=1)return nullptr;
            ++consumed_bands;return &*it;
        };
        using F=NativeField;
        if(const auto* eq=std::get_if<dsp::ParametricEqParameters>(&*payload)) {
            for(const auto& b:eq->bands()) {
                const auto* key=authority(b.band_id());
                if(!key||!add(F::frequency_hz,key)||!add(F::gain_db,key)||
                   !add(std::holds_alternative<dsp::BellPayload>(b.payload())?F::q:F::shelf_slope,key))return {};
            }
        } else if(const auto* eq=std::get_if<dsp::DynamicEqParameters>(&*payload)) {
            for(const auto& b:eq->bands()) {
                const auto* key=authority(b.spec().band_id);
                if(!key||!add(F::frequency_hz,key)||
                   !add(std::holds_alternative<dsp::DynamicEqBellShape>(b.spec().shape)?F::q:F::shelf_slope,key))return {};
                for(const auto field:{F::range_db,F::threshold_dbfs,F::ratio,F::rms_ms,F::attack_ms,F::release_ms})
                    if(!add(field,key))return {};
            }
        } else if(std::holds_alternative<dsp::CompressorParameters>(*payload)) {
            for(const auto field:{F::threshold_dbfs,F::ratio,F::knee_db,F::attack_ms,F::release_ms,F::rms_ms,F::look_ahead_ms,F::mix_percent})
                if(!add(field))return {};
        } else if(std::holds_alternative<dsp::TransientShaperParameters>(*payload)) {
            if(family.micro!=MicroMode::sustain&&!add(F::attack_amount_db))return {};
            if(family.micro!=MicroMode::attack&&!add(F::sustain_amount_db))return {};
        } else if(const auto* ms=std::get_if<dsp::StereoMsParameters>(&*payload)) {
            if(!add(F::mid_gain_db)||(!ms->side_muted()&&!add(F::side_gain_db)))return {};
            if(ms->mono_bass_mode()!=dsp::MonoBassMode::OFF&&!add(F::low_width_percent))return {};
        } else if(std::holds_alternative<dsp::GainParameters>(*payload)) {
            if(!add(F::gain_db))return {};
        } else return {};
    }
    if(consumed_bands!=band_keys.size())return {};
    return out;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
