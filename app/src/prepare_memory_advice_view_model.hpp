#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace rgsml::platform::windows {
class WindowsAudioPlaybackService;
}
namespace rgsml::app {
class SourceSelectionViewModel;

// Read-only Prepare UI bridge. Each refresh uses a NEW device/memory snapshot.
// The rows are advice only: never a rate selection, allocation permit or CPU
// qualification. In particular, the current full-track monitor remains O(track).
class PrepareMemoryAdviceViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList recommendations READ recommendations NOTIFY changed)
    Q_PROPERTY(QString statusText READ status_text NOTIFY changed)
public:
    PrepareMemoryAdviceViewModel(
        const SourceSelectionViewModel* source,
        const platform::windows::WindowsAudioPlaybackService* playback,
        QObject* parent = nullptr);
    [[nodiscard]] QVariantList recommendations() const;
    [[nodiscard]] QString status_text() const;
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    void reset_unknown(const QString& status);
    const SourceSelectionViewModel* source_;
    const platform::windows::WindowsAudioPlaybackService* playback_;
    QVariantList recommendations_;
    QString statusText_;
};

} // namespace rgsml::app
