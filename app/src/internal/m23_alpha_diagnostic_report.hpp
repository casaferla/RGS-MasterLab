// L1-M23 nonrelease immutable diagnostic projection of ACTUAL core errors.
// No second error taxonomy, no error.message(), ErrorDetail, file paths,
// Qt objects, resources, pointers, provider secrets or cloud telemetry.
// The reporter cannot grant I/P/M or a Product export ticket.
#pragma once
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <array>
#include <cstdint>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rgsml::app::internal {

enum class M23Operation : std::uint8_t {
    SOURCE_BIND, GOLD_BIND, PROJECT_OPEN, PROJECT_SAVE,
    MANUAL_RENDER, PREVIEW, FULL, EXPORT
};
enum class M23Stage : std::uint8_t {
    PREFLIGHT, WRITE, CLOSE, REOPEN, VALIDATE,
    ENCODED_SHA, DECODED_VERIFY, COMMIT, DESTINATION_READBACK,
    CLEANUP, PERSISTENCE
};
enum class M23Evidence : std::uint8_t {
    UNKNOWN, VERIFIED_UNCHANGED, VERIFIED_CHANGED
};
enum class M23Publication : std::uint8_t {
    UNKNOWN, NO_AUTHORITATIVE_OUTPUT, AUTHORITATIVE_OUTPUT_PRESENT
};
struct M23SafetyEvidence final {
    M23Evidence source{M23Evidence::UNKNOWN};
    M23Evidence gold{M23Evidence::UNKNOWN};
    M23Evidence preexisting_destination{M23Evidence::UNKNOWN};
    M23Evidence live_project{M23Evidence::UNKNOWN};
    M23Publication publication{M23Publication::UNKNOWN};
    bool candidate_cleanup_verified{false};
    bool encoded_check_completed{false};
    bool decoded_check_completed{false};
};

struct M23FailedStatus final {
    M23Operation operation;
    M23Stage stage;
    const rgsml::core::Status* status{nullptr};
};

struct M23DiagnosticFinding final {
    M23Operation operation;
    M23Stage stage;
    rgsml::core::ErrorCode canonical_code;
    [[nodiscard]] std::string_view canonical_code_token() const noexcept {
        return rgsml::core::error_code_token(canonical_code);
    }
};

class AlphaDiagnosticReport final {
public:
    // Only *failed* original Status objects can produce failure entries.
    // No user-supplied textual error, secrets or context are retained.
    [[nodiscard]] static rgsml::core::Result<AlphaDiagnosticReport>
    from_failed_statuses(
        std::span<const M23FailedStatus> source,
        M23SafetyEvidence evidence)
    {
        using R=rgsml::core::Result<AlphaDiagnosticReport>;
        if(source.empty())
            return R::failure(rgsml::core::Error{
                rgsml::core::ErrorCode::InvalidArgument,
                "M23_DIAGNOSTIC_NEEDS_REAL_FAILED_STATUS"});
        try {
            std::vector<M23DiagnosticFinding> findings;
            findings.reserve(source.size());
            for(const auto& observed:source) {
                if(!observed.status ||!*observed.status
                   ||observed.status->error()==nullptr)
                    return R::failure(rgsml::core::Error{
                        rgsml::core::ErrorCode::InvalidState,
                        "M23_DIAGNOSTIC_REFUSES_SUCCESS_OR_UNKNOWN_STATUS"});
                findings.push_back(M23DiagnosticFinding{
                    observed.operation,observed.stage,
                    observed.status->error()->code()});
            }
            return R::success(AlphaDiagnosticReport{
                std::move(findings),evidence});
        }catch(const std::bad_alloc&) {
            return R::failure(rgsml::core::Error{
                rgsml::core::ErrorCode::IoFailure,
                "M23_DIAGNOSTIC_ALLOCATION_FAILED"});
        }
    }

    [[nodiscard]] const std::vector<M23DiagnosticFinding>&
    findings() const noexcept {return findings_;}
    [[nodiscard]] M23SafetyEvidence evidence() const noexcept {return evidence_;}

    // Purely local bounded non-secret summary. It does not serialize
    // user-provided Error.message()/details() or a resource locator.
    [[nodiscard]] std::string sanitized_summary() const {
        std::string result="M23 nonrelease diagnostic:";
        for(const auto& finding:findings_) {
            result.push_back(' ');
            const auto token=finding.canonical_code_token();
            result.append(token.data(),token.size());
        }
        return result;
    }

    // Even with a real canonical error and verified cleanup, M23 I+P+M
    // and the physical Windows PO step have NOT been signed off here.
    [[nodiscard]] bool safe_failure_evidence_complete() const noexcept {
        return evidence_.source==M23Evidence::VERIFIED_UNCHANGED
            &&evidence_.gold==M23Evidence::VERIFIED_UNCHANGED
            &&evidence_.preexisting_destination==
                M23Evidence::VERIFIED_UNCHANGED
            &&evidence_.live_project==M23Evidence::VERIFIED_UNCHANGED
            &&evidence_.publication==
                M23Publication::NO_AUTHORITATIVE_OUTPUT
            &&evidence_.candidate_cleanup_verified;
    }
    [[nodiscard]] bool authorizes_release() const noexcept {return false;}
    [[nodiscard]] bool m23_i_p_m_passed() const noexcept {return false;}
    [[nodiscard]] bool physical_gate_m_passed() const noexcept {return false;}
private:
    AlphaDiagnosticReport(std::vector<M23DiagnosticFinding> findings,
        M23SafetyEvidence evidence)
        :findings_(std::move(findings)),evidence_(evidence){}
    std::vector<M23DiagnosticFinding> findings_;
    M23SafetyEvidence evidence_;
};

} // namespace rgsml::app::internal
