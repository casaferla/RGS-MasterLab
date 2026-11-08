// M22 owner-thread Product bridge, built on the existing M18 authority.
// This adapter only CAPTURES the immutable real M17 result. It does not
// install a Project rate, authorize Phase-6, export, or publish PROCESSED.
#pragma once
#include "m18_product_complete_binding.hpp"
#include "../../../render/src/internal/m22_full_chain_witness_candidate.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <QThread>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace rgsml::app::internal {

class M22ProductChainEpochCaptureCandidate final {
public:
    using Binding=M18ProductCompleteBinding;
    using Controllers=Binding::Controllers;
    using Fanout=Binding::Fanout;
    using Stage=TerminalCandidateGate::StagedCandidate;

    struct Capture final {
        Fanout::OwnerSnapshot epoch;
        std::shared_ptr<const Stage> exact_m17_owner;
        m22_private::FullChainWitness immutable_chain_witness;
        Capture()=delete;
        Capture(Fanout::OwnerSnapshot accepted,
                std::shared_ptr<const Stage> m17,
                m22_private::FullChainWitness witness)
            :epoch(std::move(accepted)),exact_m17_owner(std::move(m17)),
             immutable_chain_witness(std::move(witness)) {}
        [[nodiscard]] bool authorizes_release() const noexcept {return false;}
    };

    [[nodiscard]] static core::Result<Capture> capture(
        const Controllers& product,const Fanout& fanout,
        std::shared_ptr<const Stage> expected)
    {
        using R=core::Result<Capture>;
        if(QThread::currentThread()!=product.session.thread()
           ||!Binding::Upstream::owner_thread_current(product.upstream)
           ||!product.candidates.m18_owner_thread_current()
           ||!Binding::installed(product)
           ||!Binding::can_offer_verified_master(product,fanout)
           ||!expected
           ||product.candidates.staged()!=expected)
            return R::failure(fail("M22_PRODUCT_CHAIN_REQUIRES_EXACT_CURRENT_STAGE"));
        const auto owner=fanout.current_owner_snapshot();
        if(!owner||owner->epoch==0||!owner->identity.valid())
            return R::failure(fail("M22_PRODUCT_CHAIN_MISSING_EPOCH"));
        m22_private::FullChainWitnessFailure reason{};
        const auto witness=m22_private::observe_real_complete_chain_candidate(
            expected->rendered,reason);
        if(!witness)
            return R::failure(fail("M22_PRODUCT_CHAIN_INVALID_REAL_FULL_PROGRAMME"));
        const auto& id=owner->identity;
        if(id.master_rate_hz!=static_cast<std::uint32_t>(
                witness->final_master_hz)
           ||id.chain_revision!=witness->manual_chain_revision
           ||id.master_rate_revision!=product.upstream.rate.rate_revision()
           ||witness->complete_source_frames
               !=product.upstream.source.frame_count()
           ||witness->terminal_chain_revision
               !=product.upstream.terminal.chain().revision())
            return R::failure(fail("M22_PRODUCT_CHAIN_SOURCE_RATE_OR_REVISION_STALE"));
        // Recheck after reading the actual native worker result. No worker
        // may call this Qt-only API; no acknowledgement or publishing occurs.
        const auto after=fanout.current_owner_snapshot();
        if(!after||after->epoch!=owner->epoch
           ||after->identity!=owner->identity
           ||product.candidates.staged()!=expected)
            return R::failure(fail("M22_PRODUCT_CHAIN_CHANGED_DURING_CAPTURE"));
        return R::success(Capture{
            *owner,std::move(expected),std::move(*witness)});
    }

    [[nodiscard]] static bool still_current(
        const Controllers& product,const Fanout& fanout,
        const Capture& saved)
    {
        if(QThread::currentThread()!=product.session.thread()
           ||!Binding::Upstream::owner_thread_current(product.upstream)
           ||!saved.exact_m17_owner
           ||!Binding::can_offer_verified_master(product,fanout)
           ||product.candidates.staged()!=saved.exact_m17_owner)
            return false;
        const auto now=fanout.current_owner_snapshot();
        if(!now||now->epoch!=saved.epoch.epoch
           ||now->identity!=saved.epoch.identity)
            return false;
        m22_private::FullChainWitnessFailure reason{};
        const auto witness=m22_private::observe_real_complete_chain_candidate(
            saved.exact_m17_owner->rendered,reason);
        return witness.has_value()
            &&witness->manual_source_hz
                ==saved.immutable_chain_witness.manual_source_hz
            &&witness->final_master_hz
                ==saved.immutable_chain_witness.final_master_hz
            &&witness->complete_source_frames
                ==saved.immutable_chain_witness.complete_source_frames
            &&witness->actual_master_frames
                ==saved.immutable_chain_witness.actual_master_frames
            &&witness->channels==saved.immutable_chain_witness.channels
            &&witness->manual_chain_revision
                ==saved.immutable_chain_witness.manual_chain_revision
            &&witness->terminal_chain_revision
                ==saved.immutable_chain_witness.terminal_chain_revision
            &&witness->used_original_q0
                ==saved.immutable_chain_witness.used_original_q0
            &&witness->frozen_original_q0_sha256
                ==saved.immutable_chain_witness.frozen_original_q0_sha256
            &&product.upstream.source.frame_count()
                ==witness->complete_source_frames
            &&static_cast<std::uint32_t>(witness->final_master_hz)
                ==now->identity.master_rate_hz;
    }

private:
    [[nodiscard]] static core::Error fail(const char* code) {
        return core::Error{core::ErrorCode::InvalidState,code};
    }
};
} // namespace rgsml::app::internal
