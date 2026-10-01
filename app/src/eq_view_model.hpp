#pragma once

#include "mastering_chain_state.hpp"
#include "mastering_preview_controller.hpp"

#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/render/render_result.hpp>

#include <QObject>
#include <QString>
#include <QVariant>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace rgsml::app {

class EqViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int bandCount READ band_count NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selected_index NOTIFY changed)
    Q_PROPERTY(QString selectedBandId READ selected_band_id NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(QString filter READ filter_label NOTIFY changed)
    Q_PROPERTY(QString routing READ routing_label NOTIFY changed)
    Q_PROPERTY(double frequency READ frequency NOTIFY changed)
    Q_PROPERTY(double gain READ gain NOTIFY changed)
    Q_PROPERTY(double q READ q NOTIFY changed)
    Q_PROPERTY(double shelfSlope READ shelf_slope NOTIFY changed)
    Q_PROPERTY(QString frequencyText READ frequency_text NOTIFY changed)
    Q_PROPERTY(QString gainText READ gain_text NOTIFY changed)
    Q_PROPERTY(QString qText READ q_text NOTIFY changed)
    Q_PROPERTY(QString shelfSlopeText READ shelf_slope_text NOTIFY changed)
    Q_PROPERTY(int slopeDbPerOct READ slope_db_per_oct NOTIFY changed)
    Q_PROPERTY(bool gainApplicable READ gain_applicable NOTIFY changed)
    Q_PROPERTY(bool qApplicable READ q_applicable NOTIFY changed)
    Q_PROPERTY(bool shelfSlopeApplicable READ shelf_slope_applicable NOTIFY changed)
    Q_PROPERTY(bool slopeApplicable READ slope_applicable NOTIFY changed)
    Q_PROPERTY(bool addAvailable READ add_available NOTIFY changed)
    Q_PROPERTY(bool removeAvailable READ remove_available NOTIFY changed)
    Q_PROPERTY(bool routeAvailable READ route_available NOTIFY changed)
    Q_PROPERTY(bool mixedRouting READ mixed_routing NOTIFY changed)
    Q_PROPERTY(bool isDefault READ is_default NOTIFY changed)
    Q_PROPERTY(bool bypass READ bypass NOTIFY changed)
    Q_PROPERTY(bool canUndo READ can_undo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ can_redo NOTIFY changed)
    Q_PROPERTY(QVariantList bandSummaries READ band_summaries NOTIFY changed)
    Q_PROPERTY(QString validationField READ validation_field NOTIFY changed)
    Q_PROPERTY(QString validationMessage READ validation_message NOTIFY changed)
    Q_PROPERTY(quint64 previewGeneration READ preview_generation NOTIFY changed)
    Q_PROPERTY(QString previewStatus READ preview_status NOTIFY changed)
    Q_PROPERTY(QString previewError READ preview_error NOTIFY changed)
    Q_PROPERTY(QVariantList selectedBandResponsePoints READ selected_band_response_points NOTIFY changed)
    Q_PROPERTY(QVariantList combinedResponsePoints READ combined_response_points NOTIFY changed)
    Q_PROPERTY(bool showCombinedResponse READ show_combined_response WRITE setShowCombinedResponse NOTIFY changed)

public:
    using PreparedSnapshotProvider = MasteringPreviewController::PreparedSnapshotProvider;
    using ProcessedRealizationPublisher = MasteringPreviewController::ProcessedRealizationPublisher;
    using IdGenerator = std::function<core::Uuid()>;

    using PreviewJob = MasteringPreviewController::PreviewJob;
    using PreviewExecutor = MasteringPreviewController::PreviewExecutor;

    struct EqStateSnapshot final {
        std::vector<dsp::EqBandParameters> bands;
        std::size_t selectedIndex{0};
        bool bypass{false};

        bool operator==(const EqStateSnapshot& other) const noexcept {
            return selectedIndex == other.selectedIndex
                && bypass == other.bypass
                && bands == other.bands;
        }
    };

    struct DraftBand final {
        core::Uuid band_id;
        bool enabled{true};
        dsp::EqFilterType filter_type{dsp::EqFilterType::BELL};
        dsp::EqRouting routing{dsp::EqRouting::STEREO};
        double frequency_hz{1000.0};
        double gain_db{0.0};
        double q{0.707};
        double shelf_slope{1.0};
        QString frequency_text{QStringLiteral("1000")};
        QString gain_text{QStringLiteral("0")};
        QString q_text{QStringLiteral("0.707")};
        QString shelf_slope_text{QStringLiteral("1")};
        dsp::SlopeDbPerOctave slope_db_per_octave{dsp::SlopeDbPerOctave::DB_12};
    };

    explicit EqViewModel(
        MasteringChainState* chainState = nullptr,
        MasteringPreviewController* previewController = nullptr,
        IdGenerator idGenerator = nullptr,
        QObject* parent = nullptr);

    // Overload for backwards-compatibility with tests supplying (snapshotProvider, publisher, idGenerator)
    explicit EqViewModel(
        PreparedSnapshotProvider snapshotProvider,
        ProcessedRealizationPublisher publisher = nullptr,
        IdGenerator idGenerator = nullptr,
        QObject* parent = nullptr);

    ~EqViewModel() override = default;

    EqViewModel(const EqViewModel&) = delete;
    EqViewModel& operator=(const EqViewModel&) = delete;

    void set_preview_executor(PreviewExecutor executor);

    // Property getters
    [[nodiscard]] int band_count() const noexcept;
    [[nodiscard]] int selected_index() const noexcept;
    [[nodiscard]] QString selected_band_id() const;
    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] QString filter_label() const;
    [[nodiscard]] QString routing_label() const;
    [[nodiscard]] double frequency() const noexcept;
    [[nodiscard]] double gain() const noexcept;
    [[nodiscard]] double q() const noexcept;
    [[nodiscard]] double shelf_slope() const noexcept;
    [[nodiscard]] QString frequency_text() const;
    [[nodiscard]] QString gain_text() const;
    [[nodiscard]] QString q_text() const;
    [[nodiscard]] QString shelf_slope_text() const;
    [[nodiscard]] int slope_db_per_oct() const noexcept;

    [[nodiscard]] bool gain_applicable() const noexcept;
    [[nodiscard]] bool q_applicable() const noexcept;
    [[nodiscard]] bool shelf_slope_applicable() const noexcept;
    [[nodiscard]] bool slope_applicable() const noexcept;

    [[nodiscard]] bool add_available() const noexcept;
    [[nodiscard]] bool remove_available() const noexcept;
    [[nodiscard]] bool route_available() const noexcept;
    [[nodiscard]] bool mixed_routing() const noexcept;
    [[nodiscard]] bool is_default() const noexcept;
    [[nodiscard]] bool bypass() const noexcept;
    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;

    [[nodiscard]] QVariantList band_summaries() const;
    [[nodiscard]] QString validation_field() const;
    [[nodiscard]] QString validation_message() const;

    [[nodiscard]] quint64 preview_generation() const noexcept;
    [[nodiscard]] QString preview_status() const;
    [[nodiscard]] QString preview_error() const;
    [[nodiscard]] QVariantList selected_band_response_points() const;
    [[nodiscard]] QVariantList combined_response_points() const;
    [[nodiscard]] bool show_combined_response() const noexcept;
    void setShowCombinedResponse(bool show);

    [[nodiscard]] dsp::ModuleInstanceId instance_id() const noexcept;
    [[nodiscard]] const dsp::ParametricEqParameters& committed_parameters() const noexcept;
    [[nodiscard]] std::uint64_t stale_results_discarded() const noexcept;

    // UI Invokables
    Q_INVOKABLE void selectBand(int index);
    Q_INVOKABLE void addBand();
    Q_INVOKABLE void removeSelectedBand();
    Q_INVOKABLE void setEnabled(bool enabled);
    Q_INVOKABLE void setFilter(const QString& filter);
    Q_INVOKABLE void setRouting(const QString& routing);
    Q_INVOKABLE void setBypass(bool bypass);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void resetToFlat();
    Q_INVOKABLE void resetForNewSource();

    Q_INVOKABLE void setDraftFrequency(double frequency);
    Q_INVOKABLE void setDraftGain(double gain);
    Q_INVOKABLE void setDraftQ(double q);
    Q_INVOKABLE void setDraftShelfSlope(double shelfSlope);

    Q_INVOKABLE void setDraftFrequencyText(const QString& text);
    Q_INVOKABLE void setDraftGainText(const QString& text);
    Q_INVOKABLE void setDraftQText(const QString& text);
    Q_INVOKABLE void setDraftShelfSlopeText(const QString& text);

    Q_INVOKABLE void setDraftSlopeDbPerOct(int slope);

    Q_INVOKABLE bool commitDraft();
    Q_INVOKABLE void cancelDraft();

    Q_INVOKABLE void graphDrag(double frequency, double gain);
    Q_INVOKABLE void graphRelease();
    Q_INVOKABLE void adjustSecondaryParameter(int steps, bool shiftPressed);

    // Trigger explicit preview render
    void trigger_preview();

signals:
    void changed();

private:
    [[nodiscard]] MasteringChainState& active_chain_state() const noexcept;
    [[nodiscard]] MasteringPreviewController& active_preview_controller() const noexcept;

    [[nodiscard]] core::SampleRate current_sample_rate() const noexcept;
    [[nodiscard]] bool is_mono_prepared() const noexcept;
    [[nodiscard]] double max_frequency_hz() const noexcept;

    [[nodiscard]] static DraftBand make_default_band(core::Uuid id) noexcept;
    [[nodiscard]] static std::optional<dsp::EqBandParameters> make_band_parameters(
        const DraftBand& draft,
        double max_freq);

    void update_response_grid();
    void update_validation_state();
    void request_preview();
    void push_undo_snapshot(EqStateSnapshot previousSnapshot);
    [[nodiscard]] EqStateSnapshot capture_current_snapshot() const;
    void restore_snapshot(const EqStateSnapshot& snapshot);

    MasteringChainState* externalChainState_{nullptr};
    std::unique_ptr<MasteringChainState> ownedChainState_;

    MasteringPreviewController* externalPreviewController_{nullptr};
    std::unique_ptr<MasteringPreviewController> ownedPreviewController_;

    IdGenerator idGenerator_;

    std::vector<dsp::EqBandParameters> committedBands_;
    DraftBand draftBand_;
    std::size_t selectedIndex_{0};

    std::vector<EqStateSnapshot> undoStack_;
    std::vector<EqStateSnapshot> redoStack_;

    QString validationField_;
    QString validationMessage_;

    QVariantList responseGrid_;
    QVariantList combinedResponseGrid_;
    bool showCombinedResponse_{false};
};

}  // namespace rgsml::app
