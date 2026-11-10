#include "m21_product_status_view_model.hpp"
#include <QThread>
#include <QVariantList>
#include <array>
#include <cmath>
#include <utility>

namespace rgsml::app {
M21ProductStatusViewModel::M21ProductStatusViewModel(
    Controllers product,const Fanout& fanout,
    AuditionSourceSelector& selector,GoldSelectionViewModel& gold,
    const Inbox& inbox,QObject* parent)
    : QObject(parent),product_(product),fanout_(&fanout),
      selector_(&selector),gold_(&gold),inbox_(&inbox)
{
    refresh();
}
bool M21ProductStatusViewModel::owner_thread() const noexcept {
    return QThread::currentThread()==thread()
        &&thread()==selector_->thread()
        &&thread()==gold_->thread()
        &&thread()==product_.session.thread();
}
// Limited M19/M20/M21 Product numerical presentation: analysisReadouts
// is visible in DEVELOPMENT even before mathematical Gate G, as explicitly
// authorized by PO. phase6ReleaseReadouts remains hard-gated for certified
// use. Both reuse one source of truth, never invent values or qualify data.
QVariantMap M21ProductStatusViewModel::phase6_release_readouts() const
{
    // Certified release channel stays closed until a real Gate G issuer.
    return production_authorized() ? analysis_readouts() : QVariantMap{};
}

// Authentic diagnostic readout for the PO-approved development panels.
// The source is the EXISTING single inbox; no numerical policy is invented.
// CURRENT is independently checked on every Qt owner-thread access.
QVariantMap M21ProductStatusViewModel::analysis_readouts() const
{
    QVariantMap released;
    if(!owner_thread() || !inbox_)
        return released;
    const auto evidence=inbox_->view_current(
        product_,*fanout_,*selector_,*gold_);
    if(!evidence.independent_scopes)
        return released;

    const auto read=[&](std::size_t role,std::size_t metric)->QVariant {
        // No numerical M16 True Peak has been release-qualified. The
        // visible row remains absent regardless of private PCM evidence.
        if(metric>=m21_private::kMetricSpecs.size() || metric==3U ||
           role>=evidence.columns.size() || !evidence.columns[role])
            return {};
        const auto& record=*evidence.columns[role];
        if(record.lifecycle!=m21_private::Lifecycle::current ||
           record.generation_token==0 || record.resource_identity.empty() ||
           record.scope_frames==0 || record.sample_rate_hz==0)
            return {};
        const auto& cell=record.metrics[metric];
        if(cell.quality!=m21_private::Quality::present ||
           !cell.unrounded || !std::isfinite(*cell.unrounded))
            return {};
        return QVariant::fromValue(*cell.unrounded);
    };
    const auto released_delta=[&](const m21_private::Cell& cell,
                                  std::size_t metric)->QVariant {
        if(metric==3U || cell.quality!=m21_private::Quality::present ||
           !cell.unrounded || !std::isfinite(*cell.unrounded))
            return {};
        return QVariant::fromValue(*cell.unrounded);
    };

    // The existing Delta engine already computed REF minus Current
    // on unrounded operands; never recompute or align external clocks.
    QVariantList rows;
    static constexpr std::array<const char*,9> ids{{
        "lufs","rms","samplePeak","truePeak","crest",
        "width","P10","P50","P90"}};
    for(std::size_t i=0;i<ids.size();++i) {
        QVariantMap row;
        row.insert(QStringLiteral("key"),QString::fromLatin1(ids[i]));
        row.insert(QStringLiteral("label"),QString::fromLatin1(
            m21_private::kMetricSpecs[i].name));
        row.insert(QStringLiteral("unit"),QString::fromLatin1(
            m21_private::kMetricSpecs[i].absolute_unit));
        row.insert(QStringLiteral("deltaUnit"),QString::fromLatin1(
            m21_private::kMetricSpecs[i].delta_unit));
        row.insert(QStringLiteral("prepared"),read(0U,i));
        row.insert(QStringLiteral("reference"),read(1U,i));
        row.insert(QStringLiteral("processed"),read(2U,i));
        row.insert(QStringLiteral("deltaPrepared"),
                   released_delta(evidence.prepared_to_gold[i],i));
        row.insert(QStringLiteral("deltaProcessed"),
                   released_delta(evidence.processed_to_gold[i],i));
        rows.append(row);
    }

    // Analysis editors choose sources independently. Their selector
    // already lives on this Qt owner; do not infer a fallback role.
    const auto m19Role=static_cast<std::size_t>(routing_.selected(
        Bridge::AnalysisPanel::levels_m19));
    const auto m20Role=static_cast<std::size_t>(routing_.selected(
        Bridge::AnalysisPanel::tonal_m20));
    QVariantMap level,stereo;
    for(std::size_t i=0;i<ids.size();++i) {
        const auto label=QString::fromLatin1(ids[i]);
        if(i<5U) level.insert(label,read(m19Role,i));
        else stereo.insert(label,read(m20Role,i));
    }

    // Static genuine mean PSD from the authenticated M20 snapshot;
    // preserve physical Hz/power, never guess a normalized band shape.
    QVariantList spectrum;
    if(m20Role<evidence.columns.size() &&evidence.columns[m20Role]) {
        const auto& s=*evidence.columns[m20Role];
        if(s.lifecycle==m21_private::Lifecycle::current &&
           s.generation_token!=0 &&!s.resource_identity.empty() &&
           s.scope_frames>0 &&s.sample_rate_hz>0 &&
           s.average_spectrum.size()<=65537U) {
            double previous=-1.0;
            bool valid=true;
            for(const auto& bin:s.average_spectrum) {
                if(!std::isfinite(bin.hz)||bin.hz<=previous ||
                   bin.hz<0.0 ||
                   bin.hz>static_cast<double>(s.sample_rate_hz)*0.5 ||
                   bin.power_density.quality!=m21_private::Quality::present ||
                   !bin.power_density.unrounded ||
                   !std::isfinite(*bin.power_density.unrounded) ||
                   *bin.power_density.unrounded<0.0) {
                    valid=false;
                    break;
                }
                QVariantMap point;
                point.insert(QStringLiteral("hz"),bin.hz);
                point.insert(QStringLiteral("linearPower"),
                             *bin.power_density.unrounded);
                spectrum.append(point);
                previous=bin.hz;
            }
            if(!valid)spectrum.clear();
        }
    }
    released.insert(QStringLiteral("m19"),level);
    released.insert(QStringLiteral("m20"),stereo);
    released.insert(QStringLiteral("m20StaticPsd"),spectrum);
    released.insert(QStringLiteral("m21Rows"),rows);
    // Development observation only. Values do not certify Phase-6 Gate G.
    // Official qualified release remains separately gated above.
    return released;
}

M21ProductStatusViewModel::Bridge::Column
M21ProductStatusViewModel::column(const QString& role,bool& valid) noexcept {
    valid=true;
    if(role==QStringLiteral("PREPARED"))return Bridge::Column::prepared;
    if(role==QStringLiteral("REFERENCE"))return Bridge::Column::gold;
    if(role==QStringLiteral("PROCESSED"))return Bridge::Column::processed;
    valid=false;
    return Bridge::Column::prepared;
}
void M21ProductStatusViewModel::refresh() {
    if(!owner_thread())return;
    auto next=Bridge::capture(product_,*fanout_,*selector_,*gold_,
                              *inbox_,routing_);
    if(Bridge::same_visible_frame(frame_,next))
        return; // high-frequency Product notices must not repaint unchanged QML
    frame_=std::move(next);
    emit changed();
}
bool M21ProductStatusViewModel::selectPanelSource(
    int panel,const QString& publicRole) {
    if(!owner_thread()||panel<0||panel>2)return false;
    bool valid=false;
    const auto target=column(publicRole,valid);
    if(!valid)return false;
    constexpr std::array<Bridge::AnalysisPanel,3> panels{{
        Bridge::AnalysisPanel::levels_m19,
        Bridge::AnalysisPanel::tonal_m20,
        Bridge::AnalysisPanel::delta_m21}};
    if(!routing_.select(panels[static_cast<std::size_t>(panel)],target))
        return false;
    refresh();
    return true;
}
} // namespace rgsml::app
