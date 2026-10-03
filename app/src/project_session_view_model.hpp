#pragma once

#include <rgsml/project/project_snapshot.hpp>
#include <rgsml/core/resource_reference.hpp>

#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>
#include <optional>

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

class ProjectSessionViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canSaveProject READ can_save_project NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ error_message NOTIFY changed)
    Q_PROPERTY(bool degraded READ degraded NOTIFY changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
    Q_PROPERTY(QString projectDisplayName READ project_display_name NOTIFY changed)

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

    [[nodiscard]] bool can_save_project() const noexcept;
    [[nodiscard]] bool is_committing_project_open() const noexcept { return isCommittingProjectOpen_; }
    [[nodiscard]] QString error_message() const { return errorMessage_; }
    [[nodiscard]] bool degraded() const noexcept { return degraded_; }
    [[nodiscard]] QString status_text() const { return statusText_; }
    [[nodiscard]] QString project_display_name() const { return projectDisplayName_; }

    Q_INVOKABLE void openProject(const QUrl& selectedFile);
    Q_INVOKABLE void saveProjectAs(const QUrl& selectedFile);
    Q_INVOKABLE void cancelProjectOpen() noexcept;
    Q_INVOKABLE void cancelProjectSave() noexcept;

signals:
    void changed();

private:
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
