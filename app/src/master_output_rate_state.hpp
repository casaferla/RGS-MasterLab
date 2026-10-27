#pragma once
#include <QObject>
#include <QtGlobal>
#include <array>
namespace rgsml::app {
// Phase A Project/Render preference only: NOT the DEVICE rate, and NOT YET
// the published/rendered terminal PCM rate. Never certify SRC from selection.
class MasterOutputRateState final : public QObject {
    Q_OBJECT
    Q_PROPERTY(qint64 sourceRateHz READ source_rate_hz NOTIFY changed)
    Q_PROPERTY(qint64 masterRateHz READ master_rate_hz NOTIFY changed)
    Q_PROPERTY(bool hasSupportedMasterRate READ has_supported_master_rate NOTIFY changed)
    Q_PROPERTY(quint64 rateRevision READ rate_revision NOTIFY changed)
public:
    explicit MasterOutputRateState(QObject* parent=nullptr):QObject(parent){}
    [[nodiscard]] static constexpr bool supported(qint64 hz) noexcept {
        constexpr std::array<qint64,6> rates{44100,48000,88200,96000,176400,192000};
        for(const auto candidate:rates)if(candidate==hz)return true;
        return false;
    }
    [[nodiscard]] qint64 source_rate_hz()const noexcept{return sourceRateHz_;}
    [[nodiscard]] qint64 master_rate_hz()const noexcept{return masterRateHz_;}
    [[nodiscard]] bool has_supported_master_rate()const noexcept {
        return sourceRateHz_>0&&supported(masterRateHz_);
    }
    [[nodiscard]] quint64 rate_revision()const noexcept{return rateRevision_;}
    void source_committed(qint64 sourceRateHz) noexcept;
    void source_cleared() noexcept;
    Q_INVOKABLE bool choose_master_rate(qint64 targetHz) noexcept;
signals:
    void changed();
private:
    qint64 sourceRateHz_{0};
    qint64 masterRateHz_{0};
    quint64 rateRevision_{0};
};
} // namespace rgsml::app
