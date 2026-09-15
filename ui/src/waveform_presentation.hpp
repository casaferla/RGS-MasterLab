#pragma once

#include <rgsml/audio/waveform_summary.hpp>

#include <QObject>
#include <QString>

#include <memory>
#include <functional>
#include <optional>

namespace rgsml::ui {

namespace internal {
class WaveformViewport;
}

class WaveformPresentation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state_token NOTIFY changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool hasOverrange READ has_overrange NOTIFY changed)
    Q_PROPERTY(int channelCount READ channel_count NOTIFY changed)
    Q_PROPERTY(qint64 sourceFrameCount READ source_frame_count NOTIFY changed)
    Q_PROPERTY(qint64 baseFramesPerBucket READ base_frames_per_bucket NOTIFY changed)
    Q_PROPERTY(qint64 baseBucketCount READ base_bucket_count NOTIFY changed)
    Q_PROPERTY(qint64 levelCount READ level_count NOTIFY changed)
    Q_PROPERTY(qint64 payloadBytes READ payload_bytes NOTIFY changed)
    Q_PROPERTY(bool canNavigate READ can_navigate NOTIFY changed)
    Q_PROPERTY(bool fullFit READ full_fit NOTIFY changed)
    Q_PROPERTY(double zoomPosition READ zoom_position NOTIFY changed)
    Q_PROPERTY(QString viewportStartText READ viewport_start_text NOTIFY changed)
    Q_PROPERTY(QString viewportEndText READ viewport_end_text NOTIFY changed)
    Q_PROPERTY(QString viewportDurationText READ viewport_duration_text NOTIFY changed)
    Q_PROPERTY(qint64 visibleRangeCount READ visible_range_count NOTIFY changed)

public:
    using SeekHandler = std::function<core::Status(core::FrameIndex)>;
    using RegionCommitHandler = std::function<core::Status(core::FrameRange)>;

    enum class State {
        Empty,
        Building,
        Ready,
        Failed,
    };
    Q_ENUM(State)

    explicit WaveformPresentation(QObject* parent = nullptr);
    ~WaveformPresentation() override;

    [[nodiscard]] QString state_token() const;
    [[nodiscard]] QString status_text() const;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] bool has_overrange() const noexcept;
    [[nodiscard]] int channel_count() const noexcept;
    [[nodiscard]] qint64 source_frame_count() const noexcept;
    [[nodiscard]] qint64 base_frames_per_bucket() const noexcept;
    [[nodiscard]] qint64 base_bucket_count() const noexcept;
    [[nodiscard]] qint64 level_count() const noexcept;
    [[nodiscard]] qint64 payload_bytes() const noexcept;
    [[nodiscard]] bool can_navigate() const noexcept;
    [[nodiscard]] bool full_fit() const noexcept;
    [[nodiscard]] double zoom_position() const noexcept;
    [[nodiscard]] QString viewport_start_text() const;
    [[nodiscard]] QString viewport_end_text() const;
    [[nodiscard]] QString viewport_duration_text() const;
    [[nodiscard]] qint64 visible_range_count() const noexcept;
    [[nodiscard]] std::shared_ptr<const audio::WaveformSummary> summary() const noexcept;
    [[nodiscard]] core::FrameRange visible_range() const noexcept;
    [[nodiscard]] std::optional<core::FrameRange> displayed_region() const noexcept;
    [[nodiscard]] std::optional<core::FrameRange> candidate_region() const noexcept;
    [[nodiscard]] std::int64_t physical_frame_boundary(
        std::int64_t physicalBoundary,
        std::int64_t physicalWidth) const noexcept;
    [[nodiscard]] core::FrameIndex physical_seek_frame(
        std::int64_t physicalBoundary,
        std::int64_t physicalWidth) const noexcept;
    [[nodiscard]] std::int64_t physical_pixel_boundary(
        std::int64_t sourceFrameBoundary,
        std::int64_t physicalWidth) const noexcept;

    void publish_empty();
    void publish_building();
    void publish_ready(std::shared_ptr<const audio::WaveformSummary> summary);
    void publish_failed(QString message);
    void set_seek_handler(SeekHandler handler);
    void set_region_commit_handler(RegionCommitHandler handler);
    void set_displayed_region(std::optional<core::FrameRange> region);
    void set_playhead_frame(core::FrameIndex frame);
    void set_candidate_region(std::optional<core::FrameRange> region);
    void cancel_candidate();
    [[nodiscard]] bool seek_at(
        std::int64_t physicalBoundary,
        std::int64_t physicalWidth);
    [[nodiscard]] bool seek_frame(core::FrameIndex position);
    [[nodiscard]] bool commit_candidate(core::FrameRange candidate);
    [[nodiscard]] bool zoom_at(
        bool zoomIn,
        std::int64_t anchorPhysicalBoundary,
        std::int64_t physicalWidth);
    [[nodiscard]] bool zoom_keyboard(
        bool zoomIn,
        std::int64_t physicalWidth);
    [[nodiscard]] bool pan_from_snapshot(
        std::int64_t pressPhysicalBoundary,
        std::int64_t currentPhysicalBoundary,
        std::int64_t physicalWidth,
        core::FrameRange dragStartViewport);
    [[nodiscard]] bool pan_step(
        bool towardRight,
        bool fine,
        std::int64_t physicalWidth);

    Q_INVOKABLE void requestRetry();
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void setZoomPosition(double position);
    Q_INVOKABLE void fitSource();
    Q_INVOKABLE void fitRegion();

signals:
    void changed();
    void retryRequested();

private:
    [[nodiscard]] std::int64_t keyboard_anchor(std::int64_t physicalWidth) const noexcept;
    [[nodiscard]] QString format_frame_boundary(std::int64_t frame) const;

    State state_{State::Empty};
    QString failureMessage_;
    std::shared_ptr<const audio::WaveformSummary> summary_;
    std::unique_ptr<internal::WaveformViewport> viewport_;
    std::optional<core::FrameRange> displayedRegion_;
    std::optional<core::FrameRange> candidateRegion_;
    SeekHandler seekHandler_;
    RegionCommitHandler regionCommitHandler_;
    core::FrameIndex playheadFrame_{0};
    bool hasOverrange_{false};
};

}  // namespace rgsml::ui
