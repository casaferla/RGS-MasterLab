#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <rgsml/core/error.hpp>
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <utility>

namespace rgsml::dsp {
namespace {

[[nodiscard]] core::Error bad(std::string reason) {
    return core::Error{core::ErrorCode::InvalidArgument,
        "L3_M01_DYNAMIC_EQ_PARAMETER_INVALID",
        {{"category",std::move(reason)}}};
}
[[nodiscard]] bool range(double x,double lo,double hi) noexcept {
    return std::isfinite(x)&&x>=lo&&x<=hi;
}
[[nodiscard]] bool allowed(EqRouting x) noexcept {
    return x==EqRouting::STEREO||x==EqRouting::MID||
        x==EqRouting::SIDE||x==EqRouting::LEFT||x==EqRouting::RIGHT;
}
[[nodiscard]] double freq(const DynamicEqShape& shape) noexcept {
    return std::visit([](const auto& x){return x.frequency_hz;},shape);
}
} // namespace

DynamicEqBandParameters::DynamicEqBandParameters(DynamicEqBandSpec spec)
    :spec_(std::move(spec)) {}

core::Result<DynamicEqBandParameters>
DynamicEqBandParameters::create(DynamicEqBandSpec spec) {
    using R=core::Result<DynamicEqBandParameters>;
    if(spec.band_id.is_nil())return R::failure(bad("NIL_BAND_UUID"));
    if(!allowed(spec.routing))return R::failure(bad("INVALID_ROUTE"));
    if(spec.filter_type!=DynamicEqFilterType::BELL
       &&spec.filter_type!=DynamicEqFilterType::LOW_SHELF
       &&spec.filter_type!=DynamicEqFilterType::HIGH_SHELF)
        return R::failure(bad("INVALID_FILTER_TYPE"));

    if(spec.filter_type==DynamicEqFilterType::BELL) {
        const auto* bell=std::get_if<DynamicEqBellShape>(&spec.shape);
        if(!bell || !range(bell->q,0.10,12.0))
            return R::failure(bad("BELL_REQUIRES_VALID_Q"));
    }else{
        const auto* shelf=std::get_if<DynamicEqShelfShape>(&spec.shape);
        if(!shelf ||!range(shelf->shelf_slope,0.10,1.0))
            return R::failure(bad("SHELF_REQUIRES_VALID_SLOPE"));
    }
    if(!range(freq(spec.shape),20.0,20000.0))
        return R::failure(bad("INVALID_FREQUENCY"));

    const auto& v=spec.control;
    if(!range(v.base_gain_db,-18.0,18.0)
       ||!range(v.range_db,-18.0,18.0)
       ||!range(v.base_gain_db+v.range_db,-18.0,18.0))
        return R::failure(bad("SECTION_GAIN_ENDPOINTS"));
    if(!range(v.threshold_dbfs,-120.0,0.0)
       ||!range(v.ratio,1.0,20.0)
       ||!range(v.rms_time_constant_ms,1.0,500.0)
       ||!range(v.attack_ms,0.1,500.0)
       ||!range(v.release_ms,1.0,5000.0))
        return R::failure(bad("DYNAMIC_CONTROL_OUT_OF_RANGE"));

    // Zero-range controls still need a valid stable persisted band identity.
    // No runtime clamp, rearrangement, coefficient clock or automatic Gold.
    return R::success(DynamicEqBandParameters{std::move(spec)});
}
const DynamicEqBandSpec& DynamicEqBandParameters::spec() const noexcept {
    return spec_;
}
core::Status DynamicEqBandParameters::validate_for_audio(
    std::uint32_t sample_rate_hz,std::uint32_t channels) const
{
    if(sample_rate_hz==0 ||channels<1 ||channels>2)
        return core::Status::failure(bad("INVALID_AUDIO_FORMAT"));
    if(channels==1&&spec_.enabled&&spec_.routing!=EqRouting::STEREO)
        return core::Status::failure(bad("MONO_ROUTE_REQUIRES_STEREO"));
    // Only enabled bands instantiate coefficients, detector and audio states.
    if(spec_.enabled &&
       (sample_rate_hz==0 ||
        freq(spec_.shape)>std::min(20000.0,0.45*sample_rate_hz)))
        return core::Status::failure(bad("FREQUENCY_EXCEEDS_0_45_FS"));
    return core::Status::success();
}
DynamicEqParameters::DynamicEqParameters(
    std::vector<DynamicEqBandParameters> bands)
    :bands_(std::move(bands)) {}
core::Result<DynamicEqParameters> DynamicEqParameters::create(
    std::vector<DynamicEqBandParameters> bands)
{
    using R=core::Result<DynamicEqParameters>;
    if(bands.empty()||bands.size()>6)
        return R::failure(bad("BAND_COUNT_1_TO_6"));
    std::set<core::Uuid> ids;
    for(const auto& b:bands)
        if(!ids.insert(b.spec().band_id).second)
            return R::failure(bad("DUPLICATE_BAND_UUID"));
    // Deliberately preserve persisted band order; never sort by frequency.
    return R::success(DynamicEqParameters{std::move(bands)});
}
core::Result<DynamicEqParameters> DynamicEqParameters::create_default(
    core::Uuid band_id)
{
    DynamicEqBandSpec spec;
    spec.band_id=band_id;
    auto band=DynamicEqBandParameters::create(std::move(spec));
    if(!band)return core::Result<DynamicEqParameters>::failure(*band.error());
    std::vector<DynamicEqBandParameters> bands;
    bands.push_back(std::move(*band.value()));
    return create(std::move(bands));
}
const std::vector<DynamicEqBandParameters>&
DynamicEqParameters::bands() const noexcept {return bands_;}
core::Status DynamicEqParameters::validate_for_audio(
    std::uint32_t sample_rate_hz,std::uint32_t channels) const
{
    if(sample_rate_hz==0 ||channels<1 ||channels>2)
        return core::Status::failure(bad("INVALID_AUDIO_FORMAT"));
    for(const auto& band:bands_) {
        auto checked=band.validate_for_audio(sample_rate_hz,channels);
        if(!checked)return checked;
    }
    return core::Status::success();
}

} // namespace rgsml::dsp
