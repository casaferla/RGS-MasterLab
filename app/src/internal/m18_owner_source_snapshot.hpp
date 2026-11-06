#pragma once
// M18 PREPRODUCTION ONLY. Qt Product-owner capture of the actual current
// SOURCE provenance, not a user-supplied ResourceReference pretending to
// describe the current input. This is NOT a worker-thread API.
#include "../source_selection_view_model.hpp"
#include <rgsml/core/resource_reference.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/error.hpp>
#include <QThread>
#include <new>

namespace rgsml::app::internal {

[[nodiscard]] inline core::Result<core::ResourceReference>
capture_m18_current_source_on_product_owner(
    const SourceSelectionViewModel& owner)
{
    using R=core::Result<core::ResourceReference>;
    // Check affinity BEFORE reading any QObject-backed Source state.
    if(QThread::currentThread()!=owner.thread())
        return R::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_SOURCE_CAPTURE_REQUIRES_PRODUCT_QT_OWNER_THREAD"});
    const auto* source=owner.source_resource();
    if(!source ||!source->reference().permissions().can_read())
        return R::failure(core::Error{
            core::ErrorCode::AccessDenied,
            "M18_SOURCE_CAPTURE_REQUIRES_ACTUAL_READABLE_SOURCE"});
    // Value-copy provider+opaque locator+permissions at the same owner
    // epoch as the verified M17 stage. Nothing here opens files, transfers
    // QObject references to workers or authorizes a destination commit.
    try {
        return R::success(source->reference());
    } catch(const std::bad_alloc&) {
        // An OOM while copying opaque provider/locator metadata must not
        // mint a worker request with a missing or guessed SOURCE owner.
        return R::failure(core::Error{
            core::ErrorCode::IoFailure,
            "M18_PRODUCT_SOURCE_REFERENCE_SNAPSHOT_ALLOCATION_FAILED"});
    }
}
} // namespace rgsml::app::internal
