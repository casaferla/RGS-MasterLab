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
#include <utility>

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
        if(f.value< -6.0||f.value>6.0)return std::nullopt;
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
        if(f.value< -3.0||f.value>3.0)return std::nullopt;
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
        if(f.field==NativeField::mid_gain_db&&f.value>=-12.0&&f.value<=12.0)mid=f.value;
        else if(f.field==NativeField::side_gain_db&&!seed.side_muted()&&
                f.value>=-3.0&&f.value<=1.5)side=f.value;
        else if(f.field==NativeField::low_width_percent&&
                seed.mono_bass_mode()!=dsp::MonoBassMode::OFF&&
                f.value>=0.0&&f.value<=100.0)width=f.value;
        else return std::nullopt;
    }
    return as_payload(dsp::StereoMsParameters::create(mid,side,seed.side_muted(),
        seed.mono_bass_mode(),seed.mono_bass_cutoff_hz(),width));
}
[[nodiscard]] inline std::optional<dsp::ModuleParameterPayload>
apply_compressor(const dsp::CompressorParameters& seed,
                 std::span<const NativePhysicalField> fields)
{
    if(seed.makeup_gain_db()!=0.0||seed.detector_mode()!=dsp::CompressorDetectorMode::RMS||
       seed.channel_link()==dsp::CompressorChannelLink::DUAL_MONO)return std::nullopt;
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
        case NativeField::makeup_gain_db:return std::nullopt; // fixed zero in automatic use
        default:return std::nullopt;
        }
    }
    if(ratio>2.0)return std::nullopt;
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
        const bool side=band.routing()==dsp::EqRouting::SIDE;
        if(!side&&band.routing()!=dsp::EqRouting::STEREO)return std::nullopt;
        for(const auto& f:fields) {
            if(!f.band_id||*f.band_id!=band.band_id())continue;
            if(!band.enabled())return std::nullopt;
            if(auto* bell=std::get_if<dsp::BellPayload>(&payload)) {
                if(f.field==NativeField::frequency_hz)bell->frequency_hz=f.value;
                else if(f.field==NativeField::gain_db&&std::abs(f.value)<=(side?1.5:3.0))bell->gain_db=f.value;
                else if(f.field==NativeField::q&&f.value>=0.1&&f.value<=(side?1.0:1.5))bell->q=f.value;
                else return std::nullopt;
            }else if(auto* shelf=std::get_if<dsp::ShelfPayload>(&payload)) {
                if(f.field==NativeField::frequency_hz)shelf->frequency_hz=f.value;
                else if(f.field==NativeField::gain_db&&std::abs(f.value)<=(side?1.5:3.0))shelf->gain_db=f.value;
                else if(f.field==NativeField::shelf_slope&&f.value>=0.1&&f.value<=1.0)shelf->shelf_slope=f.value;
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
[[nodiscard]] inline std::optional<dsp::ModuleParameterPayload>
apply_dynamic_eq(const dsp::DynamicEqParameters& seed,
                 std::span<const NativePhysicalField> fields)
{
    auto bands=seed.bands();
    std::size_t matched=0U;
    for(auto& band:bands) {
        auto spec=band.spec();
        if(spec.control.base_gain_db!=0.0||spec.routing!=dsp::EqRouting::STEREO)
            return std::nullopt;
        for(const auto& f:fields) {
            if(!f.band_id||*f.band_id!=spec.band_id)continue;
            if(!spec.enabled)return std::nullopt;
            auto* bell=std::get_if<dsp::DynamicEqBellShape>(&spec.shape);
            auto* shelf=std::get_if<dsp::DynamicEqShelfShape>(&spec.shape);
            auto& c=spec.control;
            switch(f.field) {
            case NativeField::frequency_hz:
                if(bell)bell->frequency_hz=f.value;
                else if(shelf)shelf->frequency_hz=f.value;
                else return std::nullopt;
                break;
            case NativeField::q:if(!bell)return std::nullopt;bell->q=f.value;break;
            case NativeField::shelf_slope:
                if(!shelf)return std::nullopt;
                shelf->shelf_slope=f.value;break;
            case NativeField::base_gain_db:return std::nullopt; // fixed zero in automatic use
            case NativeField::range_db:
                if(f.value< -2.0||f.value>2.0||
                   (c.range_db<0.0&&f.value>0.0)||(c.range_db>0.0&&f.value<0.0))
                    return std::nullopt;
                c.range_db=f.value;break;
            case NativeField::threshold_dbfs:c.threshold_dbfs=f.value;break;
            case NativeField::ratio:c.ratio=f.value;break;
            case NativeField::rms_ms:c.rms_time_constant_ms=f.value;break;
            case NativeField::attack_ms:c.attack_ms=f.value;break;
            case NativeField::release_ms:c.release_ms=f.value;break;
            default:return std::nullopt;
            }
            ++matched;
        }
        auto updated=dsp::DynamicEqBandParameters::create(std::move(spec));
        if(!updated)return std::nullopt;
        band=std::move(*updated.value());
    }
    if(matched!=fields.size())return std::nullopt;
    return as_payload(dsp::DynamicEqParameters::create(std::move(bands)));
}
} // namespace native_payload_detail
// A complete physical vector is applied once per module. Coupled
// validation happens on the final typed payload, not intermediate edits.
[[nodiscard]] inline std::optional<dsp::ModuleParameterPayload>
apply_native_physical_fields(
    const dsp::ModuleParameterPayload& seed,
    std::span<const NativePhysicalField> fields)
{
    if(!valid_native_physical_fields(fields))return std::nullopt;
    using namespace native_payload_detail;
    if(const auto* p=std::get_if<dsp::GainParameters>(&seed))return apply_gain(*p,fields);
    if(const auto* p=std::get_if<dsp::TransientShaperParameters>(&seed))return apply_transient(*p,fields);
    if(const auto* p=std::get_if<dsp::StereoMsParameters>(&seed))return apply_stereo(*p,fields);
    if(const auto* p=std::get_if<dsp::CompressorParameters>(&seed))return apply_compressor(*p,fields);
    if(const auto* p=std::get_if<dsp::ParametricEqParameters>(&seed))return apply_eq(*p,fields);
    if(const auto* p=std::get_if<dsp::DynamicEqParameters>(&seed))return apply_dynamic_eq(*p,fields);
    return std::nullopt; // P2 and WAV Export are outside AutoMatch
}
[[nodiscard]] inline std::optional<double>
read_native_physical_field(const dsp::ModuleParameterPayload& seed,
                           NativeField field,const std::optional<core::Uuid>& band_id)
{
    if(const auto* gain=std::get_if<dsp::GainParameters>(&seed)) {
        if(!band_id&&field==NativeField::gain_db)return gain->gain_db();
    } else if(const auto* transient=std::get_if<dsp::TransientShaperParameters>(&seed)) {
        if(!band_id&&field==NativeField::attack_amount_db)return transient->attack_amount_db();
        if(!band_id&&field==NativeField::sustain_amount_db)return transient->sustain_amount_db();
    } else if(const auto* stereo=std::get_if<dsp::StereoMsParameters>(&seed)) {
        if(!band_id&&field==NativeField::mid_gain_db)return stereo->mid_gain_db();
        if(!band_id&&field==NativeField::side_gain_db&&!stereo->side_muted())return stereo->side_gain_db();
        if(!band_id&&field==NativeField::low_width_percent&&stereo->mono_bass_mode()!=dsp::MonoBassMode::OFF)
            return stereo->low_band_width_percent();
    } else if(const auto* compressor=std::get_if<dsp::CompressorParameters>(&seed)) {
        if(band_id)return std::nullopt;
        switch(field) {
        case NativeField::threshold_dbfs:return compressor->threshold_dbfs();
        case NativeField::ratio:return compressor->ratio();
        case NativeField::knee_db:return compressor->knee_db();
        case NativeField::attack_ms:return compressor->attack_ms();
        case NativeField::release_ms:return compressor->release_ms();
        case NativeField::rms_ms:return compressor->rms_time_constant_ms();
        case NativeField::look_ahead_ms:return compressor->look_ahead_ms();
        case NativeField::mix_percent:return compressor->mix_percent();
        default:return std::nullopt;
        }
    } else if(const auto* eq=std::get_if<dsp::ParametricEqParameters>(&seed)) {
        if(!band_id)return std::nullopt;
        for(const auto& b:eq->bands()) {
            if(b.band_id()!=*band_id||!b.enabled())continue;
            if(const auto* bell=std::get_if<dsp::BellPayload>(&b.payload())) {
                if(field==NativeField::frequency_hz)return bell->frequency_hz;
                if(field==NativeField::gain_db)return bell->gain_db;
                if(field==NativeField::q)return bell->q;
            } else if(const auto* shelf=std::get_if<dsp::ShelfPayload>(&b.payload())) {
                if(field==NativeField::frequency_hz)return shelf->frequency_hz;
                if(field==NativeField::gain_db)return shelf->gain_db;
                if(field==NativeField::shelf_slope)return shelf->shelf_slope;
            }
        }
    } else if(const auto* dynamic_eq=std::get_if<dsp::DynamicEqParameters>(&seed)) {
        if(!band_id)return std::nullopt;
        for(const auto& b:dynamic_eq->bands()) {
            const auto& s=b.spec();
            if(s.band_id!=*band_id||!s.enabled)continue;
            const auto* bell=std::get_if<dsp::DynamicEqBellShape>(&s.shape);
            const auto* shelf=std::get_if<dsp::DynamicEqShelfShape>(&s.shape);
            switch(field) {
            case NativeField::frequency_hz:
                if(bell)return bell->frequency_hz;
                if(shelf)return shelf->frequency_hz;
                return std::nullopt;
            case NativeField::q:if(bell)return bell->q;return std::nullopt;
            case NativeField::shelf_slope:if(shelf)return shelf->shelf_slope;return std::nullopt;
            case NativeField::range_db:return s.control.range_db;
            case NativeField::threshold_dbfs:return s.control.threshold_dbfs;
            case NativeField::ratio:return s.control.ratio;
            case NativeField::rms_ms:return s.control.rms_time_constant_ms;
            case NativeField::attack_ms:return s.control.attack_ms;
            case NativeField::release_ms:return s.control.release_ms;
            default:return std::nullopt;
            }
        }
    }
    return std::nullopt;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
