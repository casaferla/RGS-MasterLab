#pragma once

#include <rgsml/project/project_snapshot.hpp>
#include <rgsml/project/m18_master_wav_preference.hpp>
#include <rgsml/core/resource_reference.hpp>

#include <QObject>
#include <QThread>
#include <QString>
#include <QUrl>

#include <functional>
#include <optional>
#include <memory>
#include <utility>

namespace rgsml::app {

class SourceSelectionViewModel;
class GoldSelectionViewModel;
class AuditionRegionViewModel;
class PlaybackTransportViewModel;
class MasteringChainState;
class GainViewModel;
class EqViewModel;
class DspChainAdapterModel;
class MasteringPreviewController;
class MasterOutputRateState;

class ProjectSessionViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canSaveProject READ can_save_project NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ error_message NOTIFY changed)
    Q_PROPERTY(bool degraded READ degraded NOTIFY changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
    Q_PROPERTY(QString projectDisplayName READ project_display_name NOTIFY changed)
    Q_PROPERTY(int m18WavEncodingIndex READ m18_wav_encoding_index NOTIFY changed)

public:
    using UuidFactory = std::function<core::Uuid()>;

    ProjectSessionViewModel(SourceSelectionViewModel* source,
                            GoldSelectionViewModel* gold,
                            AuditionRegionViewModel* region,
                            PlaybackTransportViewModel* playback,
                            MasteringChainState* masteringChainState = nullptr,
                            GainViewModel* gainViewModel = nullptr,
                            EqViewModel* eqViewModel = nullptr,
                            MasteringPreviewController* previewController = nullptr,
                            UuidFactory uuidFactory = {},
                            QObject* parent = nullptr);

    ProjectSessionViewModel(SourceSelectionViewModel* source,
                            GoldSelectionViewModel* gold,
                            AuditionRegionViewModel* region,
                            PlaybackTransportViewModel* playback,
                            MasteringChainState* masteringChainState,
                            GainViewModel* gainViewModel,
                            EqViewModel* eqViewModel,
                            DspChainAdapterModel* adapterModel,
                            MasteringPreviewController* previewController = nullptr,
                            UuidFactory uuidFactory = {},
                            QObject* parent = nullptr);

    // M18 PREPRODUCTION: one opt-in Product-authoritative epoch gate
    // before the FIRST destructive Project-open or SaveAs publication.
    // No production caller installs this until all other mutators are
    // reconciled. This gate MUST be shared with SOURCE/MASTER admission.
    using M18ProjectMutationAdmission=std::function<core::Status(const char*)>;
    void set_private_m18_project_mutation_admission(
        M18ProjectMutationAdmission gate)
    {
        if(QThread::currentThread()!=thread()||m18ProjectOperationActive_)return;
        m18ProjectMutationAdmission_=std::move(gate);
    }
    // M18 PREPRODUCTION: optional one-shot Product batch transaction,
    // acquired BEFORE stopping the transport and kept alive until the
    // entire Project-open function has returned (including error paths).
    // Product may install this INSTEAD of a per-step open admission.
    // This opaque RAII token must itself own the genuine exclusive epoch
    // batch lease and is never sent to a worker. No GUI caller sets it.
    using M18ProjectOpenBatchBegin=
        std::function<core::Result<std::shared_ptr<const void>>()>;
    void set_private_m18_project_open_batch_begin(
        M18ProjectOpenBatchBegin begin)
    {
        if(QThread::currentThread()!=thread()||m18ProjectOperationActive_)return;
        m18ProjectOpenBatchBegin_=std::move(begin);
    }
    // A Project-open batch and a one-shot generic Project gate cannot be
    // installed together; SaveAs still needs a separate Product admission.
    // If a batch is configured without this additional SaveAs guard,
    // SaveAs MUST fail closed (never write an unguarded .rgsml file).
    using M18ProjectSaveAdmission=std::function<core::Status(const char*)>;
    void set_private_m18_project_save_admission(
        M18ProjectSaveAdmission gate)
    {
        if(QThread::currentThread()!=thread()||m18ProjectOperationActive_)return;
        m18ProjectSaveAdmission_=std::move(gate);
    }
    // Unlike a one-shot status admission, this Product-issued RAII token
    // holds a real exclusive mutation epoch across snapshot serialization,
    // file creation, postwrite verification and Project state publication.
    // Required whenever an M18 Project-open batch has been configured.
    using M18ProjectSaveBatchBegin=
        std::function<core::Result<std::shared_ptr<const void>>()>;
    void set_private_m18_project_save_batch_begin(
        M18ProjectSaveBatchBegin begin)
    {
        if(QThread::currentThread()!=thread()||m18ProjectOperationActive_)return;
        m18ProjectSaveBatchBegin_=std::move(begin);
    }
    [[nodiscard]] bool m18_open_batch_installed() const noexcept
    {return static_cast<bool>(m18ProjectOpenBatchBegin_);}
    [[nodiscard]] bool m18_save_batch_installed() const noexcept
    {return static_cast<bool>(m18ProjectSaveBatchBegin_);}
    [[nodiscard]] bool m18_project_one_shot_installed() const noexcept
    {return static_cast<bool>(m18ProjectMutationAdmission_);}
    [[nodiscard]] bool m18_save_one_shot_installed() const noexcept
    {return static_cast<bool>(m18ProjectSaveAdmission_);}
    void set_master_output_rate_state(MasterOutputRateState* state) noexcept {
        masterOutputRateState_ = state;
    }
    [[nodiscard]] bool can_save_project() const noexcept;
    [[nodiscard]] bool is_committing_project_open() const noexcept { return isCommittingProjectOpen_; }
    [[nodiscard]] QString error_message() const { return errorMessage_; }
    [[nodiscard]] bool degraded() const noexcept { return degraded_; }
    [[nodiscard]] QString status_text() const { return statusText_; }
    [[nodiscard]] QString project_display_name() const { return projectDisplayName_; }

    // Persist only an explicit user choice. This is NOT WAV publication,
    // a DSP change or a Master epoch authorization. It is read by the
    // Product Export action on the Qt owner thread at consent time.
    [[nodiscard]] int m18_wav_encoding_index() const noexcept {
        return m18OutputPreference_
            &&m18OutputPreference_->encoding
                ==project::M18MasterWavEncoding::IEEE_FLOAT32?1:0;
    }
    [[nodiscard]] std::uint64_t m18_pcm24_seed() const noexcept {
        return m18OutputPreference_
            &&m18OutputPreference_->encoding
                ==project::M18MasterWavEncoding::PCM24_TPDF
            &&m18OutputPreference_->pcm24_dither_seed
            ?*m18OutputPreference_->pcm24_dither_seed
            :project::M18MasterWavPreferenceCodec::default_pcm24_seed();
    }
    Q_INVOKABLE bool chooseM18WavEncoding(int index);

    Q_INVOKABLE void openProject(const QUrl& selectedFile);
    Q_INVOKABLE void saveProjectAs(const QUrl& selectedFile);
    Q_INVOKABLE void cancelProjectOpen() noexcept;
    Q_INVOKABLE void cancelProjectSave() noexcept;

signals:
    void changed();

private:
    [[nodiscard]] core::Status admit_m18_project_mutation(
        const char* operation) const;
    [[nodiscard]] core::Result<std::shared_ptr<const void>>
    begin_m18_project_save() const;
    [[nodiscard]] core::Result<project::ProjectSnapshot> current_snapshot(
        const QString& destinationStem);
    [[nodiscard]] core::Uuid next_id();
    void publish_error(const core::Error& error);
    void clear_error();
    void on_source_changed();
    void on_gold_changed();
    void on_region_changed();

    SourceSelectionViewModel* source_;
    GoldSelectionViewModel* gold_;
    AuditionRegionViewModel* region_;
    PlaybackTransportViewModel* playback_;
    MasteringChainState* masteringChainState_{nullptr};
    GainViewModel* gainViewModel_{nullptr};
    EqViewModel* eqViewModel_{nullptr};
    DspChainAdapterModel* adapterModel_{nullptr};
    MasteringPreviewController* previewController_{nullptr};
    MasterOutputRateState* masterOutputRateState_{nullptr};
    std::optional<project::M18MasterWavPreference> m18OutputPreference_;
    M18ProjectMutationAdmission m18ProjectMutationAdmission_;
    M18ProjectOpenBatchBegin m18ProjectOpenBatchBegin_;
    M18ProjectSaveAdmission m18ProjectSaveAdmission_;
    M18ProjectSaveBatchBegin m18ProjectSaveBatchBegin_;
    // Owner-thread only, scoped to one Open/Save command including
    // synchronous signals and filesystem readback. With any M18 policy
    // installed, nested Project operations fail before another admission.
    bool m18ProjectOperationActive_{false};
    UuidFactory uuidFactory_;
    bool isCommittingProjectOpen_{false};
    std::optional<project::ProjectSnapshot> opened_;
    std::optional<core::Uuid> projectId_;
    std::optional<core::Uuid> sourceId_;
    std::optional<core::Uuid> goldId_;
    std::optional<core::Uuid> referenceId_;
    std::optional<core::Uuid> regionId_;
    std::optional<core::Uuid> clearedRegionId_;
    std::optional<core::ResourceReference> lastSource_;
    std::optional<core::ResourceReference> lastGold_;
    bool degraded_{false};
    bool sessionChainMaterialized_{false};
    QString projectDisplayName_;
    QString errorMessage_;
    QString statusText_;
};

}  // namespace rgsml::app
