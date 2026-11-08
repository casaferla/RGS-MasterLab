#pragma once
// M22 genuine Product M17 full-master preview admission. Retains the
// same exact verified M18 owner, immutable PCM and absolute output frames.
// Does not run a new renderer, SRC, limiter, dither or user-device handoff.
#include "m22_verified_master_preview_parity.hpp"
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <QThread>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace rgsml::app::internal {

class M22ProductPreviewAdmission final {
public:
    using Binding=M18ProductCompleteBinding;
    using Controllers=Binding::Controllers;
    using Fanout=Binding::Fanout;
    using Stage=TerminalCandidateGate::StagedCandidate;

    struct Capture final {
        Fanout::OwnerSnapshot owner;
        std::shared_ptr<const Stage> full_master;
        core::FrameRange exact_window;
        // Memory is borrowed from full_master. The shared owner above
        // keeps it alive even after the Product revokes this permission.
        audio::AudioBufferView crop;
    };

    [[nodiscard]] static core::Result<Capture> capture(
        const Controllers& product,const Fanout& fanout,
        std::shared_ptr<const Stage> fullMaster,
        core::FrameRange absoluteOutputWindow)
    {
        using R=core::Result<Capture>;
        if(!Binding::installed(product)
           ||!Binding::Upstream::owner_thread_current(product.upstream)
           ||QThread::currentThread()!=product.session.thread())
            return R::failure(fail("M22_PRODUCT_PREVIEW_REQUIRES_QT_OWNER"));
        const auto owner=fanout.current_owner_snapshot();
        if(!owner||owner->epoch==0||!owner->identity.valid())
            return R::failure(fail("M22_PRODUCT_PREVIEW_MISSING_OWNER_EPOCH"));
        auto cropped=m22_verified_master_preview_crop(
            product,fanout,fullMaster,absoluteOutputWindow);
        if(!cropped)return R::failure(*cropped.error());
        const auto after=fanout.current_owner_snapshot();
        if(!after||after->epoch!=owner->epoch
           ||after->identity!=owner->identity
           ||cropped.value()->absolute_start_frame()
                !=absoluteOutputWindow.begin()
           ||cropped.value()->absolute_end_frame()
                !=absoluteOutputWindow.end())
            return R::failure(fail("M22_PRODUCT_PREVIEW_EPOCH_OR_WINDOW_CHANGED"));
        return R::success(Capture{
            *owner,std::move(fullMaster),absoluteOutputWindow,
            *cropped.value()});
    }

    // A retained binary64 view is not a playback or publication license.
    // The Product must re-admit the exact unchanged verified M17 owner
    // before any queued use of an earlier output-rate window.
    [[nodiscard]] static bool still_current(
        const Controllers& product,const Fanout& fanout,
        const Capture& capture)
    {
        if(!Binding::installed(product)
           ||!Binding::Upstream::owner_thread_current(product.upstream)
           ||QThread::currentThread()!=product.session.thread()
           ||!capture.full_master||capture.owner.epoch==0)
            return false;
        const auto now=fanout.current_owner_snapshot();
        if(!now||now->epoch!=capture.owner.epoch
           ||now->identity!=capture.owner.identity
           ||product.candidates.staged()!=capture.full_master)
            return false;
        const auto verified=m22_verified_master_preview_crop(
            product,fanout,capture.full_master,capture.exact_window);
        if(!verified)return false;
        if(verified.value()->absolute_start_frame()
                !=capture.crop.absolute_start_frame()
           ||verified.value()->absolute_end_frame()
                !=capture.crop.absolute_end_frame()
           ||verified.value()->frame_count()!=capture.crop.frame_count()
           ||verified.value()->timebase().frame_domain_id()
                !=capture.crop.timebase().frame_domain_id()
           ||verified.value()->format()!=capture.crop.format())
            return false;
        // Retained preview may only alias the same immutable verified
        // double buffers. A detached/replaced/misaddressed view is NOT
        // proof even when its displayed duration and rate still match.
        for(std::size_t ch=0;
            ch<verified.value()->format().channel_count();++ch) {
            const auto expected=verified.value()->channel(ch);
            const auto retained=capture.crop.channel(ch);
            if(!expected||!retained
               ||expected.value()->data()!=retained.value()->data()
               ||expected.value()->size()!=retained.value()->size())
                return false;
        }
        return true;
    }

private:
    [[nodiscard]] static core::Error fail(const char* reason) {
        return {core::ErrorCode::InvalidState,reason};
    }
};
} // namespace rgsml::app::internal
