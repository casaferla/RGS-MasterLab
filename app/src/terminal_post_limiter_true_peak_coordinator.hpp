#pragma once
#include "terminal_consumed_output_binding.hpp"
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <QObject>
#include <QTimer>
#include <QString>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <thread>

namespace rgsml::app {

// M16 FIR on actual consumed M17 OUTPUT_RATE PCM, not manual SOURCE PCM.
// No device, provenance, identity, or uninterrupted chronology => no reading.
// Independently acknowledged sink consumption is REQUIRED for every job;
// does not claim FQ, final WAV certification or a DAC hardware TP guarantee.
class TerminalPostLimiterTruePeakCoordinator final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool monitoringAttempted READ monitoring_attempted NOTIFY changed)
    Q_PROPERTY(bool recentAvailable READ recent_available NOTIFY changed)
    Q_PROPERTY(bool holdAvailable READ hold_available NOTIFY changed)
    Q_PROPERTY(double leftHoldLevel READ left_hold_level NOTIFY changed)
    Q_PROPERTY(double rightHoldLevel READ right_hold_level NOTIFY changed)
    Q_PROPERTY(QString peakText READ peak_text NOTIFY changed)
    Q_PROPERTY(QString holdText READ hold_text NOTIFY changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
    Q_PROPERTY(QString leftText READ left_text NOTIFY changed)
    Q_PROPERTY(QString rightText READ right_text NOTIFY changed)
    Q_PROPERTY(double leftLevel READ left_level NOTIFY changed)
    Q_PROPERTY(double rightLevel READ right_level NOTIFY changed)
    Q_PROPERTY(bool leftOverZero READ left_over_zero NOTIFY changed)
    Q_PROPERTY(bool rightOverZero READ right_over_zero NOTIFY changed)
    Q_PROPERTY(int channelCount READ channel_count NOTIFY changed)
public:
    using Observer=std::function<core::Result<TerminalConsumedOutputEvidence>()>;
    explicit TerminalPostLimiterTruePeakCoordinator(
        Observer observe,QObject* parent=nullptr);
    ~TerminalPostLimiterTruePeakCoordinator() override;
    TerminalPostLimiterTruePeakCoordinator(const TerminalPostLimiterTruePeakCoordinator&)=delete;
    TerminalPostLimiterTruePeakCoordinator& operator=(const TerminalPostLimiterTruePeakCoordinator&)=delete;

    [[nodiscard]] bool monitoring_attempted() const noexcept{return attempted_;}
    [[nodiscard]] bool recent_available() const noexcept{return recent_;}
    [[nodiscard]] bool hold_available() const noexcept{return hold_;}
    [[nodiscard]] double left_hold_level() const noexcept;
    [[nodiscard]] double right_hold_level() const noexcept;
    [[nodiscard]] QString peak_text() const;
    [[nodiscard]] QString hold_text() const;
    [[nodiscard]] QString status_text() const{return status_;}
    [[nodiscard]] QString left_text() const;
    [[nodiscard]] QString right_text() const;
    [[nodiscard]] double left_level() const noexcept;
    [[nodiscard]] double right_level() const noexcept;
    [[nodiscard]] bool left_over_zero() const noexcept;
    [[nodiscard]] bool right_over_zero() const noexcept;
    [[nodiscard]] int channel_count() const noexcept{return channels_;}

public slots:
    void invalidate();
signals:
    void changed();
private:
    struct Job final {
        std::uint64_t serial,epoch;
        TerminalConsumedOutputEvidence proof;
    };
    struct Completion final {
        bool valid{false},recent{false},hold{false};
        std::array<double,2> recentPeaks{},holdPeaks{};
    };
    void poll();
    void worker_loop();
    void accept(std::uint64_t,Completion);
    void clear(bool attempted, QString diagnostic={});
    static QString reading(double value);
    static double normalized(double value) noexcept;
    Observer observe_;
    QTimer timer_;
    bool attempted_{false},recent_{false},hold_{false};
    int channels_{0};
    QString status_{QStringLiteral("NOT MONITORED")};
    std::array<double,2> recentPeaks_{},holdPeaks_{};
    std::uint64_t serial_{0},epoch_{0};
    std::optional<core::RealizationId> id_;
    std::optional<std::uint64_t> generation_,seek_,traversal_,wrap_;
    // GUI-only FIR scheduling cache, scoped to the exact checked epoch.
    // clear() invalidates it on GAP, seek, wrap or device owner retirement.
    std::optional<std::int64_t> scheduledOutputWatermark_;
    std::optional<std::int64_t> scheduledFirstTerminalFrame_;
    std::optional<std::int64_t> scheduledLastTerminalFrame_;
    std::shared_ptr<const TerminalCandidateGate::StagedCandidate> owner_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Job> pending_;
    bool stopping_{false};
    std::jthread worker_;
};
} // namespace rgsml::app
