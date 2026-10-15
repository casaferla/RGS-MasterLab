#pragma once

#include "true_peak_live_input_binding.hpp"

#include <QObject>
#include <QString>
#include <QTimer>

#include <array>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace rgsml::app {

// The first M16 LIVE path is strictly PROCESSED, never GOLD/PREPARED,
// and remains SUPPORTED_UNQUALIFIED while Gate G is open.
struct TruePeakLiveSource final {
    std::shared_ptr<const render::RenderResult> owner;
    core::RealizationId realizationId;
    TruePeakExpectedSource expected;
};

class TruePeakLiveCoordinator final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool monitoringAttempted READ monitoring_attempted NOTIFY changed)
    Q_PROPERTY(bool recentAvailable READ recent_available NOTIFY changed)
    Q_PROPERTY(bool holdAvailable READ hold_available NOTIFY changed)
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
    using PrefixProvider = std::function<
        core::Result<platform::windows::WindowsConsumedOutputPrefix>()>;
    using PlaybackProvider = std::function<core::Result<core::PlaybackSnapshot>()>;
    using SourceProvider = std::function<std::optional<TruePeakLiveSource>()>;

    explicit TruePeakLiveCoordinator(
        PrefixProvider prefix, PlaybackProvider playback,
        SourceProvider source, QObject* parent = nullptr);
    ~TruePeakLiveCoordinator() override;
    TruePeakLiveCoordinator(const TruePeakLiveCoordinator&) = delete;
    TruePeakLiveCoordinator& operator=(const TruePeakLiveCoordinator&) = delete;

    [[nodiscard]] bool monitoring_attempted() const noexcept { return monitoringAttempted_; }
    [[nodiscard]] bool recent_available() const noexcept { return recentAvailable_; }
    [[nodiscard]] bool hold_available() const noexcept { return holdAvailable_; }
    [[nodiscard]] QString peak_text() const;
    [[nodiscard]] QString hold_text() const;
    [[nodiscard]] QString status_text() const { return statusText_; }
    [[nodiscard]] QString left_text() const;
    [[nodiscard]] QString right_text() const;
    [[nodiscard]] double left_level() const;
    [[nodiscard]] double right_level() const;
    [[nodiscard]] bool left_over_zero() const noexcept {
        return recentAvailable_ && recent_[0] > 1.0;
    }
    [[nodiscard]] bool right_over_zero() const noexcept {
        return recentAvailable_ && channelCount_ == 2 && recent_[1] > 1.0;
    }
    [[nodiscard]] int channel_count() const noexcept { return channelCount_; }

signals:
    void changed();

private:
    struct Job final {
        std::uint64_t serial;
        std::uint64_t observationEpoch;
        BoundLiveTruePeakInput input;
    };
    struct Completion final {
        bool valid{false};
        bool recent{false};
        bool hold{false};
        std::array<double, 2> recentPeak{};
        std::array<double, 2> holdPeak{};
    };

    void poll();
    void run_worker();
    void accept_result(std::uint64_t serial, Completion completed);
    void clear_display(bool attempted);
    [[nodiscard]] static QString reading(double linear);
    [[nodiscard]] static double bar_level(double linear) noexcept;

    PrefixProvider prefix_;
    PlaybackProvider playback_;
    SourceProvider source_;
    QTimer timer_;
    bool monitoringAttempted_{false};
    bool recentAvailable_{false};
    bool holdAvailable_{false};
    int channelCount_{0};
    QString statusText_{QStringLiteral("NOT MONITORED")};
    std::array<double, 2> recent_{};
    std::array<double, 2> hold_{};
    std::uint64_t serial_{0};
    std::uint64_t observationEpoch_{0};
    std::optional<std::uint64_t> generation_;
    std::optional<std::uint64_t> seekSerial_;
    std::optional<std::uint64_t> traversalSerial_;
    std::optional<std::uint64_t> loopWrapCount_;
    std::optional<core::RealizationId> activeId_;
    std::shared_ptr<const render::RenderResult> currentOwner_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Job> pending_;
    bool stopping_{false};
    std::jthread worker_;
};

} // namespace rgsml::app
