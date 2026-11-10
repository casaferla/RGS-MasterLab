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
            if(active_ &&completed->request_id==active_->id
               &&completed->result) {
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
        }
        if(worker_.busy())return;

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
                return;
            }

            auto captured=Product::capture(
                product_,*fanout_,*selector_);
            if(!captured)continue;
            const auto& evidence=*captured.value();
            if(column==Column::processed &&!evidence.processed)
                continue;
            const auto key=render_key(evidence,column);
            const auto index=static_cast<std::size_t>(column);
            if(attempted_[index] &&*attempted_[index]==key)continue;
            auto task=[evidence,column]() -> std::optional<Snapshot> {
                auto result=Product::measure_render_worker(evidence,column);
                if(!result)return std::nullopt;
                return std::move(*result.value());
            };
            const auto id=next_request_id_;
            if(!worker_.start(id,std::move(task)))return;
            next_request_id_+=1;
            active_=Active{id,column,evidence,std::nullopt};
            attempted_[index]=key;
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
            std::to_string(captured.source_full_frames)+"|"+
            captured.source.provider_id()+"|"+captured.source.locator()+"|"+
            std::to_string(static_cast<unsigned>(column))+"|";
        if(column==Column::processed &&captured.processed_realization_id)
            key+=std::to_string(captured.processed_realization_id->value);
        return key;
    }
    [[nodiscard]] static std::string gold_key(
        const Product::GoldCapture& captured)
    {
        return std::to_string(captured.owner.epoch)+"|"+
            std::to_string(captured.owner.identity.source_generation)+"|"+
            captured.gold.provider_id()+"|"+captured.gold.locator()+"|"+
            std::to_string(captured.sample_rate_hz)+"|"+
            std::to_string(captured.full_frames)+"|"+
            std::to_string(captured.channels);
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
    std::array<std::optional<std::string>,3> attempted_{};
    std::optional<Active> active_{};
};
} // namespace rgsml::app::internal
