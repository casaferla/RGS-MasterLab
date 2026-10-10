#pragma once

#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/realization_identity.hpp>
#include <rgsml/core/resource_reference.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/render/render_result.hpp>

#include <QObject>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace rgsml::app {

class PlaybackTransportViewModel;

enum class AuditionTarget {
    PREPARED,
    PROCESSED,
    GOLD,
};

class AuditionSourceSelector final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool preparedAvailable READ prepared_available NOTIFY changed)
    Q_PROPERTY(bool processedAvailable READ processed_available NOTIFY changed)
    Q_PROPERTY(bool goldAvailable READ gold_available NOTIFY changed)
    Q_PROPERTY(QString activeTarget READ active_target_label NOTIFY changed)
    Q_PROPERTY(bool goldActive READ gold_active NOTIFY changed)
    Q_PROPERTY(bool sourcePlayheadVisible READ source_playhead_visible NOTIFY changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)

public:
    using SourceLoopProvider = std::function<std::optional<core::FrameRange>()>;

    explicit AuditionSourceSelector(
        PlaybackTransportViewModel* playback,
        QObject* parent = nullptr);
    ~AuditionSourceSelector() noexcept override;

    [[nodiscard]] bool prepared_available() const noexcept;
    [[nodiscard]] bool processed_available() const noexcept;
    [[nodiscard]] bool gold_available() const noexcept;
    [[nodiscard]] QString active_target_label() const;
    [[nodiscard]] bool gold_active() const noexcept;
    [[nodiscard]] bool source_playhead_visible() const noexcept;
    [[nodiscard]] QString status_text() const;
    [[nodiscard]] std::optional<AuditionTarget> active_target() const noexcept;
    [[nodiscard]] core::FrameIndex source_derived_cue() const noexcept;
    [[nodiscard]] core::FrameIndex gold_cue() const noexcept;
    [[nodiscard]] std::shared_ptr<const render::RenderResult> prepared_realization_snapshot() const noexcept;
    [[nodiscard]] std::shared_ptr<const render::RenderResult> processed_realization_snapshot() const noexcept;
    // Identity of the last ACCEPTED, published Processed RenderResult.
    // Not necessarily audible; PlaybackSnapshot is the independent authority.
    [[nodiscard]] std::optional<core::RealizationId>
    processed_realization_id() const noexcept;

    void set_source_loop_provider(SourceLoopProvider provider);
    [[nodiscard]] core::Status source_committed(
        const core::ResourceReference& source);
    [[nodiscard]] core::Status set_prepared_realization(
        render::RenderResult realization);
    [[nodiscard]] core::Status set_processed_realization(
        render::RenderResult realization);
    [[nodiscard]] core::Status set_gold(
        core::ResourceReference reference,
        core::SampleRate sampleRate,
        core::FrameCount frameCount);
    [[nodiscard]] core::Status clear_gold();
    [[nodiscard]] core::Status switch_to(AuditionTarget target);

    Q_INVOKABLE void selectPrepared();
    Q_INVOKABLE void selectProcessed();
    Q_INVOKABLE void selectGold();

signals:
    void changed();

private:
    [[nodiscard]] core::Status store_active_cue();
    [[nodiscard]] core::Status prepare_realization(
        const render::RenderResult& realization,
        std::shared_ptr<const void> lifetime = nullptr,
        std::optional<core::RealizationId> realizationId = std::nullopt);
    [[nodiscard]] core::Status materialize_prepared(
        const core::ResourceReference& source);
    void fail_closed(const core::Error& error);
    void publish_error(QString message);

    PlaybackTransportViewModel* playback_;
    SourceLoopProvider sourceLoopProvider_;
    std::optional<core::ResourceReference> source_;
    std::shared_ptr<const render::RenderResult> prepared_;
    std::shared_ptr<const render::RenderResult> processed_;
    std::optional<core::RealizationId> processedRealizationId_;
    std::uint64_t nextProcessedRealizationIdValue_{1};
    std::optional<core::ResourceReference> gold_;
    std::optional<core::SampleRate> goldRate_;
    std::optional<core::FrameCount> goldFrames_;
    std::optional<AuditionTarget> activeTarget_;
    core::FrameIndex sourceDerivedCue_{0};
    core::FrameIndex goldCue_{0};
    QString statusText_;
};

}  // namespace rgsml::app
