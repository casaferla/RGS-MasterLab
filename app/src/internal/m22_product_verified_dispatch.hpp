#pragma once
// L1-M22 verified Product execution dispatch, exact current M17 stage.
// FULL uses the preexisting M18 user-authorized offline WAV job.
// PREVIEW borrows the already-verified full M17 master with no second DSP.
// No alternative limiter/renderer/encoder or synthetic Phase-6 gate.
#include "m22_product_preview_admission.hpp"
#include "m18_private_product_export_action.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/resource_reference.hpp>
#include <QThread>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace rgsml::app::internal {
class M22ProductVerifiedDispatch final {
public:
    using Binding=M18ProductCompleteBinding;
    using Controllers=Binding::Controllers;
    using Fanout=Binding::Fanout;
    using Stage=TerminalCandidateGate::StagedCandidate;
    using Full=M18PrivateProductExportAction;
    using Preview=M22ProductPreviewAdmission;

    [[nodiscard]] static core::Status begin_full(
        const Controllers& product,const Fanout& fanout,Full& exporter,
        const std::shared_ptr<const Stage>& expectedVerifiedMaster,
        core::ResourceReference destination,audio::WavSampleFormat format,
        std::optional<std::uint64_t> seed,double ceilingDbTp,
        bool confirmedByUser)
    {
        if(QThread::currentThread()!=product.session.thread()
           ||!expectedVerifiedMaster
           ||product.candidates.staged()!=expectedVerifiedMaster
           ||!Binding::can_offer_verified_master(product,fanout)
           ||!exporter.can_begin_verified_export())
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M22_FULL_REQUIRES_EXACT_CURRENT_PRODUCT_VERIFIED_MASTER"});
        // The existing M18 action alone owns user consent, mutation
        // epoch, worker, transactional WAV, hash and final ACK.
        return exporter.begin(std::move(destination),format,seed,
                              ceilingDbTp,confirmedByUser);
    }

    [[nodiscard]] static core::Result<Preview::Capture> begin_preview(
        const Controllers& product,const Fanout& fanout,
        std::shared_ptr<const Stage> expectedVerifiedMaster,
        core::FrameRange absoluteOutputWindow)
    {
        return Preview::capture(product,fanout,
                                std::move(expectedVerifiedMaster),
                                absoluteOutputWindow);
    }
};
} // namespace rgsml::app::internal
