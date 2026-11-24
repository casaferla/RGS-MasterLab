#pragma once
// M09 physical coordinates -> existing validated DSP payloads.
// The frozen M09 inventory supplies bounds, semantic IDs and seeds.
// No implicit default, topology change, MANUAL edit or P2 coordinate.
#include <rgsml/dsp/module_parameter_payload.hpp>
#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::app::l3_automatch::m09_frozen {
enum class NativeField {
    gain_db,frequency_hz,q,shelf_slope,base_gain_db,range_db,
    threshold_dbfs,ratio,knee_db,attack_ms,release_ms,rms_ms,
    look_ahead_ms,mix_percent,makeup_gain_db,attack_amount_db,
    sustain_amount_db,mid_gain_db,side_gain_db,low_width_percent
};
struct NativePhysicalField final {
    NativeField field;
    std::optional<core::Uuid> band_id;
    double value{};
};
[[nodiscard]] inline bool valid_native_physical_fields(
    std::span<const NativePhysicalField> fields)
{
    for(std::size_t i=0;i<fields.size();++i) {
        if(!std::isfinite(fields[i].value)||
           (fields[i].band_id&&fields[i].band_id->is_nil()))return false;
        for(std::size_t j=0;j<i;++j)
            if(fields[i].field==fields[j].field&&
               fields[i].band_id==fields[j].band_id)return false;
    }
    return true;
}
namespace native_payload_detail {
template<class T>
[[nodiscard]] inline std::optional<dsp::ModuleParameterPayload>
as_payload(core::Result<T> result)
{
    if(!result)return std::nullopt;
    return dsp::ModuleParameterPayload{std::move(*result.value())};
}
[[nodiscard]] inline std::optional<dsp::ModuleParameterPayload>
apply_gain(const dsp::GainParameters& seed,
           std::span<const NativePhysicalField> fields)
{
    double gain=seed.gain_db();
    for(const auto& f:fields) {
        if(f.band_id||f.field!=NativeField::gain_db)return std::nullopt;
        gain=f.value;
    }
    return as_payload(dsp::GainParameters::create(gain));
}
[[nodiscard]] inline std::optional<dsp::ModuleParameterPayload>
apply_transient(const dsp::TransientShaperParameters& seed,
                std::span<const NativePhysicalField> fields)
{
    double attack=seed.attack_amount_db(),sustain=seed.sustain_amount_db();
    for(const auto& f:fields) {
        if(f.band_id)return std::nullopt;
        if(f.field==NativeField::attack_amount_db)attack=f.value;
        else if(f.field==NativeField::sustain_amount_db)sustain=f.value;
        else return std::nullopt;
    }
    return as_payload(dsp::TransientShaperParameters::create(attack,sustain));
}
[[nodiscard]] inline std::optional<dsp::ModuleParameterPayload>
apply_stereo(const dsp::StereoMsParameters& seed,
             std::span<const NativePhysicalField> fields)
{
    double mid=seed.mid_gain_db(),side=seed.side_gain_db();
    double width=seed.low_band_width_percent();
    for(const auto& f:fields) {
        if(f.band_id)return std::nullopt;
        if(f.field==NativeField::mid_gain_db)mid=f.value;
        else if(f.field==NativeField::side_gain_db&&!seed.side_muted())side=f.value;
        else if(f.field==NativeField::low_width_percent&&
                seed.mono_bass_mode()!=dsp::MonoBassMode::OFF)width=f.value;
        else return std::nullopt;
    }
    return as_payload(dsp::StereoMsParameters::create(mid,side,seed.side_muted(),
        seed.mono_bass_mode(),seed.mono_bass_cutoff_hz(),width));
}
[[nodiscard]] inline std::optional<dsp::ModuleParameterPayload>
apply_compressor(const dsp::CompressorParameters& seed,
                 std::span<const NativePhysicalField> fields)
{
    double threshold=seed.threshold_dbfs(),ratio=seed.ratio(),knee=seed.knee_db();
    double attack=seed.attack_ms(),release=seed.release_ms(),rms=seed.rms_time_constant_ms();
    double look=seed.look_ahead_ms(),mix=seed.mix_percent(),makeup=seed.makeup_gain_db();
    for(const auto& f:fields) {
        if(f.band_id)return std::nullopt;
        switch(f.field) {
        case NativeField::threshold_dbfs:threshold=f.value;break;
        case NativeField::ratio:ratio=f.value;break;
        case NativeField::knee_db:knee=f.value;break;
        case NativeField::attack_ms:attack=f.value;break;
        case NativeField::release_ms:release=f.value;break;
        case NativeField::rms_ms:rms=f.value;break;
        case NativeField::look_ahead_ms:look=f.value;break;
        case NativeField::mix_percent:mix=f.value;break;
        case NativeField::makeup_gain_db:makeup=f.value;break;
        default:return std::nullopt;
        }
    }
    return as_payload(dsp::CompressorParameters::create(seed.detector_mode(),
        seed.channel_link(),threshold,ratio,knee,attack,release,rms,look,mix,makeup));
}
[[nodiscard]] inline std::optional<dsp::ModuleParameterPayload>
apply_eq(const dsp::ParametricEqParameters& seed,
         std::span<const NativePhysicalField> fields)
{
    auto bands=seed.bands();
    std::size_t matched=0U;
    for(auto& band:bands) {
        auto payload=band.payload();
        for(const auto& f:fields) {
            if(!f.band_id||*f.band_id!=band.band_id())continue;
            if(!band.enabled())return std::nullopt;
            if(auto* bell=std::get_if<dsp::BellPayload>(&payload)) {
                if(f.field==NativeField::frequency_hz)bell->frequency_hz=f.value;
                else if(f.field==NativeField::gain_db)bell->gain_db=f.value;
                else if(f.field==NativeField::q)bell->q=f.value;
                else return std::nullopt;
            }else if(auto* shelf=std::get_if<dsp::ShelfPayload>(&payload)) {
                if(f.field==NativeField::frequency_hz)shelf->frequency_hz=f.value;
                else if(f.field==NativeField::gain_db)shelf->gain_db=f.value;
                else if(f.field==NativeField::shelf_slope)shelf->shelf_slope=f.value;
                else return std::nullopt;
            }else return std::nullopt;
            ++matched;
        }
        auto updated=dsp::EqBandParameters::create(band.band_id(),band.enabled(),
            band.filter_type(),band.routing(),std::move(payload));
        if(!updated)return std::nullopt;
        band=std::move(*updated.value());
    }
    if(matched!=fields.size())return std::nullopt;
    return as_payload(dsp::ParametricEqParameters::create(std::move(bands)));
}
} // namespace native_payload_detail
} // namespace rgsml::app::l3_automatch::m09_frozen
