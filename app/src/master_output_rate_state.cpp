#include "master_output_rate_state.hpp"
namespace rgsml::app {
void MasterOutputRateState::source_committed(qint64 hz) noexcept {
    sourceRateHz_=hz>0?hz:0;
    masterRateHz_=supported(sourceRateHz_)?sourceRateHz_:0;
    ++rateRevision_; // different SOURCE identity invalidates even at same Fs
    emit changed();
}
void MasterOutputRateState::source_cleared() noexcept {
    if(sourceRateHz_==0&&masterRateHz_==0)return;
    sourceRateHz_=0;masterRateHz_=0;++rateRevision_;emit changed();
}
bool MasterOutputRateState::choose_master_rate(qint64 hz) noexcept {
    if(sourceRateHz_<=0||!supported(hz))return false;
    if(hz==masterRateHz_)return true;
    masterRateHz_=hz;++rateRevision_;emit changed();return true;
}
} // namespace rgsml::app
