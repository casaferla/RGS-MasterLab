#pragma once
// M21 genuine Product integration: strict Qt-owner capture, ONE
// immutable off-thread M19/M20 analysis, Qt-owner acceptance into the
// existing PRIVATE M21 inbox. Never sends private numbers to QML and
// never grants official Phase-6 FeatureStatus/Gate S/U/G authority.
// A mutation while a worker runs is harmless: the same physical M18
// epoch + resource + scope must pass accept_render/accept_gold on return.
// The worker owns neither a QObject nor a reference to this coordinator.
// Work is single-flight and source-key coalesced: no burst of workers
// on each QML refresh, navigation, or Qt notification.
#include "m21_product_owner_analysis.hpp"
#include "m21_product_publication_inbox.hpp"
#include "m21_private_single_flight.hpp"
#include "m21_product_status_view_model.hpp"
#include <QThread>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::app::internal {
class M21PrivateProductAnalysisCoordinator final {
public:
    using Product=M21ProductOwnerAnalysis;
    using Inbox=M21ProductPublicationInbox;
    using Controllers=Product::Controllers;
    using Fanout=Product::Fanout;
    using Column=Product::Column;
    using Snapshot=m21_private::Snapshot;

    M21PrivateProductAnalysisCoordinator(
        Controllers product,const Fanout& fanout,
        AuditionSourceSelector& selector,GoldSelectionViewModel& gold,
        Inbox& inbox,M21ProductStatusViewModel& status)
        :product_(product),fanout_(&fanout),selector_(&selector),
         gold_(&gold),inbox_(&inbox),status_(&status),
         owner_(QThread::currentThread()) {}
    M21PrivateProductAnalysisCoordinator(
        const M21PrivateProductAnalysisCoordinator&)=delete;
    M21PrivateProductAnalysisCoordinator&
    operator=(const M21PrivateProductAnalysisCoordinator&)=delete;

    // Called periodically from the real Product event loop. Only this
    // entrypoint may inspect the mutable M18/Gold Qt controllers.
    void tick() {
        if(QThread::currentThread()!=owner_ ||
           owner_!=selector_->thread() ||
           owner_!=gold_->thread() ||
           owner_!=product_.session.thread() ||
           owner_!=status_->thread())
            return;

        if(auto completed=worker_.take_finished()) {
            const auto expectedGeneration=!active_ ? std::uint64_t{0}
                : active_->gold ? active_->gold->owner.epoch
                : active_->render ? active_->render->owner.epoch
                : std::uint64_t{0};
            if(active_ &&M21PrivateSingleFlight::matches_expected(
                *completed,active_->id,active_->column,
                expectedGeneration)) {
                bool accepted=false;
                const auto& evidence=*completed->result;
                if(active_->column==Column::gold &&active_->gold)
                    accepted=inbox_->accept_gold(
                        product_,*fanout_,*selector_,*gold_,
                        *active_->gold,evidence);
                else if(active_->render)
                    accepted=inbox_->accept_render(
                        product_,*fanout_,*selector_,
                        *active_->render,evidence);
                // Only quality/lifecycle labels are exposed; not values.
                if(accepted)status_->refresh();
            }
            active_.reset();
            status_->set_running_analysis_role({});
        }
        // Preserve unaffected numeric measurements even while a previous
        // single-flight CPU worker is retiring. No worker is started here:
        // only immutable, already computed results are reauthenticated.
        const auto unchanged=inbox_->rebind_unchanged_prepared_reference(
            product_,*fanout_,*selector_,*gold_);
        if(unchanged[static_cast<std::size_t>(Column::prepared)]
           ||unchanged[static_cast<std::size_t>(Column::gold)]) {
            if(unchanged[static_cast<std::size_t>(Column::prepared)]) {
                const auto fresh=Product::capture(
                    product_,*fanout_,*selector_);
                if(fresh) {
                    renderAttempts_[
                        static_cast<std::size_t>(Column::prepared)]
                        .remember(render_key(*fresh.value(),Column::prepared),
                                  fresh.value()->prepared);
                }
            }
            if(unchanged[static_cast<std::size_t>(Column::gold)]) {
                const auto fresh=Product::capture_gold(
                    product_,*fanout_,*selector_,*gold_);
                if(fresh)attempted_[
                    static_cast<std::size_t>(Column::gold)]=
                        gold_key(*fresh.value());
            }
            status_->refresh();
        }
        if(worker_.busy())return;

        // A genuine REFERENCE-only change advances the global M18 epoch,
        // but does not change any retained PREPARED/PROCESSED PCM. The
        // inbox alone decides whether old measurements may be re-stamped
        // under the new epoch after STRICT source/chain/render validation.
        const auto reused=inbox_->rebind_after_reference_only_change(
            product_,*fanout_,*selector_,*gold_);
        if(reused[static_cast<std::size_t>(Column::prepared)]
           ||reused[static_cast<std::size_t>(Column::processed)]) {
            // Update per-render deduplication only AFTER actual inbox
            // acceptance; otherwise tick() would recompute immediately.
            auto fresh=Product::capture(product_,*fanout_,*selector_);
            if(fresh) {
                for(const auto column:{Column::prepared,Column::processed}) {
                    const auto i=static_cast<std::size_t>(column);
                    if(!reused[i])continue;
                    const auto render=column==Column::prepared
                        ?fresh.value()->prepared:fresh.value()->processed;
                    renderAttempts_[i].remember(
                        render_key(*fresh.value(),column),render);
                }
            }
            status_->refresh();
        }

        // Fixed round-robin priority, at most ONE CPU analysis active;
        // attempts are remembered per exact Product identity so a
        // nonpublishable or unsupported WAV cannot spin at 4 Hz forever.
        // A source/epoch/realization change gives a new independent key.
        for(const auto column:{Column::prepared,Column::gold,Column::processed}) {
            if(column==Column::gold) {
                auto captured=Product::capture_gold(
                    product_,*fanout_,*selector_,*gold_);
                if(!captured)continue;
                const auto& evidence=*captured.value();
                const auto key=gold_key(evidence);
                const auto index=static_cast<std::size_t>(column);
                if(attempted_[index] &&*attempted_[index]==key)continue;
                auto task=[evidence]() -> std::optional<Snapshot> {
                    auto result=Product::measure_gold_worker(evidence);
                    if(!result)return std::nullopt;
                    return std::move(*result.value());
                };
                const auto id=next_request_id_;
                if(!worker_.start(id,std::move(task)))return;
                next_request_id_+=1;
                active_=Active{id,column,std::nullopt,evidence};
                attempted_[index]=key;
                status_->set_running_analysis_role(QStringLiteral("REFERENCE"));
                return;
            }

            auto captured=Product::capture(
                product_,*fanout_,*selector_);
            if(!captured)continue;
            const auto& evidence=*captured.value();
            if(column==Column::processed &&!evidence.processed)
                continue;
            // Exact immutable render identity is independent of the M18
            // owner epoch. A new render can be installed in the SAME
            // epoch: suppress only when BOTH the key and retained PCM
            // owner are identical. Weak ownership avoids pinning old PCM.
            const auto render=column==Column::prepared
                ?evidence.prepared:evidence.processed;
            auto key=render_key(evidence,column);
            const auto index=static_cast<std::size_t>(column);
            if(renderAttempts_[index].matches(key,render))continue;
            auto task=[evidence,column]() -> std::optional<Snapshot> {
                auto result=Product::measure_render_worker(evidence,column);
                if(!result)return std::nullopt;
                return std::move(*result.value());
            };
            const auto id=next_request_id_;
            if(!worker_.start(id,std::move(task)))return;
            next_request_id_+=1;
            active_=Active{id,column,evidence,std::nullopt};
            renderAttempts_[index].remember(std::move(key),render);
            status_->set_running_analysis_role(column==Column::prepared
                ?QStringLiteral("PREPARED"):QStringLiteral("PROCESSED"));
            return;
        }
    }

    [[nodiscard]] bool busy() const noexcept {return worker_.busy();}

private:
    struct Active final {
        std::uint64_t id{};
        Column column{Column::prepared};
        std::optional<Product::Capture> render{};
        std::optional<Product::GoldCapture> gold{};
    };

    [[nodiscard]] static std::string render_key(
        const Product::Capture& captured,Column column)
    {
        std::string key=
            std::to_string(captured.owner.epoch)+"|"+
            std::to_string(captured.owner.identity.source_generation)+"|"+
            std::to_string(captured.owner.identity.chain_revision)+"|"+
            std::to_string(captured.owner.identity.master_rate_revision)+"|"+
            std::to_string(captured.owner.identity.master_rate_hz)+"|"+
            std::to_string(captured.source_full_frames)+"|";
        M21PrivateSingleFlight::append_identity_field(
            key,captured.source.provider_id());
        M21PrivateSingleFlight::append_identity_field(
            key,captured.source.locator());
        key+=std::to_string(static_cast<unsigned>(column))+"|";
        if(column==Column::processed &&captured.processed_realization_id)
            key+=std::to_string(captured.processed_realization_id->value);
        return key;
    }
    [[nodiscard]] static std::string gold_key(
        const Product::GoldCapture& captured)
    {
        std::string key=std::to_string(captured.owner.epoch)+"|"+
            std::to_string(captured.owner.identity.source_generation)+"|"+
            std::to_string(captured.owner.identity.chain_revision)+"|"+
            std::to_string(captured.owner.identity.master_rate_revision)+"|"+
            std::to_string(captured.owner.identity.master_rate_hz)+"|";
        M21PrivateSingleFlight::append_identity_field(
            key,captured.gold.provider_id());
        M21PrivateSingleFlight::append_identity_field(
            key,captured.gold.locator());
        key+=std::to_string(captured.sample_rate_hz)+"|"+
            std::to_string(captured.full_frames)+"|"+
            std::to_string(captured.channels);
        return key;
    }

    Controllers product_;
    const Fanout* fanout_;
    AuditionSourceSelector* selector_;
    GoldSelectionViewModel* gold_;
    Inbox* inbox_;
    M21ProductStatusViewModel* status_;
    QThread* owner_;
    M21PrivateSingleFlight worker_;
    std::uint64_t next_request_id_{1};
    // REFERENCE comes from an independently selected WAV file, so its
    // attempt key is a resource+epoch identity rather than a RenderResult.
    std::array<std::optional<std::string>,3> attempted_{};
    std::array<M21PrivateRenderAttempt,3> renderAttempts_{};
    std::optional<Active> active_{};
};
} // namespace rgsml::app::internal
