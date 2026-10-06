#pragma once

#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"

#include <rgsml/dsp/compressor_parameters.hpp>

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>
#include <vector>

namespace rgsml::app {

class CompressorViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString detectorMode READ detector_mode NOTIFY changed)
    Q_PROPERTY(QString channelLink READ channel_link NOTIFY changed)
    Q_PROPERTY(double thresholdDbfs READ threshold_dbfs NOTIFY changed)
    Q_PROPERTY(double ratio READ ratio NOTIFY changed)
    Q_PROPERTY(double kneeDb READ knee_db NOTIFY changed)
    Q_PROPERTY(double attackMs READ attack_ms NOTIFY changed)
    Q_PROPERTY(double releaseMs READ release_ms NOTIFY changed)
    Q_PROPERTY(double rmsTimeConstantMs READ rms_time_constant_ms NOTIFY changed)
    Q_PROPERTY(double lookAheadMs READ look_ahead_ms NOTIFY changed)
    Q_PROPERTY(double mixPercent READ mix_percent NOTIFY changed)
    Q_PROPERTY(double makeupGainDb READ makeup_gain_db NOTIFY changed)

    Q_PROPERTY(QString thresholdText READ threshold_text NOTIFY changed)
    Q_PROPERTY(QString ratioText READ ratio_text NOTIFY changed)
    Q_PROPERTY(QString kneeText READ knee_text NOTIFY changed)
    Q_PROPERTY(QString attackText READ attack_text NOTIFY changed)
    Q_PROPERTY(QString releaseText READ release_text NOTIFY changed)
    Q_PROPERTY(QString rmsTimeConstantText READ rms_time_constant_text NOTIFY changed)
    Q_PROPERTY(QString lookAheadText READ look_ahead_text NOTIFY changed)
    Q_PROPERTY(QString mixPercentText READ mix_percent_text NOTIFY changed)
    Q_PROPERTY(QString makeupGainText READ makeup_gain_text NOTIFY changed)
    Q_PROPERTY(bool bypass READ bypass WRITE setBypass NOTIFY changed)
    Q_PROPERTY(bool canUndo READ can_undo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ can_redo NOTIFY changed)
    Q_PROPERTY(QString validationField READ validation_field NOTIFY changed)
    Q_PROPERTY(QString validationMessage READ validation_message NOTIFY changed)
    Q_PROPERTY(quint64 previewGeneration READ preview_generation NOTIFY changed)
    Q_PROPERTY(QString previewStatus READ preview_status NOTIFY changed)
    Q_PROPERTY(QString previewError READ preview_error NOTIFY changed)
    Q_PROPERTY(bool rmsTimeEffective READ rms_time_effective NOTIFY changed)
    Q_PROPERTY(bool channelLinkEffective READ channel_link_effective NOTIFY changed)
    Q_PROPERTY(QVariantList transferCurvePoints READ transfer_curve_points NOTIFY changed)
    Q_PROPERTY(QVariantList transferCurveHandles READ transfer_curve_handles NOTIFY changed)
    Q_PROPERTY(double plotXMinDbfs READ plot_x_min_dbfs NOTIFY changed)
    Q_PROPERTY(double plotYMinDbfs READ plot_y_min_dbfs NOTIFY changed)
    Q_PROPERTY(double plotYMaxDbfs READ plot_y_max_dbfs NOTIFY changed)

    Q_PROPERTY(double draftThresholdDbfs READ draft_threshold_dbfs NOTIFY changed)
    Q_PROPERTY(double draftRatio READ draft_ratio NOTIFY changed)
    Q_PROPERTY(double draftKneeDb READ draft_knee_db NOTIFY changed)
    Q_PROPERTY(double draftAttackMs READ draft_attack_ms NOTIFY changed)
    Q_PROPERTY(double draftReleaseMs READ draft_release_ms NOTIFY changed)
    Q_PROPERTY(double draftRmsTimeConstantMs READ draft_rms_time_constant_ms NOTIFY changed)
    Q_PROPERTY(double draftLookAheadMs READ draft_look_ahead_ms NOTIFY changed)
    Q_PROPERTY(double draftMixPercent READ draft_mix_percent NOTIFY changed)
    Q_PROPERTY(double draftMakeupGainDb READ draft_makeup_gain_db NOTIFY changed)

public:
    struct CompressorStateSnapshot final {
        dsp::CompressorParameters parameters;
        bool bypass{false};

        bool operator==(const CompressorStateSnapshot& other) const noexcept {
            return parameters == other.parameters && bypass == other.bypass;
        }
    };

    explicit CompressorViewModel(
        MasteringChainState* chainState = nullptr,
        MasteringPreviewController* previewController = nullptr,
        QObject* parent = nullptr);
    ~CompressorViewModel() override = default;

    CompressorViewModel(const CompressorViewModel&) = delete;
    CompressorViewModel& operator=(const CompressorViewModel&) = delete;

    [[nodiscard]] QString detector_mode() const;
    [[nodiscard]] QString channel_link() const;
    [[nodiscard]] double threshold_dbfs() const noexcept;
    [[nodiscard]] double ratio() const noexcept;
    [[nodiscard]] double knee_db() const noexcept;
    [[nodiscard]] double attack_ms() const noexcept;
    [[nodiscard]] double release_ms() const noexcept;
    [[nodiscard]] double rms_time_constant_ms() const noexcept;
    [[nodiscard]] double look_ahead_ms() const noexcept;
    [[nodiscard]] double mix_percent() const noexcept;
    [[nodiscard]] double makeup_gain_db() const noexcept;

    [[nodiscard]] QString threshold_text() const;
    [[nodiscard]] QString ratio_text() const;
    [[nodiscard]] QString knee_text() const;
    [[nodiscard]] QString attack_text() const;
    [[nodiscard]] QString release_text() const;
    [[nodiscard]] QString rms_time_constant_text() const;
    [[nodiscard]] QString look_ahead_text() const;
    [[nodiscard]] QString mix_percent_text() const;
    [[nodiscard]] QString makeup_gain_text() const;

    [[nodiscard]] bool bypass() const noexcept;
    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;
    [[nodiscard]] QString validation_field() const;
    [[nodiscard]] QString validation_message() const;
    [[nodiscard]] quint64 preview_generation() const noexcept;
    [[nodiscard]] QString preview_status() const;
    [[nodiscard]] QString preview_error() const;
    [[nodiscard]] bool rms_time_effective() const noexcept;
    [[nodiscard]] bool channel_link_effective() const noexcept;
    [[nodiscard]] QVariantList transfer_curve_points() const;
    [[nodiscard]] QVariantList transfer_curve_handles() const;
    [[nodiscard]] double plot_x_min_dbfs() const;
    [[nodiscard]] double plot_y_min_dbfs() const;
    [[nodiscard]] double plot_y_max_dbfs() const;

    [[nodiscard]] double draft_threshold_dbfs() const noexcept { return draftThresholdDbfs_; }
    [[nodiscard]] double draft_ratio() const noexcept { return draftRatio_; }
    [[nodiscard]] double draft_knee_db() const noexcept { return draftKneeDb_; }
    [[nodiscard]] double draft_attack_ms() const noexcept { return draftAttackMs_; }
    [[nodiscard]] double draft_release_ms() const noexcept { return draftReleaseMs_; }
    [[nodiscard]] double draft_rms_time_constant_ms() const noexcept { return draftRmsTimeConstantMs_; }
    [[nodiscard]] double draft_look_ahead_ms() const noexcept { return draftLookAheadMs_; }
    [[nodiscard]] double draft_mix_percent() const noexcept { return draftMixPercent_; }
    [[nodiscard]] double draft_makeup_gain_db() const noexcept { return draftMakeupGainDb_; }

    Q_INVOKABLE void setDetectorMode(const QString& mode);
    Q_INVOKABLE void setChannelLink(const QString& link);
    Q_INVOKABLE void setThresholdDbfs(double val);
    Q_INVOKABLE void setRatio(double val);
    Q_INVOKABLE void setKneeDb(double val);
    Q_INVOKABLE void setAttackMs(double val);
    Q_INVOKABLE void setReleaseMs(double val);
    Q_INVOKABLE void setRmsTimeConstantMs(double val);
    Q_INVOKABLE void setLookAheadMs(double val);
    Q_INVOKABLE void setMixPercent(double val);
    Q_INVOKABLE void setMakeupGainDb(double val);

    Q_INVOKABLE void setDraftFieldText(const QString& fieldName, const QString& text);
    Q_INVOKABLE void setDraftFieldValue(const QString& fieldName, double value);
    Q_INVOKABLE void setCurveHandleDraft(const QString& handleId, double inputDbfs, double outputDbfs);
    Q_INVOKABLE bool commitDraft();
    Q_INVOKABLE void cancelDraft();

    Q_INVOKABLE void setBypass(bool bypass);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void resetToDefault();
    Q_INVOKABLE void resetForNewSource();
    Q_INVOKABLE void refreshFromAuthority();

signals:
    void changed();

private:
    [[nodiscard]] MasteringChainState& active_chain_state() const noexcept;
    [[nodiscard]] MasteringPreviewController& active_preview_controller() const noexcept;
    [[nodiscard]] bool is_mono_prepared() const noexcept;

    void request_preview();
    void push_undo_snapshot(CompressorStateSnapshot previousSnapshot);
    [[nodiscard]] CompressorStateSnapshot capture_current_snapshot() const;
    void restore_snapshot(const CompressorStateSnapshot& snapshot);

    void commit_candidate_or_set_validation(
        dsp::CompressorDetectorMode detectorMode,
        dsp::CompressorChannelLink channelLink,
        double thresholdDbfs,
        double ratio,
        double kneeDb,
        double attackMs,
        double releaseMs,
        double rmsTimeConstantMs,
        double lookAheadMs,
        double mixPercent,
        double makeupGainDb,
        const QString& fieldName);

    MasteringChainState* externalChainState_{nullptr};
    std::unique_ptr<MasteringChainState> ownedChainState_;
    MasteringPreviewController* externalPreviewController_{nullptr};
    std::unique_ptr<MasteringPreviewController> ownedPreviewController_;

    QString validationField_;
    QString validationMessage_;

    // Draft parameters when editing
    dsp::CompressorDetectorMode draftDetectorMode_{dsp::CompressorDetectorMode::RMS};
    dsp::CompressorChannelLink draftChannelLink_{dsp::CompressorChannelLink::LINKED_MAX};
    double draftThresholdDbfs_{-24.0};
    double draftRatio_{2.0};
    double draftKneeDb_{6.0};
    double draftAttackMs_{30.0};
    double draftReleaseMs_{200.0};
    double draftRmsTimeConstantMs_{50.0};
    double draftLookAheadMs_{5.0};
    double draftMixPercent_{100.0};
    double draftMakeupGainDb_{0.0};

    QString draftThresholdText_{QStringLiteral("-24.0")};
    QString draftRatioText_{QStringLiteral("2.00")};
    QString draftKneeText_{QStringLiteral("6.0")};
    QString draftAttackText_{QStringLiteral("30.0")};
    QString draftReleaseText_{QStringLiteral("200.0")};
    QString draftRmsTimeConstantText_{QStringLiteral("50.0")};
    QString draftLookAheadText_{QStringLiteral("5.0")};
    QString draftMixPercentText_{QStringLiteral("100.0")};
    QString draftMakeupGainText_{QStringLiteral("0.0")};

    std::vector<CompressorStateSnapshot> undoStack_;
    std::vector<CompressorStateSnapshot> redoStack_;

    void update_cached_curve() const;

    mutable bool curveCacheValid_{false};
    mutable QVariantList cachedTransferCurvePoints_;
    mutable QVariantList cachedTransferCurveHandles_;
    mutable double cachedPlotXMinDbfs_{-60.0};
    mutable double cachedPlotYMinDbfs_{-60.0};
    mutable double cachedPlotYMaxDbfs_{6.0};
};

}  // namespace rgsml::app
