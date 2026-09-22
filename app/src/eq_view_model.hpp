#pragma once

#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/render/render_result.hpp>

#include <QObject>
#include <QString>

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
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
    Q_PROPERTY(int slopeDbPerOct READ slope_db_per_oct NOTIFY changed)
    Q_PROPERTY(bool gainApplicable READ gain_applicable NOTIFY changed)
    Q_PROPERTY(bool qApplicable READ q_applicable NOTIFY changed)
    Q_PROPERTY(bool shelfSlopeApplicable READ shelf_slope_applicable NOTIFY changed)
    Q_PROPERTY(bool slopeApplicable READ slope_applicable NOTIFY changed)
    Q_PROPERTY(bool addAvailable READ add_available NOTIFY changed)
    Q_PROPERTY(bool removeAvailable READ remove_available NOTIFY changed)
    Q_PROPERTY(bool routeAvailable READ route_available NOTIFY changed)
    Q_PROPERTY(bool mixedRouting READ mixed_routing NOTIFY changed)
    Q_PROPERTY(bool bypass READ bypass NOTIFY changed)
    Q_PROPERTY(quint64 previewGeneration READ preview_generation NOTIFY changed)
    Q_PROPERTY(QString previewStatus READ preview_status NOTIFY changed)
    Q_PROPERTY(QString previewError READ preview_error NOTIFY changed)
    Q_PROPERTY(QVariantList selectedBandResponsePoints READ selected_band_response_points NOTIFY changed)

public:
    using PreparedSnapshotProvider = std::function<std::shared_ptr<const render::RenderResult>()>;
    using ProcessedRealizationPublisher = std::function<core::Status(render::RenderResult)>;
    using IdGenerator = std::function<core::Uuid()>;

    struct DraftBand final {
        core::Uuid band_id;
        bool enabled{true};
        dsp::EqFilterType filter_type{dsp::EqFilterType::BELL};
        dsp::EqRouting routing{dsp::EqRouting::STEREO};
        double frequency_hz{1000.0};
        double gain_db{0.0};
        double q{0.707};
        double shelf_slope{1.0};
        dsp::SlopeDbPerOctave slope_db_per_octave{dsp::SlopeDbPerOctave::DB_12};
    };

    explicit EqViewModel(
        PreparedSnapshotProvider snapshotProvider = nullptr,
        ProcessedRealizationPublisher publisher = nullptr,
        IdGenerator idGenerator = nullptr,
        QObject* parent = nullptr);
    ~EqViewModel() override;

    EqViewModel(const EqViewModel&) = delete;
    EqViewModel& operator=(const EqViewModel&) = delete;

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
    [[nodiscard]] int slope_db_per_oct() const noexcept;

    [[nodiscard]] bool gain_applicable() const noexcept;
    [[nodiscard]] bool q_applicable() const noexcept;
    [[nodiscard]] bool shelf_slope_applicable() const noexcept;
    [[nodiscard]] bool slope_applicable() const noexcept;

    [[nodiscard]] bool add_available() const noexcept;
    [[nodiscard]] bool remove_available() const noexcept;
    [[nodiscard]] bool route_available() const noexcept;
    [[nodiscard]] bool mixed_routing() const noexcept;
    [[nodiscard]] bool bypass() const noexcept;

    [[nodiscard]] quint64 preview_generation() const noexcept;
    [[nodiscard]] QString preview_status() const;
    [[nodiscard]] QString preview_error() const;
    [[nodiscard]] QVariantList selected_band_response_points() const;

    [[nodiscard]] core::Uuid instance_id() const noexcept;
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

    Q_INVOKABLE void setDraftFrequency(double frequency);
    Q_INVOKABLE void setDraftGain(double gain);
    Q_INVOKABLE void setDraftQ(double q);
    Q_INVOKABLE void setDraftShelfSlope(double shelfSlope);
    Q_INVOKABLE void setDraftSlopeDbPerOct(int slope);

    Q_INVOKABLE bool commitDraft();
    Q_INVOKABLE void cancelDraft();

    Q_INVOKABLE void graphDrag(double frequency, double gain);
    Q_INVOKABLE void graphRelease();

    // Trigger explicit preview render (e.g., when PREPARED realization becomes available)
    void trigger_preview();

signals:
    void changed();

private:
    struct PreviewJob final {
        std::uint64_t generation;
        dsp::ParametricEqParameters parameters;
        bool bypass;
        std::shared_ptr<const render::RenderResult> preparedSnapshot;
        core::Uuid instanceId;
    };

    [[nodiscard]] core::SampleRate current_sample_rate() const noexcept;
    [[nodiscard]] bool is_mono_prepared() const noexcept;
    [[nodiscard]] double max_frequency_hz() const noexcept;

    [[nodiscard]] static DraftBand make_default_band(core::Uuid id) noexcept;
    [[nodiscard]] static std::optional<dsp::EqBandParameters> make_band_parameters(
        const DraftBand& draft,
        double max_freq);

    void update_response_grid();
    void request_preview();
    void worker_loop();
    void publish_preview_result(
        std::uint64_t generation,
        std::shared_ptr<core::Result<render::RenderResult>> outcome);

    PreparedSnapshotProvider snapshotProvider_;
    ProcessedRealizationPublisher publisher_;
    IdGenerator idGenerator_;

    core::Uuid instanceId_;
    std::vector<dsp::EqBandParameters> committedBands_;
    dsp::ParametricEqParameters committedParams_;
    DraftBand draftBand_;
    std::size_t selectedIndex_{0};
    bool bypass_{false};

    std::uint64_t previewGeneration_{0};
    std::uint64_t staleResultsDiscarded_{0};
    QString previewStatus_{QStringLiteral("IDLE")};
    QString previewError_;

    QVariantList responseGrid_;

    std::mutex workerMutex_;
    std::condition_variable workerCond_;
    std::optional<PreviewJob> pendingJob_;
    bool workerStopping_{false};
    std::jthread workerThread_;
};

}  // namespace rgsml::app
