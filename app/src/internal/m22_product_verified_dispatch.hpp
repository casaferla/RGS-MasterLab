#pragma once
// L1-M22 verified Product execution dispatch, exact current M17 stage.
// FULL uses the preexisting M18 user-authorized offline WAV job.
// PREVIEW borrows the already-verified full M17 master with no second DSP.
// No alternative limiter/renderer/encoder or synthetic Phase-6 gate.
#include "m22_product_preview_admission.hpp"
#include "m22_product_chain_epoch_capture_candidate.hpp"
#include "../../../render/src/internal/m22_real_verified_output_plan_candidate.hpp"
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
        // Admission for FULL must be as strict as PREVIEW: validate
        // actual SOURCE resource identity/current Product epoch and native
        // complete Manual -> original Q0 -> checked M17/M16 output PCM.
        // A synthetic metadata-only output plan is NOT an export ticket.
        const auto captured=M22ProductChainEpochCaptureCandidate::capture(
            product,fanout,expectedVerifiedMaster);
        if(!captured)
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M22_FULL_REQUIRES_REAL_SOURCE_CHAIN_AND_EPOCH"});
        const auto frames=captured.value()->
            immutable_chain_witness.actual_master_frames;
        if(frames<=0)
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M22_FULL_REQUIRES_TRUE_OUTPUT_EOS"});
        m22_private::RealFullOutputPlanFailure plan_reason{};
        const auto genuine=m22_private::observe_real_full_output_plan_candidate(
            expectedVerifiedMaster->rendered,m22_private::Mode::FULL,
            m22_private::Window{0,static_cast<std::uint64_t>(frames)},
            plan_reason);
        if(!genuine
           ||genuine->native.manual_source_hz!=captured.value()->
               immutable_chain_witness.manual_source_hz
           ||genuine->native.final_master_hz!=captured.value()->
               immutable_chain_witness.final_master_hz
           ||genuine->native.complete_source_frames!=captured.value()->
               immutable_chain_witness.complete_source_frames
           ||genuine->native.actual_master_frames!=frames
           ||genuine->native.channels!=captured.value()->
               immutable_chain_witness.channels
           ||genuine->native.manual_chain_revision!=captured.value()->
               immutable_chain_witness.manual_chain_revision
           ||genuine->native.terminal_chain_revision!=captured.value()->
               immutable_chain_witness.terminal_chain_revision
           ||genuine->native.used_original_q0!=captured.value()->
               immutable_chain_witness.used_original_q0
           ||genuine->native.frozen_original_q0_sha256!=captured.value()->
               immutable_chain_witness.frozen_original_q0_sha256
           ||genuine->output.absolute_output_first!=0
           ||genuine->output.required_source_true_eos!=
               static_cast<std::uint64_t>(
                   genuine->native.complete_source_frames)
           ||genuine->output.required_output_true_eos!=
               static_cast<std::uint64_t>(frames)
           ||genuine->output.output_frame_count()!=
               static_cast<std::uint64_t>(frames)
           ||!M22ProductChainEpochCaptureCandidate::still_current(
               product,fanout,*captured.value()))
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M22_FULL_REQUIRES_TRUE_NATIVE_VERIFIED_OUTPUT_PLAN"});
        // The existing M18 action alone owns explicit user consent,
        // mutation epoch, worker, transactional WAV, hash and final ACK.
        // M22 cannot mint separate release authority or re-encode PCM.
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
