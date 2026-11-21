#pragma once
// M20 preproduction: read-only C++ presentation -> Qt display data.
// No DSP math in QML and no registration in the product.
#include "internal/m20_presentation_candidate.hpp"
#include <QVariantList>
#include <QVariantMap>
namespace rgsml::analysis::m20_private {
[[nodiscard]] inline QVariantMap to_qt_preview_candidate(
    const M20ReadOnlyViewCandidate& model)
{
    QVariantList spectrum,profile,correlation;
    for(const auto& p:model.spectrum_points) spectrum.append(QVariantMap{
        {"x",p.x},{"y",p.y},{"valid",p.valid},{"zero",p.genuine_zero}});
    for(const auto& b:model.profile_bands) profile.append(QVariantMap{
        {"x",b.plot.x},{"y",b.plot.y},{"valid",b.plot.valid},
        {"zero",b.plot.genuine_zero},
        {"label",QString::fromStdString(b.label)},
        {"support",QString::fromStdString(b.support)}});
    for(const auto& p:model.correlation_recent) correlation.append(QVariantMap{
        {"x",p.x},{"y",p.y},{"valid",p.valid},{"zero",p.genuine_zero}});
    return QVariantMap{
        {"ready",model.any_actual_content},
        {"hasSpectrum",model.has_spectrum},
        {"hasProfile",model.has_profile},
        {"hasWidth",model.has_width},
        {"hasCorrelation",model.has_correlation},
        {"denominator",model.profile_denominator},
        {"supportText",QString::fromStdString(model.support_text)},
        {"widthText",QString::fromStdString(model.width_text)},
        {"p10Text",QString::fromStdString(model.p10_text)},
        {"p50Text",QString::fromStdString(model.p50_text)},
        {"p90Text",QString::fromStdString(model.p90_text)},
        {"spectrumPoints",spectrum},
        {"profileBands",profile},
        {"correlationHistory",correlation}
    };
}
} // namespace rgsml::analysis::m20_private
