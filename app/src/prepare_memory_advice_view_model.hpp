#pragma once

#include "accepted_prepare_observation_policy.hpp"
#include <QObject>
#include <QString>
#include <QVariantList>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <utility>
#include <thread>
#include <mutex>
#include <optional>

namespace rgsml::platform::windows {
class WindowsAudioPlaybackService;
}
namespace rgsml::app {
class SourceSelectionViewModel;

// Read-only Prepare UI bridge. Each refresh uses a NEW device/memory snapshot.
// The rows are advice only: never a rate selection, allocation permit or CPU
// qualification. In particular, the current full-track monitor remains O(track).
class PrepareMemoryAdviceViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList recommendations READ recommendations NOTIFY changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
    Q_PROPERTY(bool calibrationRunning READ calibration_running NOTIFY calibrationChanged)
    Q_PROPERTY(int calibrationCompletedRates READ calibration_completed_rates NOTIFY calibrationChanged)
    Q_PROPERTY(bool calibrationValid READ calibration_valid NOTIFY calibrationChanged)
    Q_PROPERTY(double calibratedCeilingDbTp READ calibrated_ceiling_db_tp NOTIFY calibrationChanged)
    Q_PROPERTY(QVariantList dspEstimates READ dsp_estimates NOTIFY calibrationChanged)
    Q_PROPERTY(QString calibrationStatus READ calibration_status NOTIFY calibrationChanged)
    Q_PROPERTY(QVariantList actualPrepareTimes READ actual_prepare_times NOTIFY actualPrepareChanged)
    Q_PROPERTY(QVariantList hostTotalRanges READ host_total_ranges NOTIFY actualPrepareChanged)
public:
    PrepareMemoryAdviceViewModel(
        const SourceSelectionViewModel* source,
        const platform::windows::WindowsAudioPlaybackService* playback,
        QObject* parent = nullptr);
    ~PrepareMemoryAdviceViewModel() override;
    PrepareMemoryAdviceViewModel(const PrepareMemoryAdviceViewModel&)=delete;
    PrepareMemoryAdviceViewModel& operator=(const PrepareMemoryAdviceViewModel&)=delete;
    [[nodiscard]] QVariantList recommendations() const;
    [[nodiscard]] QString status_text() const;
    Q_INVOKABLE void refresh();
    // Runs genuine accelerated M17 + full-resource M16 on bounded internal
    // deterministic PCM in a NON-AUDIO worker. No Final SRC/DEVICE/WAV.
    Q_INVOKABLE bool calibrate(double ceilingDbTp);
    Q_INVOKABLE void cancel_calibration();
    // A GUI-thread authority veto: calibrated CPU probes never overlap a
    // live M17 terminal Prepare or an exclusive Master Audition session.
    // The QML enabled state is only presentation, not this safety gate.
    void set_calibration_activity_guard(std::function<bool()> guard) {
        calibrationActivityGuard_=std::move(guard);
    }
    [[nodiscard]] bool calibration_running() const noexcept {return calibrationRunning_;}
    [[nodiscard]] int calibration_completed_rates() const noexcept {return completedCalibrationRates_;}
    [[nodiscard]] bool calibration_valid() const noexcept {return calibrationValid_;}
    [[nodiscard]] double calibrated_ceiling_db_tp() const noexcept {return calibratedCeiling_;}
    [[nodiscard]] QVariantList dsp_estimates() const {return dspEstimates_;}
    [[nodiscard]] QString calibration_status() const {return calibrationStatus_;}
    [[nodiscard]] QVariantList actual_prepare_times() const;
    [[nodiscard]] QVariantList host_total_ranges() const;
    using CeilingProvider=std::function<double()>;
    void set_active_ceiling_provider(CeilingProvider provider) {
        activeCeilingProvider_=std::move(provider);
        emit actualPrepareChanged();
    }
    void observe_approved_prepare(const AcceptedPrepareObservation& observation);
    using RevisionProvider=std::function<
        std::optional<AcceptedPrepareRevisionKey>()>;
    void set_active_revision_provider(RevisionProvider provider) {
        activeRevisionProvider_=std::move(provider);
        emit actualPrepareChanged();
    }
    // Passive repaint on any committed terminal edit or completed/invalidated
    // MANUAL preview. Never starts a render or changes an audio owner.
    void current_revision_changed() {emit actualPrepareChanged();}
signals:
    void changed();
    void calibrationChanged();
    void actualPrepareChanged();
private:
    struct CalibrationJob {
        std::uint64_t serial;
        std::int64_t sourceRate;
        std::int64_t sourceFrames;
        int channels;
        double ceiling;
    };
    struct DspProbeEstimate {
        std::int64_t lowUs{0};
        std::int64_t highUs{0};
        bool valid{false};
    };
    void reset_unknown(const QString& status);
    void calibration_loop();
    void finish_calibration(std::uint64_t serial,double ceiling,
        std::array<DspProbeEstimate,6> results);
    void clear_calibration(const QString& message);
    const SourceSelectionViewModel* source_;
    const platform::windows::WindowsAudioPlaybackService* playback_;
    QVariantList recommendations_;
    QString statusText_;
    std::mutex calibrationMutex_;
    std::condition_variable calibrationWake_;
    std::optional<CalibrationJob> pendingCalibration_;
    std::atomic<std::uint64_t> activeCalibrationSerial_{0};
    std::uint64_t calibrationSerial_{0}; // GUI-thread only
    bool stoppingCalibration_{false};
    bool calibrationRunning_{false};
    int completedCalibrationRates_{0};
    bool calibrationValid_{false};
    double calibratedCeiling_{0.0};
    QVariantList dspEstimates_;
    AcceptedPrepareObservationHistory actualHistory_;
    AcceptedPrepareHostTimingHistory hostTimingHistory_;
    CeilingProvider activeCeilingProvider_;
    RevisionProvider activeRevisionProvider_;
    std::function<bool()> calibrationActivityGuard_;
    QString calibrationStatus_{QStringLiteral("No on-host calibration yet")};
    std::jthread calibrationThread_;
};

} // namespace rgsml::app
