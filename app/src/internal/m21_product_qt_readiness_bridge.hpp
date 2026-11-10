#pragma once
// M18-M22 isolated Product-to-Qt readiness bridge.
// This uses the *actual* current M18 owner and M21 publication inbox.
// It emits NO metric numbers, no Product QML registration and no
// Phase-6 approval. The public visual workbench stays fail-closed.
#include "m21_product_publication_inbox.hpp"
#include "m21_independent_panels_review_candidate.hpp"
#include "m21_qt_snapshot_review_candidate.hpp"
#include <QThread>
#include <QVariantMap>
#include <QString>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace rgsml::app::internal {

class M21ProductQtReadinessBridge final {
public:
    using Inbox=M21ProductPublicationInbox;
    using Product=M21ProductOwnerAnalysis;
    using Controllers=Product::Controllers;
    using Fanout=Product::Fanout;
    using Column=m21_private::Column;
    using Lifecycle=m21_private::Lifecycle;
    using Routing=m21_private::ui_candidate::panel_review::IndependentPanelRouting;
    using AnalysisPanel=m21_private::ui_candidate::panel_review::AnalysisPanel;

    struct Identity final {
        std::string resource{};
        std::uint64_t generation{};
        Lifecycle lifecycle{Lifecycle::missing};
        std::string scope{};
        std::uint64_t start{};
        std::uint64_t frames{};
        std::uint32_t sample_rate{};
        [[nodiscard]] bool operator==(const Identity&) const = default;
    };
    struct Frame final {
        std::uint64_t owner_epoch{};
        // The complete M18 owner identity must travel with the private
        // pending Qt frame; an epoch number alone is not a source identity.
        platform::windows::internal::M18ProductExportIdentity owner_identity{};
        std::uint64_t routing_revision{};
        bool current_product_owner{false};
        bool product_ui_authorized{false}; // ALWAYS FALSE in this bridge
        std::array<Column,3> selected{{
            Column::prepared,Column::prepared,Column::prepared}};
        std::array<Identity,3> identities{};
        std::array<QVariantMap,3> panels{};
    };

    // Equality includes every field observable by QML *and* the complete
    // owner/routing identity. Never suppress a genuine SOURCE, REFERENCE,
    // Product epoch, quality, scope or per-panel status transition.
    // This is only a notification coalescer, NOT an owner authority.
    [[nodiscard]] static bool same_visible_frame(
        const Frame& a,const Frame& b)
    {
        return a.owner_epoch==b.owner_epoch
            &&a.owner_identity==b.owner_identity
            &&a.routing_revision==b.routing_revision
            &&a.current_product_owner==b.current_product_owner
            &&a.product_ui_authorized==b.product_ui_authorized
            &&a.selected==b.selected
            &&a.identities==b.identities
            &&a.panels==b.panels;
    }

    // Must be called on the true Qt Product owner, not a numerical worker.
    // Missing, invalid and stale evidence must never turn into fake zeros.
    [[nodiscard]] static Frame capture(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,const Inbox& inbox,
        const Routing& routing)
    {
        Frame result;
        // Wrong-thread callers must never inspect a mutable Qt-owned
        // panel routing or Product QObject. Return only static masks.
        const bool on_owner_thread=
            QThread::currentThread()==selector.thread()
            &&QThread::currentThread()==gold.thread()
            &&QThread::currentThread()==product.session.thread();
        constexpr std::array<AnalysisPanel,3> panels{{
            AnalysisPanel::levels_m19,AnalysisPanel::tonal_m20,
            AnalysisPanel::delta_m21}};
        constexpr std::array<const char*,3> panel_names{{"M19","M20","M21"}};
        constexpr std::array<const char*,3> role_names{{
            "PREPARED","REFERENCE","PROCESSED"}};
        for(std::size_t i=0;i<3;++i) {
            result.panels[i]=m21_private::ui_candidate::qt_review::
                masked_unpublished_envelope();
            result.panels[i].insert("panelId",
                QString::fromLatin1(panel_names[i]));
            result.panels[i].insert("sourceRole",
                QStringLiteral("UNAVAILABLE"));
            result.panels[i].insert("privateEvidenceStatus",
                QStringLiteral("MISSING"));
        }
        if(!on_owner_thread)return result;
        result.routing_revision=routing.routing_revision();
        for(std::size_t i=0;i<3;++i) {
            result.selected[i]=routing.selected(panels[i]);
            result.panels[i].insert("sourceRole",
                QString::fromLatin1(role_names[
                    static_cast<std::size_t>(result.selected[i])]));
        }
        if(!Product::Binding::installed(product)
           ||!Product::Binding::Upstream::
                owner_thread_current(product.upstream)
           ||!fanout.can_install_full_product_guards())
            return result;
        const auto owner=fanout.current_owner_snapshot();
        if(!owner||!owner->identity.valid()||owner->epoch==0)
            return result;
        result.current_product_owner=true;
        result.owner_epoch=owner->epoch;
        result.owner_identity=owner->identity;
        const auto current=inbox.view_current(
            product,fanout,selector,gold);
        for(std::size_t i=0;i<3;++i) {
            const auto index=static_cast<std::size_t>(result.selected[i]);
            if(index>=current.columns.size())continue;
            const auto& selected=current.columns[index];
            if(!selected)continue;
            const auto& s=*selected;
            result.identities[i]=Identity{
                s.resource_identity,s.generation_token,s.lifecycle,
                s.analysis_scope_label,s.absolute_scope_begin,s.scope_frames,
                s.sample_rate_hz};
            const auto status=m21_private::ui_candidate::
                lifecycle_label(s.lifecycle);
            result.panels[i].insert("privateEvidenceStatus",
                QString::fromStdString(status));
            // Quality labels are observed from the genuine private M19/M20
            // Product result, NEVER inferred from displayed strings. They
            // carry no numeric values and cannot authorize any Qt readout.
            // A stale/invalid/missing column exposes NO old per-cell status.
            if(s.lifecycle==Lifecycle::current) {
                static constexpr std::array<const char*,9> names{{
                    "lufs","rms","samplePeak","truePeak","crest",
                    "width","P10","P50","P90"}};
                QVariantMap quality;
                const auto begin=i==0 ? std::size_t{0} :
                    i==1 ? std::size_t{5} : std::size_t{0};
                const auto end=i==0 ? std::size_t{5} :
                    i==1 ? std::size_t{9} : std::size_t{9};
                for(std::size_t metric=begin;metric<end;++metric) {
                    const auto tag=metric==3
                      ? QStringLiteral("M16 UNQUALIFIED")
                      : QString::fromStdString(
                          m21_private::ui_candidate::quality_label(
                              s.metrics[metric].quality));
                    quality.insert(QString::fromLatin1(names[metric]),tag);
                }
                result.panels[i].insert("privateQualityOnly",quality);
            }
            // Not an official Product/Phase-6 status, even if the private
            // inbox holds an authenticated CURRENT numerical result.
            result.panels[i].insert("analysisStatus",
                QString::fromStdString(status));
            result.panels[i].insert("scopeLabel",
                s.lifecycle==Lifecycle::current
                  ?QString::fromStdString(s.analysis_scope_label)
                  :QStringLiteral("SCOPE UNAVAILABLE"));
        }
        return result;
    }

    // A queued Qt frame becomes unusable after any change of current
    // M18 epoch, source identity, result generation OR individual route.
    [[nodiscard]] static bool still_current(
        const Frame& expected,const Controllers& product,
        const Fanout& fanout,const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,const Inbox& inbox,
        const Routing& routing)
    {
        if(QThread::currentThread()!=selector.thread()
           ||QThread::currentThread()!=gold.thread()
           ||QThread::currentThread()!=product.session.thread()
           ||!expected.current_product_owner||expected.product_ui_authorized
           ||expected.owner_epoch==0
           ||!expected.owner_identity.valid()
           ||!routing.is_current_route(expected.routing_revision))
            return false;
        const auto now=capture(product,fanout,selector,gold,inbox,routing);
        return now.current_product_owner&&!now.product_ui_authorized
            &&now.owner_epoch==expected.owner_epoch
            &&now.owner_identity==expected.owner_identity
            &&now.routing_revision==expected.routing_revision
            &&now.selected==expected.selected
            &&now.identities==expected.identities;
    }
};
} // namespace rgsml::app::internal
