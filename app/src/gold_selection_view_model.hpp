#pragma once

#include <rgsml/audio/source_resource.hpp>

#include <QObject>
#include <QString>
#include <QUrl>

#include <optional>

namespace rgsml::app {

class AuditionSourceSelector;

class GoldSelectionViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasGold READ has_gold NOTIFY changed)
    Q_PROPERTY(QString displayName READ display_name NOTIFY changed)
    Q_PROPERTY(QString metadata READ metadata NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ error_message NOTIFY changed)

public:
    GoldSelectionViewModel(
        AuditionSourceSelector* selector,
        QObject* parent = nullptr);

    [[nodiscard]] bool has_gold() const noexcept;
    [[nodiscard]] QString display_name() const;
    [[nodiscard]] QString metadata() const;
    [[nodiscard]] QString error_message() const;

    Q_INVOKABLE void selectGold(const QUrl& selectedFile);
    Q_INVOKABLE void cancelGoldSelection() noexcept;
    Q_INVOKABLE void clearGold();
    void sourceChanged();

signals:
    void changed();

private:
    AuditionSourceSelector* selector_;
    std::optional<audio::SourceResource> gold_;
    QString displayName_;
    QString metadata_;
    QString errorMessage_;
};

}  // namespace rgsml::app
