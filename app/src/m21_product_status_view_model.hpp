#pragma once
// M19/M20/M21 genuine Product owner -> Qt presentation status adapter.
// The private inbox is authenticated against live M18 generations.
// PO-approved real numerical DEVELOPMENT observation; official Phase-6
// FeatureStatus / Gate G qualification remains a distinct release gate.
#include "internal/m21_product_qt_readiness_bridge.hpp"
#include <QObject>
#include <QVariantMap>
#include <QString>

namespace rgsml::app {
class M21ProductStatusViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap m19 READ m19 NOTIFY changed)
    Q_PROPERTY(QVariantMap m20 READ m20 NOTIFY changed)
    Q_PROPERTY(QVariantMap m21 READ m21 NOTIFY changed)
    Q_PROPERTY(bool productionAuthorized READ production_authorized CONSTANT)
    // Strictly empty in current Product builds: only a future canonical
    // Phase-6 Gate G issuer may activate this release-only readout path.
    Q_PROPERTY(QVariantMap phase6ReleaseReadouts READ phase6_release_readouts
               NOTIFY changed)
    // PO-authorized DEVELOPMENT OBSERVATION of genuine current results.
    // Unlike certified Phase-6 release, this property is intentionally
    // usable before Gate G and never claims mathematical qualification.
    Q_PROPERTY(QVariantMap analysisReadouts READ analysis_readouts NOTIFY changed)
public:
    using Bridge=internal::M21ProductQtReadinessBridge;
    using Controllers=Bridge::Controllers;
    using Fanout=Bridge::Fanout;
    using Inbox=Bridge::Inbox;

    explicit M21ProductStatusViewModel(
        Controllers product,const Fanout& fanout,
        AuditionSourceSelector& selector,GoldSelectionViewModel& gold,
        const Inbox& inbox,QObject* parent=nullptr);

    [[nodiscard]] QVariantMap m19() const {return frame_.panels[0];}
    [[nodiscard]] QVariantMap m20() const {return frame_.panels[1];}
    [[nodiscard]] QVariantMap m21() const {return frame_.panels[2];}
    [[nodiscard]] bool production_authorized() const noexcept {return false;}
    // Existing authenticated inbox is the ONE eventual numerical source.
    // Current code does not issue a FeatureStatus or unlock Gate G.
    [[nodiscard]] QVariantMap phase6_release_readouts() const;
    [[nodiscard]] QVariantMap analysis_readouts() const;

    // Product-linked status is refreshed on the real Qt owner. Development
    // display admits only genuine CURRENT evidence; invalid or stale
    // sources cannot retain old visible numerical payloads.
    Q_INVOKABLE void refresh();
    // Separate per-panel routing is an ordinary UI choice, not the
    // authority to analyze a SOURCE or replace its REFERENCE.
    Q_INVOKABLE bool selectPanelSource(int panel,const QString& role);
signals:
    void changed();
private:
    [[nodiscard]] bool owner_thread() const noexcept;
    [[nodiscard]] static Bridge::Column column(const QString& role,
                                                bool& valid) noexcept;
    Controllers product_;
    const Fanout* fanout_;
    AuditionSourceSelector* selector_;
    GoldSelectionViewModel* gold_;
    const Inbox* inbox_;
    Bridge::Routing routing_{};
    Bridge::Frame frame_{};
};
} // namespace rgsml::app
