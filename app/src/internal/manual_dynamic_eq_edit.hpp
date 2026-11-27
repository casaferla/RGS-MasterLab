#pragma once

// Shared native C01 manual edit primitives. Every edit constructs a new
// validated parameter value; neither a GUI draft nor a linked ticket owns it.
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <rgsml/core/error.hpp>
#include <QString>
#include <functional>
#include <utility>

namespace rgsml::app::manual_dynamic_eq {
using namespace rgsml::dsp;
using Result=core::Result<DynamicEqParameters>;
inline Result invalid(const char* reason) {
    return Result::failure(core::Error{core::ErrorCode::InvalidArgument,reason});
}
inline Result replace_band(const DynamicEqParameters& current,
    const core::Uuid& id,const std::function<bool(DynamicEqBandSpec&)>& edit)
{
    auto bands=current.bands();
    for(auto& band:bands) {
        if(band.spec().band_id!=id)continue;
        auto spec=band.spec();
        if(!edit(spec))return invalid("DYNAMIC_EQ_FIELD_NOT_APPLICABLE");
        auto validated=DynamicEqBandParameters::create(std::move(spec));
        if(!validated)return Result::failure(*validated.error());
        band=std::move(*validated.value());
        return DynamicEqParameters::create(std::move(bands));
    }
    return invalid("DYNAMIC_EQ_BAND_NO_LONGER_CURRENT");
}
inline Result numeric(const DynamicEqParameters& p,const core::Uuid& id,
                      const QString& field,double value) {
    return replace_band(p,id,[&](DynamicEqBandSpec& s){
        auto& c=s.control;
        if(field=="frequency")std::visit([&](auto& shape){shape.frequency_hz=value;},s.shape);
        else if(field=="gain")c.base_gain_db=value;
        else if(field=="range")c.range_db=value;
        else if(field=="threshold")c.threshold_dbfs=value;
        else if(field=="ratio")c.ratio=value;
        else if(field=="rmsTime")c.rms_time_constant_ms=value;
        else if(field=="attack")c.attack_ms=value;
        else if(field=="release")c.release_ms=value;
        else if(field=="q") {
            auto* bell=std::get_if<DynamicEqBellShape>(&s.shape);
            if(!bell)return false;
            bell->q=value;
        } else if(field=="shelfSlope") {
            auto* shelf=std::get_if<DynamicEqShelfShape>(&s.shape);
            if(!shelf)return false;
            shelf->shelf_slope=value;
        } else return false;
        return true;
    });
}
inline Result filter(const DynamicEqParameters& p,const core::Uuid& id,
                     DynamicEqFilterType type) {
    return replace_band(p,id,[&](DynamicEqBandSpec& s){
        if(s.filter_type==type)return true;
        if(type!=DynamicEqFilterType::BELL&&type!=DynamicEqFilterType::LOW_SHELF&&
           type!=DynamicEqFilterType::HIGH_SHELF)return false;
        const double hz=std::visit([](const auto& shape){return shape.frequency_hz;},s.shape);
        if(type==DynamicEqFilterType::BELL)s.shape=DynamicEqBellShape{hz,0.707};
        else if(s.filter_type==DynamicEqFilterType::BELL)s.shape=DynamicEqShelfShape{hz,1.0};
        s.filter_type=type;return true;
    });
}
inline Result routing(const DynamicEqParameters& p,const core::Uuid& id,EqRouting route) {
    return replace_band(p,id,[&](DynamicEqBandSpec& s){s.routing=route;return true;});
}
inline Result enabled(const DynamicEqParameters& p,const core::Uuid& id,bool enabled) {
    return replace_band(p,id,[&](DynamicEqBandSpec& s){s.enabled=enabled;return true;});
}
inline Result graph(const DynamicEqParameters& p,const core::Uuid& id,double hz,double gain) {
    return replace_band(p,id,[&](DynamicEqBandSpec& s){
        std::visit([&](auto& shape){shape.frequency_hz=hz;},s.shape);
        s.control.base_gain_db=gain;return true;
    });
}
inline Result add(const DynamicEqParameters& p,const core::Uuid& id) {
    auto bands=p.bands();
    DynamicEqBandSpec spec;spec.band_id=id;
    auto band=DynamicEqBandParameters::create(std::move(spec));
    if(!band)return Result::failure(*band.error());
    bands.push_back(std::move(*band.value()));
    return DynamicEqParameters::create(std::move(bands));
}
inline Result remove(const DynamicEqParameters& p,const core::Uuid& id) {
    auto bands=p.bands();
    for(auto i=bands.begin();i!=bands.end();++i)if(i->spec().band_id==id) {
        bands.erase(i);return DynamicEqParameters::create(std::move(bands));
    }
    return invalid("DYNAMIC_EQ_BAND_NO_LONGER_CURRENT");
}
inline Result reset_flat(const DynamicEqParameters& p) {
    // Keep identities, routing and shapes. Reset both static and dynamic gain.
    auto bands=p.bands();
    for(auto& band:bands) {
        auto spec=band.spec();
        spec.control=DynamicEqControl{};
        auto next=DynamicEqBandParameters::create(std::move(spec));
        if(!next)return Result::failure(*next.error());
        band=std::move(*next.value());
    }
    return DynamicEqParameters::create(std::move(bands));
}
}
