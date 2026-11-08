#include "internal/m23_alpha_diagnostic_report.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <QtTest/QTest>
#include <array>
#include <string>

namespace rgsml::tests {
namespace {
using namespace rgsml::app::internal;

class M23AlphaDiagnosticReportTest final : public QObject {
    Q_OBJECT
private slots:
    void capturesActualStatusCodesAndRefusesSecretErrorStrings();
    void rejectsSuccessfulAbsentAndEmptyStatuses();
    void reportedFailureNeverClaimsPhysicalOrReleaseGate();
    void diagnosticEvidenceRemainsUnknownUntilProven();
};

void M23AlphaDiagnosticReportTest::
capturesActualStatusCodesAndRefusesSecretErrorStrings()
{
    constexpr auto secret="sensitive/path/user/secret-provider-token";
    const auto actualExport=rgsml::core::Status::failure(
        rgsml::core::Error{rgsml::core::ErrorCode::IoFailure,
            "raw provider failed at "+std::string{secret},
            {rgsml::core::ErrorDetail{"credential",secret}}});
    const auto actualProject=rgsml::core::Status::failure(
        rgsml::core::Error{rgsml::core::ErrorCode::MalformedAudioContainer,
            "malformed project at "+std::string{secret}});
    const std::array observations{
        M23FailedStatus{M23Operation::EXPORT,M23Stage::REOPEN,&actualExport},
        M23FailedStatus{M23Operation::PROJECT_OPEN,
                        M23Stage::PERSISTENCE,&actualProject}};
    const auto result=AlphaDiagnosticReport::from_failed_statuses(
        observations,M23SafetyEvidence{});
    QVERIFY(result);
    QCOMPARE(result.value()->findings().size(),std::size_t{2});
    QCOMPARE(result.value()->findings()[0].canonical_code,
             rgsml::core::ErrorCode::IoFailure);
    QCOMPARE(result.value()->findings()[1].canonical_code,
             rgsml::core::ErrorCode::MalformedAudioContainer);
    const auto summary=result.value()->sanitized_summary();
    QVERIFY(summary.find("io_failure")!=std::string::npos);
    QVERIFY(summary.find("malformed_audio_container")!=std::string::npos);
    QVERIFY(summary.find(secret)==std::string::npos);
    QVERIFY(summary.find("credential")==std::string::npos);
    QVERIFY(summary.find("provider")==std::string::npos);
    QVERIFY(!result.value()->authorizes_release());
    QVERIFY(!result.value()->m23_i_p_m_passed());
}

void M23AlphaDiagnosticReportTest::
rejectsSuccessfulAbsentAndEmptyStatuses()
{
    const auto ok=rgsml::core::Status::success();
    const auto failed=rgsml::core::Status::failure(
        rgsml::core::Error{rgsml::core::ErrorCode::AccessDenied,
                            "not authorized"});
    const std::array empty{M23FailedStatus{
        M23Operation::EXPORT,M23Stage::PREFLIGHT,&failed}};
    const std::array mixed{M23FailedStatus{
        M23Operation::EXPORT,M23Stage::PREFLIGHT,&failed},
        M23FailedStatus{M23Operation::EXPORT,M23Stage::COMMIT,&ok}};
    const std::array nullStatus{M23FailedStatus{
        M23Operation::SOURCE_BIND,M23Stage::PREFLIGHT,nullptr}};
    QVERIFY(!AlphaDiagnosticReport::from_failed_statuses(
        std::span<const M23FailedStatus>{},M23SafetyEvidence{}));
    QVERIFY(!AlphaDiagnosticReport::from_failed_statuses(
        mixed,M23SafetyEvidence{}));
    QVERIFY(!AlphaDiagnosticReport::from_failed_statuses(
        nullStatus,M23SafetyEvidence{}));
    QVERIFY(AlphaDiagnosticReport::from_failed_statuses(
        empty,M23SafetyEvidence{}));
}

void M23AlphaDiagnosticReportTest::
reportedFailureNeverClaimsPhysicalOrReleaseGate()
{
    const auto error=rgsml::core::Status::failure(
        rgsml::core::Error{rgsml::core::ErrorCode::InvalidState,
                           "destination exists"});
    const std::array source{M23FailedStatus{
        M23Operation::EXPORT,M23Stage::COMMIT,&error}};
    M23SafetyEvidence evidence{};
    evidence.source=M23Evidence::VERIFIED_UNCHANGED;
    evidence.gold=M23Evidence::VERIFIED_UNCHANGED;
    evidence.preexisting_destination=M23Evidence::VERIFIED_UNCHANGED;
    evidence.live_project=M23Evidence::VERIFIED_UNCHANGED;
    evidence.publication=M23Publication::NO_AUTHORITATIVE_OUTPUT;
    evidence.candidate_cleanup_verified=true;
    const auto report=AlphaDiagnosticReport::from_failed_statuses(
        source,evidence);
    QVERIFY(report);
    QVERIFY(report.value()->safe_failure_evidence_complete());
    QVERIFY(!report.value()->authorizes_release());
    QVERIFY(!report.value()->m23_i_p_m_passed());
    QVERIFY(!report.value()->physical_gate_m_passed());
    QCOMPARE(report.value()->evidence().gold,M23Evidence::VERIFIED_UNCHANGED);
}

void M23AlphaDiagnosticReportTest::
diagnosticEvidenceRemainsUnknownUntilProven()
{
    const auto failed=rgsml::core::Status::failure(
        rgsml::core::Error{rgsml::core::ErrorCode::TruncatedAudioData,
                           "late partial read"});
    const std::array source{M23FailedStatus{
        M23Operation::EXPORT,M23Stage::VALIDATE,&failed}};
    auto result=AlphaDiagnosticReport::from_failed_statuses(
        source,M23SafetyEvidence{});
    QVERIFY(result);
    QVERIFY(!result.value()->safe_failure_evidence_complete());
    QVERIFY(!result.value()->authorizes_release());

    M23SafetyEvidence contradicted{};
    contradicted.source=M23Evidence::VERIFIED_CHANGED;
    contradicted.gold=M23Evidence::VERIFIED_UNCHANGED;
    contradicted.preexisting_destination=M23Evidence::VERIFIED_UNCHANGED;
    contradicted.live_project=M23Evidence::VERIFIED_UNCHANGED;
    contradicted.publication=M23Publication::AUTHORITATIVE_OUTPUT_PRESENT;
    contradicted.candidate_cleanup_verified=true;
    const auto unsafe=AlphaDiagnosticReport::from_failed_statuses(
        source,contradicted);
    QVERIFY(unsafe);
    QVERIFY(!unsafe.value()->safe_failure_evidence_complete());
    QVERIFY(!unsafe.value()->m23_i_p_m_passed());
}

} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::M23AlphaDiagnosticReportTest)
#include "test_m23_alpha_diagnostic_report.moc"
