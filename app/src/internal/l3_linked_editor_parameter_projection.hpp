#pragma once
// Presentation only: project the accepted native C00/C01 parameters with
// the research editor's vocabulary. No default values, edits or telemetry.
#include "l3_linked_editor_snapshot.hpp"
#include <QVariantList>
#include <QVariantMap>

namespace rgsml::app::l3_automatch {
[[nodiscard]] inline QString linked_editor_filter_name(dsp::DynamicEqFilterType type)
{
    switch(type) {
    case dsp::DynamicEqFilterType::BELL:return QStringLiteral("BELL");
    case dsp::DynamicEqFilterType::LOW_SHELF:return QStringLiteral("LOW_SHELF");
    case dsp::DynamicEqFilterType::HIGH_SHELF:return QStringLiteral("HIGH_SHELF");
    }
    return {};
}
[[nodiscard]] inline QString linked_editor_routing_name(dsp::EqRouting route)
{
    switch(route) {
    case dsp::EqRouting::STEREO:return QStringLiteral("STEREO");
    case dsp::EqRouting::MID:return QStringLiteral("MID");
    case dsp::EqRouting::SIDE:return QStringLiteral("SIDE");
    case dsp::EqRouting::LEFT:return QStringLiteral("LEFT");
    case dsp::EqRouting::RIGHT:return QStringLiteral("RIGHT");
    }
    return {};
}
[[nodiscard]] inline QVariantMap project_linked_dynamic_band(
    const dsp::DynamicEqBandSpec& band)
{
    const auto filter=linked_editor_filter_name(band.filter_type);
    const auto routing=linked_editor_routing_name(band.routing);
    if(filter.isEmpty()||routing.isEmpty()||band.band_id.is_nil())return {};
    const auto& c=band.control;
    QVariantMap out{
        {"bandId",QString::fromStdString(band.band_id.to_string())},
        {"enabled",band.enabled},{"filter",filter},{"routing",routing},
        {"frequency",std::visit([](const auto& b){return b.frequency_hz;},band.shape)},
        {"gain",c.base_gain_db},{"gainApplicable",true},
        {"dynamics",QVariantMap{{"range",c.range_db},{"threshold",c.threshold_dbfs},
             {"ratio",c.ratio},{"rmsTime",c.rms_time_constant_ms},
             {"attack",c.attack_ms},{"release",c.release_ms}}}
    };
    if(const auto* bell=std::get_if<dsp::DynamicEqBellShape>(&band.shape)) {
        if(band.filter_type!=dsp::DynamicEqFilterType::BELL)return {};
        out.insert("q",bell->q);
        out.insert("qApplicable",true);
        out.insert("shelfSlopeApplicable",false);
    } else if(const auto* shelf=std::get_if<dsp::DynamicEqShelfShape>(&band.shape)) {
        if(band.filter_type==dsp::DynamicEqFilterType::BELL)return {};
        out.insert("shelfSlope",shelf->shelf_slope);
        out.insert("shelfSlopeApplicable",true);
        out.insert("qApplicable",false);
    } else return {};
    return out;
}
[[nodiscard]] inline QVariantMap project_linked_editor_parameters(
    const LinkedEditorSnapshot& snapshot)
{
    const auto type=snapshot.instance.module_type_id();
    QVariantMap out{{"manualEditable",false},{"activityAuthorized",false},
        {"instanceId",QString::fromStdString(snapshot.instance.instance_id().to_string())}};
    if(type=="rgsml.dsp.transient-shaper") {
        const auto* p=std::get_if<dsp::TransientShaperParameters>(&snapshot.parameters);
        if(!p)return {};
        out.insert("attackAmountDb",p->attack_amount_db());
        out.insert("sustainAmountDb",p->sustain_amount_db());
        return out;
    }
    if(type!="rgsml.dsp.dynamic-eq")return {};
    const auto* p=std::get_if<dsp::DynamicEqParameters>(&snapshot.parameters);
    if(!p||p->bands().empty())return {};
    QVariantList bands;
    for(const auto& band:p->bands()) {
        auto entry=project_linked_dynamic_band(band.spec());
        if(entry.isEmpty())return {};
        bands.append(entry);
    }
    out.insert("bands",bands);
    return out;
}
} // namespace rgsml::app::l3_automatch
