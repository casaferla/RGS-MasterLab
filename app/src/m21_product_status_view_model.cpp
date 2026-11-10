#include "m21_product_status_view_model.hpp"
#include <QThread>
#include <array>
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
