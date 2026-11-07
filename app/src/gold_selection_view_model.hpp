#pragma once

#include <rgsml/audio/source_resource.hpp>

#include <QObject>
#include <QThread>
#include <QString>
#include <QUrl>

#include <optional>
#include <functional>
#include <utility>

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
    [[nodiscard]] const audio::SourceResource* gold_resource() const noexcept;
    // PREPRODUCTION M18: Product-owned admission before an independently
    // selected GOLD replacement or clear. Inert unless explicitly installed.
    // SOURCE-driven GOLD retirement must instead be within its SOURCE/Project
    // epoch transaction, because the selector may already be invalidated.
    using M18GoldMutationAdmission=std::function<core::Status(const char*)>;
    void set_private_m18_gold_mutation_admission(M18GoldMutationAdmission gate)
    {
        // Only the QObject owner may configure the private Product gate.
        // Keep its callback installed across admission and Qt publication.
        if(QThread::currentThread()!=thread())return;
        if(m18GoldAdmitting_||m18GoldPublicationActive_)return;
        m18GoldMutationAdmission_=std::move(gate);
    }
    [[nodiscard]] core::Status adopt_probed_gold(audio::SourceResource candidate);

    Q_INVOKABLE void selectGold(const QUrl& selectedFile);
    Q_INVOKABLE void cancelGoldSelection() noexcept;
    Q_INVOKABLE void clearGold();
    void sourceChanged();

signals:
    void changed();

private:
    [[nodiscard]] core::Status admit_m18_gold_mutation(const char* operation) const;
    AuditionSourceSelector* selector_;
    M18GoldMutationAdmission m18GoldMutationAdmission_;
    // Owner-thread only: a callback must not install a nested M18 epoch,
    // and a Qt changed() listener must not reenter GOLD publication.
    mutable bool m18GoldAdmitting_{false};
    bool m18GoldPublicationActive_{false};
    std::optional<audio::SourceResource> gold_;
    QString displayName_;
    QString metadata_;
    QString errorMessage_;
};

}  // namespace rgsml::app
