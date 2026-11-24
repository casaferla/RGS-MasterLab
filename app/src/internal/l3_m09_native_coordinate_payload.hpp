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
} // namespace native_payload_detail
} // namespace rgsml::app::l3_automatch::m09_frozen
