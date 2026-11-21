#pragma once
// L3 non-publishing candidate batch, numerical/profiling development seam.
// This is NOT the frozen M10 48-coordinate/27-feature solver, tie-order,
// topology planner, Product acceptance, or verification M13. The Qt owner
// must supply a set of individually authenticated immutable jobs and an
// independent REFERENCE M21 snapshot. No QObject travels to this worker.
// Cancellation cannot interrupt a synchronous DSP block, but it prevents
// the next candidate and never publishes partial PCM.
#include "l3_offline_private_metric_probe.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch {
class PrivateBatchProgress final {
public:
    // Qt may read these atomics; do not expose worker Result/PCM to Qt.
    void request_cancel() noexcept {
        cancel_requested_.store(true,std::memory_order_release);
    }
    [[nodiscard]] bool cancellation_requested() const noexcept {
        return cancel_requested_.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::uint32_t attempted() const noexcept {
        return attempted_.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::uint32_t completed() const noexcept {
        return completed_.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::uint32_t failed() const noexcept {
        return failed_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool finished() const noexcept {
        return finished_.load(std::memory_order_acquire);
    }
private:
    friend class PrivateScalarCandidateBatch;
    std::atomic<bool> cancel_requested_{false};
    std::atomic<std::uint32_t> attempted_{0};
    std::atomic<std::uint32_t> completed_{0};
    std::atomic<std::uint32_t> failed_{0};
    std::atomic<bool> finished_{false};
};
enum class PrivateBatchEnd : std::uint8_t {
    all_attempted,
    cancelled
};
struct PrivateBatchBest final {
    std::string typed_dsp_fingerprint;
    std::size_t ordinal{};
    double measured_scalar_error{};
};
struct PrivateBatchReport final {
    PrivateBatchEnd end{PrivateBatchEnd::cancelled};
    std::uint32_t attempted{};
    std::uint32_t completed{};
    std::uint32_t failed{};
    std::optional<PrivateBatchBest> best;
    // Explicitly not authorization for a Product/QML projection.
    [[nodiscard]] constexpr bool authorizes_product_publication()
        const noexcept {return false;}
};
class PrivateScalarCandidateBatch final {
public:
    static constexpr std::size_t kMaxAttempts=800;

    // Invoke on a numerical worker with independent shared Progress.
    // A Product Qt owner must re-authenticate ALL source/reference/chain
    // owners after this returns, even when end==all_attempted.
    [[nodiscard]] static core::Result<PrivateBatchReport> run(
        const std::vector<OfflineProductTrialJob>& jobs,
        const m21_private::Snapshot& authenticated_gold,
        std::span<const Objective> objectives,
        const std::shared_ptr<PrivateBatchProgress>& progress)
    {
        using R=core::Result<PrivateBatchReport>;
        const auto reject=[](const char* why) {
            return R::failure(core::Error{
                core::ErrorCode::InvalidArgument,why});
        };
        if(!progress||progress->finished()
           ||progress->attempted()!=0
           ||progress->completed()!=0||progress->failed()!=0
           ||jobs.empty()||jobs.size()>kMaxAttempts
           ||objectives.empty()||objectives.size()>9)
            return reject("L3_PRIVATE_BATCH_INVALID_CONFIGURATION");
        const auto& origin=jobs.front();
        if(!origin.stamp().populated()||!origin.prepared_owner())
            return reject("L3_PRIVATE_BATCH_MISSING_PRODUCT_ORIGIN");
        // The M21 Product inbox uses this exact generation+resource
        // identity. The raw provider/locator remain separate in the
        // stamp, so a different REFERENCE cannot be smuggled in even
        // when both references happen to have current numerical cells.
        const auto expected_gold_id=
            "gold-file-generation:"+
            std::to_string(origin.stamp().m18_owner_epoch)+"|"+
            origin.stamp().reference_provider_id+"|"+
            origin.stamp().reference_locator;
        if(authenticated_gold.column!=m21_private::Column::gold
           ||authenticated_gold.lifecycle!=m21_private::Lifecycle::current
           ||authenticated_gold.generation_token!=
                origin.stamp().m18_owner_epoch
           ||authenticated_gold.resource_identity!=expected_gold_id
           ||authenticated_gold.analysis_scope_label!="Full Track")
            return reject("L3_PRIVATE_BATCH_UNAUTHENTICATED_REFERENCE");
        // Explicit reference-side objective suitability is knowable before
        // DSP dispatch. Reject invalid/duplicate/Gold-unavailable objectives
        // now: they must NOT consume solver render/analyzer budget.
        std::array<bool,9> selected{};
        for(const auto& objective:objectives) {
            const auto index=static_cast<std::size_t>(objective.metric);
            if(index>=selected.size()||selected[index]
               ||!std::isfinite(objective.tolerance)
               ||!std::isfinite(objective.weight)
               ||objective.tolerance<=0.0||objective.weight<=0.0)
                return reject("L3_PRIVATE_BATCH_INVALID_OBJECTIVE");
            selected[index]=true;
            const auto& cell=authenticated_gold.metrics[index];
            if(cell.quality!=m21_private::Quality::present
               ||!cell.unrounded||!std::isfinite(*cell.unrounded))
                return reject("L3_PRIVATE_BATCH_REFERENCE_OBJECTIVE_UNSUPPORTED");
        }
        std::unordered_set<std::string> fingerprints;
        fingerprints.reserve(jobs.size());
        for(const auto& job:jobs) {
            if(!job.stamp().populated()||job.stamp()!=origin.stamp()
               ||job.prepared_owner()!=origin.prepared_owner()
               ||job.chain().context()!=origin.chain().context()
               ||job.chain().revision()!=origin.chain().revision()
               ||job.baseline_bindings()!=origin.baseline_bindings()
               ||job.ticket_identity().empty()
               ||!fingerprints.insert(job.ticket_identity()).second)
                return reject("L3_PRIVATE_BATCH_MIXED_OR_DUPLICATE_OWNERS");
            const auto a=job.chain().instances();
            const auto b=origin.chain().instances();
            if(a.size()!=b.size()||
               !std::equal(a.begin(),a.end(),b.begin()))
                return reject("L3_PRIVATE_BATCH_MIXED_DSP_GRAPH");
        }
        // All preflight rejections above consume ZERO attempts and cannot
        // mutate the progress object. From here only actual DSP+M19/M20
        // attempted evaluations increment the ledger.
        PrivateBatchReport report;
        report.end=PrivateBatchEnd::all_attempted;
        for(std::size_t ordinal=0;ordinal<jobs.size();++ordinal) {
            if(progress->cancellation_requested()) {
                report.end=PrivateBatchEnd::cancelled;
                break;
            }
            // Mark the attempted evaluation BEFORE DSP dispatch, even if
            // rendering or measuring fails or cancellation arrives mid-render.
            ++report.attempted;
            progress->attempted_.store(report.attempted,
                                      std::memory_order_release);
            const auto eval=jobs[ordinal].evaluate();
            if(!eval) {
                ++report.failed;
                progress->failed_.store(report.failed,
                                        std::memory_order_release);
                continue;
            }
            const auto score=probe_unpublished_candidate(
                *eval.value(),authenticated_gold,objectives);
            if(!score) {
                ++report.failed;
                progress->failed_.store(report.failed,
                                        std::memory_order_release);
                continue;
            }
            // If Qt cancelled while C01 was processing, never retain the
            // in-flight result as an acceptably completed best.
            if(progress->cancellation_requested()) {
                report.end=PrivateBatchEnd::cancelled;
                break;
            }
            ++report.completed;
            progress->completed_.store(report.completed,
                                       std::memory_order_release);
            if(!report.best ||
               score.value()->normalized_weighted_squared_error
                   <report.best->measured_scalar_error)
                report.best=PrivateBatchBest{
                    jobs[ordinal].ticket_identity(),ordinal,
                    score.value()->normalized_weighted_squared_error};
        }
        if(progress->cancellation_requested())
            report.end=PrivateBatchEnd::cancelled;
        progress->finished_.store(true,std::memory_order_release);
        return R::success(std::move(report));
    }
};
} // namespace rgsml::app::l3_automatch
