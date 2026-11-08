#pragma once
// L1 M19-M21 actual Product numerical worker handoff.
// Capture on the one Qt Product owner; measure immutable PCM on a worker;
// publish on the current owner only after the M18/Gold generation recheck.
// This is NOT a Phase-6 FeatureDefinitionRegistry or a QML release grant.
#include "m21_product_owner_analysis.hpp"
#include "m21_product_publication_inbox.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <QThread>
#include <cstdint>
#include <optional>
#include <utility>

namespace rgsml::app::internal {

class M21ProductAnalysisHandoff final {
public:
    using Owner=M21ProductOwnerAnalysis;
    using Inbox=M21ProductPublicationInbox;
    using Controllers=Owner::Controllers;
    using Fanout=Owner::Fanout;
    using Column=Owner::Column;
    using Snapshot=m21_private::Snapshot;
    struct Job final {
        Column column{Column::prepared};
        std::optional<Owner::Capture> render;
        std::optional<Owner::GoldCapture> reference;
        std::uint64_t captured_epoch{0};
        [[nodiscard]] bool structurally_valid() const noexcept {
            const bool defined=column==Column::prepared
                ||column==Column::processed||column==Column::gold;
            return defined &&captured_epoch>0
                &&(render.has_value()!=reference.has_value())
                &&((column==Column::gold)==reference.has_value())
                &&(!render||render->owner.epoch==captured_epoch)
                &&(!reference||reference->owner.epoch==captured_epoch);
        }
    };

    [[nodiscard]] static core::Result<Job> capture_on_owner(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,Column target)
    {
        using R=core::Result<Job>;
        // No Qt-owned mutable reads from a worker. The referenced Source
        // and independent Reference resources stay distinct.
        if(QThread::currentThread()!=product.session.thread()
           ||QThread::currentThread()!=selector.thread()
           ||QThread::currentThread()!=gold.thread())
            return R::failure(error("M21_HANDOFF_CAPTURE_REQUIRES_QT_OWNER"));
        if(target==Column::gold) {
            auto ref=Owner::capture_gold(product,fanout,selector,gold);
            if(!ref)return R::failure(*ref.error());
            const auto epoch=ref.value()->owner.epoch;
            return R::success(Job{
                target,std::nullopt,std::move(*ref.value()),epoch});
        }
        if(target!=Column::prepared && target!=Column::processed)
            return R::failure(error("M21_HANDOFF_INVALID_COLUMN"));
        auto render=Owner::capture(product,fanout,selector);
        if(!render)return R::failure(*render.error());
        if(target==Column::processed &&!render.value()->processed)
            return R::failure(error("M21_HANDOFF_MISSING_PROCESSED_OWNER"));
        const auto epoch=render.value()->owner.epoch;
        return R::success(Job{
            target,std::move(*render.value()),std::nullopt,epoch});
    }

    // Pure worker-side numerical work: only retained immutable PCM,
    // bounded authentic WAV reference IO and frozen metadata are read.
    // No QApplication, QObject, projector or dispatcher is touched.
    [[nodiscard]] static core::Result<Snapshot> measure_on_worker(
        const Job& job)
    {
        using R=core::Result<Snapshot>;
        if(!job.structurally_valid())
            return R::failure(error("M21_HANDOFF_MALFORMED_JOB"));
        auto result=job.reference
            ?Owner::measure_gold_worker(*job.reference)
            :Owner::measure_render_worker(*job.render,job.column);
        if(!result)return result;
        if(result.value()->column!=job.column
           ||result.value()->generation_token!=job.captured_epoch)
            return R::failure(error("M21_HANDOFF_RESULT_NOT_CAPTURED_OWNER"));
        return result;
    }

    [[nodiscard]] static bool publish_on_owner(
        const Controllers& product,const Fanout& fanout,
        const AuditionSourceSelector& selector,
        const GoldSelectionViewModel& gold,Inbox& inbox,
        const Job& job,const Snapshot& actual)
    {
        if(QThread::currentThread()!=product.session.thread()
           ||QThread::currentThread()!=selector.thread()
           ||QThread::currentThread()!=gold.thread()
           ||!job.structurally_valid()
           ||actual.column!=job.column
           ||actual.generation_token!=job.captured_epoch)
            return false;
        if(job.reference)
            return inbox.accept_gold(product,fanout,selector,gold,
                                     *job.reference,actual);
        return inbox.accept_render(product,fanout,selector,
                                   *job.render,actual);
    }

private:
    [[nodiscard]] static core::Error error(const char* reason) {
        return core::Error{core::ErrorCode::InvalidState,reason};
    }
};
} // namespace rgsml::app::internal
