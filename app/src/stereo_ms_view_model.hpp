#pragma once

#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"

#include <rgsml/dsp/stereo_ms_width.hpp>
#include <rgsml/dsp/stereo_ms_width_response.hpp>
#include <rgsml/render/audible_stereo_ms_telemetry_resolver.hpp>

#include <QTimer>

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <optional>
#include <functional>
#include <memory>

namespace rgsml::app {

// M15 editor authority: a typed draft is not the audible processing state.
// The single commit boundary owns validation, chain mutation and preview
// invalidation; graph drags/sliders must not trigger renders on every move.
class StereoMsViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(double midGainDb READ mid_gain_db NOTIFY changed)
    Q_PROPERTY(double sideGainDb READ side_gain_db NOTIFY changed)
    Q_PROPERTY(double widthPercent READ width_percent NOTIFY changed)
    Q_PROPERTY(bool sideMuted READ side_muted NOTIFY changed)
    Q_PROPERTY(QString monoBassMode READ mono_bass_mode NOTIFY changed)
    Q_PROPERTY(double monoBassCutoffHz READ mono_bass_cutoff_hz NOTIFY changed)
    Q_PROPERTY(double lowBandWidthPercent READ low_band_width_percent NOTIFY changed)
    Q_PROPERTY(double draftWidthPercent READ draft_width_percent NOTIFY changed)
    Q_PROPERTY(bool monoBassControlsEffective READ mono_bass_controls_effective NOTIFY changed)
    Q_PROPERTY(bool bypass READ bypass NOTIFY changed)
    Q_PROPERTY(QString validationField READ validation_field NOTIFY changed)
    Q_PROPERTY(QString validationMessage READ validation_message NOTIFY changed)
    Q_PROPERTY(quint64 previewGeneration READ preview_generation NOTIFY changed)
    Q_PROPERTY(QString previewStatus READ preview_status NOTIFY changed)
    Q_PROPERTY(QString previewError READ preview_error NOTIFY changed)
    Q_PROPERTY(QVariantList widthResponsePoints READ width_response_points NOTIFY changed)
    Q_PROPERTY(QString widthResponseStatus READ width_response_status NOTIFY changed)
    Q_PROPERTY(QString telemetryStatus READ telemetry_status NOTIFY telemetryChanged)
    Q_PROPERTY(bool telemetryActive READ telemetry_active NOTIFY telemetryChanged)
    Q_PROPERTY(bool telemetryGap READ telemetry_gap NOTIFY telemetryChanged)
    Q_PROPERTY(QString telemetryRealizationId READ telemetry_realization_id NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList telemetryDensityBuckets READ telemetry_density_buckets NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantMap telemetryCorrelation READ telemetry_correlation NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantMap telemetrySideLow READ telemetry_side_low NOTIFY telemetryChanged)

public:
    explicit StereoMsViewModel(
        MasteringChainState* chainState,
        MasteringPreviewController* previewController = nullptr,
        QObject* parent = nullptr);

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] double mid_gain_db() const noexcept;
    [[nodiscard]] double side_gain_db() const noexcept;
    [[nodiscard]] double width_percent() const noexcept;
    [[nodiscard]] bool side_muted() const noexcept;
    [[nodiscard]] QString mono_bass_mode() const;
    [[nodiscard]] double mono_bass_cutoff_hz() const noexcept;
    [[nodiscard]] double low_band_width_percent() const noexcept;
    [[nodiscard]] double draft_width_percent() const noexcept;
    [[nodiscard]] bool mono_bass_controls_effective() const noexcept;
    [[nodiscard]] bool bypass() const noexcept;
    [[nodiscard]] QString validation_field() const;
    [[nodiscard]] QString validation_message() const;
    [[nodiscard]] quint64 preview_generation() const noexcept;
    [[nodiscard]] QString preview_status() const;
    [[nodiscard]] QString preview_error() const;
    [[nodiscard]] QVariantList width_response_points() const;
    [[nodiscard]] QString width_response_status() const;

    struct AcceptedRenderEvidence final {
        std::shared_ptr<const render::RenderResult> result;
        std::optional<core::RealizationId> realization_id;
    };
    using AcceptedRenderProvider = std::function<AcceptedRenderEvidence()>;
    using PlaybackSnapshotProvider =
        std::function<core::Result<core::PlaybackSnapshot>()>;
    using AuditionProcessedProvider = std::function<bool()>;

    // Only accepted/published render identity plus audible PlaybackSnapshot
    // may generate observational M/S telemetry. A UI draft never can.
    void setTelemetryProviders(
        AcceptedRenderProvider acceptedRender,
        PlaybackSnapshotProvider playback,
        AuditionProcessedProvider auditionProcessed);
    [[nodiscard]] QString telemetry_status() const { return telemetryStatus_; }
    [[nodiscard]] bool telemetry_active() const noexcept { return telemetryActive_; }
    [[nodiscard]] bool telemetry_gap() const noexcept { return telemetryGap_; }
    [[nodiscard]] QString telemetry_realization_id() const { return telemetryRealizationId_; }
    [[nodiscard]] QVariantList telemetry_density_buckets() const { return telemetryDensityBuckets_; }
    [[nodiscard]] QVariantMap telemetry_correlation() const { return telemetryCorrelation_; }
    [[nodiscard]] QVariantMap telemetry_side_low() const { return telemetrySideLow_; }
    // Explicit poll seam for deterministic tests; the production timer polls
    // nominally at 30Hz. No future buckets are projected between polls.
    Q_INVOKABLE void refreshTelemetry();

    // All draft edits are validated by the canonical DSP constructors/helper;
    // invalid attempts cannot corrupt either the draft or chain state.
    // Width has NO persistent field and preserves common gain by contract.
    // Display source-format seam. Must be supplied from the actual prepared
    // realization, never guessed from its filename or a nominal global rate.
    // An absent/mono format produces no fabricated stereo response.
    Q_INVOKABLE void setSignalFormat(double effectiveSampleRateHz, int channelCount);
    // Bridge used by canonical StudioParameterSlider/StudioNumericField.
    // The typed setters remain the sole DSP validation/commit authority.
    Q_INVOKABLE bool setDraftFieldValue(const QString& field, double value);
    Q_INVOKABLE bool setDraftFieldText(const QString& field, const QString& text);
    Q_INVOKABLE bool setDraftWidthPercent(double value);
    Q_INVOKABLE bool setDraftMidGainDb(double value);
    Q_INVOKABLE bool setDraftSideGainDb(double value);
    Q_INVOKABLE bool setDraftSideMuted(bool value);
    Q_INVOKABLE bool setDraftMonoBassMode(const QString& mode);
    Q_INVOKABLE bool setDraftMonoBassCutoffHz(double value);
    Q_INVOKABLE bool setDraftLowBandWidthPercent(double value);
    Q_INVOKABLE bool commitDraft();
    Q_INVOKABLE void cancelDraft();
    Q_INVOKABLE void resetToDefault();
    Q_INVOKABLE void setBypass(bool bypass);
    Q_INVOKABLE void refreshFromAuthority();
    Q_INVOKABLE void resetForNewSource();

signals:
    void changed();
    void telemetryChanged();

private:
    [[nodiscard]] const dsp::StereoMsParameters* committed() const noexcept;
    [[nodiscard]] const dsp::StereoMsParameters* editing() const noexcept;
    [[nodiscard]] bool stage(
        double mid, double side, bool mute, dsp::MonoBassMode mode,
        double cutoff, double lowWidth, const QString& field);
    void set_error(const QString& field, const QString& message);
    void clear_error();
    void request_preview();

    MasteringChainState* chainState_{nullptr};
    MasteringPreviewController* previewController_{nullptr};
    std::optional<dsp::StereoMsParameters> draft_;
    QString validationField_;
    QString validationMessage_;
    double effectiveSampleRateHz_{0.0};
    int sourceChannelCount_{0};
    AcceptedRenderProvider acceptedRenderProvider_;
    PlaybackSnapshotProvider playbackSnapshotProvider_;
    AuditionProcessedProvider auditionProcessedProvider_;
    render::AudibleStereoMsTelemetryResolver telemetryResolver_;
    QTimer telemetryTimer_;
    QString telemetryStatus_{QStringLiteral("UNAVAILABLE")};
    QString telemetryRealizationId_;
    bool telemetryActive_{false};
    bool telemetryGap_{false};
    QVariantList telemetryDensityBuckets_;
    QVariantMap telemetryCorrelation_;
    QVariantMap telemetrySideLow_;
};

}  // namespace rgsml::app
