#include "internal/m23_alpha_diagnostic_report.hpp"
#include "internal/m21_private_single_flight.hpp"
#include <atomic>
#include <memory>
#include <thread>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <QtTest/QTest>
#include <array>
#include <string>
#include <limits>

namespace rgsml::tests {
namespace {
using namespace rgsml::app::internal;


namespace m21 = rgsml::app::m21_private;

// Pure private test fixture: it conveys NO authentic Product owner proof
// and is never exported to QML or a Phase-6 feature registry.
inline m21::Snapshot private_m21_fixture(
    m21::Column column=m21::Column::prepared,
    std::uint64_t generation=1)
{
    m21::Snapshot s;
    s.column=column;
    s.generation_token=generation;
    s.resource_identity="fixture:independent-evidence";
    s.analysis_scope_label="Full Track";
    s.scope_frames=48000;
    s.sample_rate_hz=48000;
    s.lifecycle=m21::Lifecycle::current;
    s.metrics[3]={{},m21::Quality::missing,"M16 not qualified"};
    return s;
}

class M23AlphaDiagnosticReportTest final : public QObject {
    Q_OBJECT
private slots:
    void capturesActualStatusCodesAndRefusesSecretErrorStrings();
    void rejectsSuccessfulAbsentAndEmptyStatuses();
    void reportedFailureNeverClaimsPhysicalOrReleaseGate();
    void diagnosticEvidenceRemainsUnknownUntilProven();
    void m21AsyncSingleFlightIsBoundedAndOwnerOnly();
    void m21AsyncDestructorCannotTouchDeadOwner();
    void m21CompletionRejectsCrossColumnAndStaleGeneration();
    void m21AnalysisIdentityKeyIsUnambiguous();
    void m21WorkerExceptionCannotPublish();
    void m21RenderAttemptAcceptsReplacementInsideSameEpoch();
    void m21RenderAttemptDeduplicatesSamePhysicalOwner();
    void m21PublicationRejectsRelabeledColumnAndUnknownRole();
    void m21PublicationPreservesIndependentGoldScope();
    void m21TonalRejectsOrphanPairProof();
    void m21PublicationRejectsContradictoryScalarStates();
    void m21PublicationRejectsScopeOverflowAndBadRate();
    void m21PublicationStaysStaleAfterMalformedResult();
    void m21TonalRejectsNonFiniteShape();
    void m21TonalRejectsUnsupportedNumericBand();
    void m21TonalRejectsMissingProofAndBadResolution();
    void m21TonalRejectsDuplicateIds();
    void m21TonalAcceptsUnsortedCertifiedBands();
    void m21PsdRejectsBadPowerAndMissingValue();
    void m21PsdRejectsOutOfBandAndNonFinite();
    void m21PsdRejectsDuplicateAndDescending();
    void m21PsdAcceptsZeroAndNyquist();


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

void M23AlphaDiagnosticReportTest::
m21AsyncSingleFlightIsBoundedAndOwnerOnly()
{
    M21PrivateSingleFlight queue;
    auto gate=std::make_shared<std::atomic<bool>>(false);
    auto entered=std::make_shared<std::atomic<bool>>(false);
    QVERIFY(queue.start(19,[gate,entered]() {
        entered->store(true,std::memory_order_release);
        while(!gate->load(std::memory_order_acquire))
            std::this_thread::yield();
        rgsml::app::m21_private::Snapshot s;
        s.column=rgsml::app::m21_private::Column::prepared;
        s.generation_token=19;
        return std::optional<rgsml::app::m21_private::Snapshot>{std::move(s)};
    }));
    QTRY_VERIFY_WITH_TIMEOUT(entered->load(std::memory_order_acquire),2000);
    QVERIFY(queue.busy());
    QVERIFY(!queue.finished());
    QVERIFY(!queue.take_finished());
    QVERIFY(!queue.start(20,[] {
        return std::optional<rgsml::app::m21_private::Snapshot>{};
    }));
    std::atomic<bool> foreignAdmitted{false};
    std::thread foreign([&] {
        foreignAdmitted.store(queue.start(21,[] {
            return std::optional<rgsml::app::m21_private::Snapshot>{};
        }),std::memory_order_release);
    });
    foreign.join();
    QVERIFY(!foreignAdmitted.load(std::memory_order_acquire));
    gate->store(true,std::memory_order_release);
    QTRY_VERIFY_WITH_TIMEOUT(queue.finished(),2500);
    const auto result=queue.take_finished();
    QVERIFY(result);
    QCOMPARE(result->request_id,std::uint64_t{19});
    QVERIFY(result->result);
    QCOMPARE(result->result->generation_token,std::uint64_t{19});
    QVERIFY(!queue.busy());
    QVERIFY(!queue.take_finished());
    QVERIFY(queue.start(22,[] {
        return std::optional<rgsml::app::m21_private::Snapshot>{};
    }));
    QTRY_VERIFY_WITH_TIMEOUT(queue.finished(),2500);
    const auto empty=queue.take_finished();
    QVERIFY(empty);
    QCOMPARE(empty->request_id,std::uint64_t{22});
    QVERIFY(!empty->result);
}

void M23AlphaDiagnosticReportTest::
m21AsyncDestructorCannotTouchDeadOwner()
{
    auto entered=std::make_shared<std::atomic<bool>>(false);
    auto release=std::make_shared<std::atomic<bool>>(false);
    auto done=std::make_shared<std::atomic<bool>>(false);
    {
        M21PrivateSingleFlight queue;
        QVERIFY(queue.start(44,[entered,release,done]() {
            entered->store(true,std::memory_order_release);
            while(!release->load(std::memory_order_acquire))
                std::this_thread::yield();
            done->store(true,std::memory_order_release);
            return std::optional<rgsml::app::m21_private::Snapshot>{};
        }));
        QTRY_VERIFY_WITH_TIMEOUT(entered->load(std::memory_order_acquire),2000);
    } // No worker thread may retain the destroyed queue/Qt owner.
    release->store(true,std::memory_order_release);
    QTRY_VERIFY_WITH_TIMEOUT(done->load(std::memory_order_acquire),2500);
}

void M23AlphaDiagnosticReportTest::
m21CompletionRejectsCrossColumnAndStaleGeneration()
{
    using F=M21PrivateSingleFlight;
    using Column=rgsml::app::m21_private::Column;
    using Lifecycle=rgsml::app::m21_private::Lifecycle;
    F::Completion received;
    received.request_id=42;
    rgsml::app::m21_private::Snapshot genuine;
    genuine.column=Column::prepared;
    genuine.generation_token=88;
    genuine.lifecycle=Lifecycle::current;
    received.result=genuine;
    QVERIFY(F::matches_expected(received,42,Column::prepared,88));
    QVERIFY(!F::matches_expected(received,41,Column::prepared,88));
    QVERIFY(!F::matches_expected(received,42,Column::gold,88));
    QVERIFY(!F::matches_expected(received,42,Column::processed,88));
    QVERIFY(!F::matches_expected(received,42,Column::prepared,87));
    QVERIFY(!F::matches_expected(received,42,Column::prepared,0));
    QVERIFY(!F::matches_expected(received,0,Column::prepared,88));
    received.result->lifecycle=Lifecycle::stale;
    QVERIFY(!F::matches_expected(received,42,Column::prepared,88));
    received.result->lifecycle=Lifecycle::invalid;
    QVERIFY(!F::matches_expected(received,42,Column::prepared,88));
    received.result->lifecycle=Lifecycle::current;
    received.result.reset();
    QVERIFY(!F::matches_expected(received,42,Column::prepared,88));
}

void M23AlphaDiagnosticReportTest::
m21AnalysisIdentityKeyIsUnambiguous()
{
    std::string left,right;
    M21PrivateSingleFlight::append_identity_field(left,"source|file");
    M21PrivateSingleFlight::append_identity_field(left,"candidate");
    M21PrivateSingleFlight::append_identity_field(right,"source");
    M21PrivateSingleFlight::append_identity_field(right,"file|candidate");
    QVERIFY(left!=right);
    std::string one,two;
    M21PrivateSingleFlight::append_identity_field(one,"");
    M21PrivateSingleFlight::append_identity_field(one,":|");
    M21PrivateSingleFlight::append_identity_field(two,":|");
    M21PrivateSingleFlight::append_identity_field(two,"");
    QVERIFY(one!=two);
}

void M23AlphaDiagnosticReportTest::
m21WorkerExceptionCannotPublish()
{
    M21PrivateSingleFlight worker;
    QVERIFY(worker.start(113,[]() -> std::optional<
        rgsml::app::m21_private::Snapshot> {
        throw 1;
    }));
    QTRY_VERIFY_WITH_TIMEOUT(worker.finished(),2500);
    const auto completed=worker.take_finished();
    QVERIFY(completed);
    QCOMPARE(completed->request_id,std::uint64_t{113});
    QVERIFY(!completed->result);
    QVERIFY(!M21PrivateSingleFlight::matches_expected(
        *completed,113,rgsml::app::m21_private::Column::prepared,1));
}

void M23AlphaDiagnosticReportTest::
m21PsdAcceptsZeroAndNyquist()
{
    auto s=private_m21_fixture();
    s.average_spectrum={{0.,{0.,m21::Quality::present,{}}},
                        {100.,{1.e-9,m21::Quality::present,{}}},
                        {24000.,{0.,m21::Quality::present,{}}}};
    QVERIFY(m21::valid_private_spectrum(s));
    m21::PrivatePublicationGuard guard;
    QVERIFY(guard.expect(m21::Column::prepared,1));
    QVERIFY(guard.publish(s));
    const auto view=guard.view();
    QVERIFY(view.columns[0]);
    QCOMPARE(view.columns[0]->average_spectrum.size(),std::size_t{3});
    QCOMPARE(view.columns[0]->average_spectrum.back().hz,24000.);
}

void M23AlphaDiagnosticReportTest::
m21PsdRejectsDuplicateAndDescending()
{
    auto s=private_m21_fixture();
    s.average_spectrum={{100.,{1.,m21::Quality::present,{}}},
                        {100.,{2.,m21::Quality::present,{}}}};
    QVERIFY(!m21::valid_private_spectrum(s));
    s.average_spectrum[1].hz=99.;
    QVERIFY(!m21::valid_private_spectrum(s));
    s.average_spectrum[1].hz=101.;
    QVERIFY(m21::valid_private_spectrum(s));
}

void M23AlphaDiagnosticReportTest::
m21PsdRejectsOutOfBandAndNonFinite()
{
    auto s=private_m21_fixture();
    s.average_spectrum={{24000.01,{0.,m21::Quality::present,{}}}};
    QVERIFY(!m21::valid_private_spectrum(s));
    s.average_spectrum[0].hz=-1.;
    QVERIFY(!m21::valid_private_spectrum(s));
    s.average_spectrum[0].hz=std::numeric_limits<double>::infinity();
    QVERIFY(!m21::valid_private_spectrum(s));
    s.average_spectrum[0].hz=std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!m21::valid_private_spectrum(s));
}

void M23AlphaDiagnosticReportTest::
m21PsdRejectsBadPowerAndMissingValue()
{
    auto s=private_m21_fixture();
    s.average_spectrum={{100.,{-1.e-6,m21::Quality::present,{}}}};
    QVERIFY(!m21::valid_private_spectrum(s));
    s.average_spectrum[0].power_density.unrounded=
        std::numeric_limits<double>::quiet_NaN();
    QVERIFY(!m21::valid_private_spectrum(s));
    s.average_spectrum[0].power_density={0.,m21::Quality::missing,"bad zero"};
    QVERIFY(!m21::valid_private_spectrum(s));
    s.average_spectrum[0].power_density={{},m21::Quality::missing,"no value"};
    QVERIFY(m21::valid_private_spectrum(s));
}

void M23AlphaDiagnosticReportTest::
m21TonalAcceptsUnsortedCertifiedBands()
{
    auto s=private_m21_fixture();
    s.profile_denominator=12;
    s.common_comparison_support_id="pair-evidence";
    s.tonal_profile={{205,true,{3.,m21::Quality::present,{}}},
                     {100,false,{{},m21::Quality::missing,"no support"}},
                     {202,true,{4.,m21::Quality::present,{}}}};
    QVERIFY(m21::valid_private_tonal_profile(s));
    m21::PrivatePublicationGuard guard;
    QVERIFY(guard.expect(m21::Column::prepared,1));
    QVERIFY(guard.publish(s));
    const auto view=guard.view();
    QVERIFY(view.columns[0]);
    QCOMPARE(view.columns[0]->tonal_profile[1].band_id,100U);
    QVERIFY(!view.columns[0]->tonal_profile[1].normalized_shape_db.unrounded);
}

void M23AlphaDiagnosticReportTest::
m21TonalRejectsDuplicateIds()
{
    auto s=private_m21_fixture();
    s.profile_denominator=12;
    s.common_comparison_support_id="pair-evidence";
    s.tonal_profile={{100,true,{1.,m21::Quality::present,{}}},
                     {200,true,{2.,m21::Quality::present,{}}},
                     {100,true,{3.,m21::Quality::present,{}}}};
    QVERIFY(!m21::valid_private_tonal_profile(s));
    s.tonal_profile[2].band_id=201;
    QVERIFY(m21::valid_private_tonal_profile(s));
}

void M23AlphaDiagnosticReportTest::
m21TonalRejectsMissingProofAndBadResolution()
{
    auto s=private_m21_fixture();
    s.tonal_profile={{100,true,{1.,m21::Quality::present,{}}}};
    QVERIFY(!m21::valid_private_tonal_profile(s));
    s.profile_denominator=12;
    QVERIFY(!m21::valid_private_tonal_profile(s));
    s.common_comparison_support_id="external-pair-proof";
    QVERIFY(m21::valid_private_tonal_profile(s));
    s.profile_denominator=7;
    QVERIFY(!m21::valid_private_tonal_profile(s));
    s.profile_denominator=3;
    QVERIFY(m21::valid_private_tonal_profile(s));
}

void M23AlphaDiagnosticReportTest::
m21TonalRejectsUnsupportedNumericBand()
{
    auto s=private_m21_fixture();
    s.profile_denominator=12;
    s.common_comparison_support_id="external-pair-proof";
    s.tonal_profile={{100,false,{0.,m21::Quality::present,{}}}};
    QVERIFY(!m21::valid_private_tonal_profile(s));
    s.tonal_profile[0].normalized_shape_db={{},m21::Quality::missing,
                                            "outside shared bandwidth"};
    QVERIFY(m21::valid_private_tonal_profile(s));
    s.tonal_profile[0].in_common_support=true;
    s.tonal_profile[0].normalized_shape_db={0.,m21::Quality::present,{}};
    QVERIFY(m21::valid_private_tonal_profile(s));
}

void M23AlphaDiagnosticReportTest::
m21TonalRejectsNonFiniteShape()
{
    auto s=private_m21_fixture();
    s.profile_denominator=3;
    s.common_comparison_support_id="external-pair-proof";
    s.tonal_profile={{100,true,{
       std::numeric_limits<double>::quiet_NaN(),m21::Quality::present,{}}}};
    QVERIFY(!m21::valid_private_tonal_profile(s));
    s.tonal_profile[0].normalized_shape_db.unrounded=
        std::numeric_limits<double>::infinity();
    QVERIFY(!m21::valid_private_tonal_profile(s));
    s.tonal_profile[0].normalized_shape_db={0.,m21::Quality::missing,"false zero"};
    QVERIFY(!m21::valid_private_tonal_profile(s));
    s.tonal_profile[0].normalized_shape_db={{},m21::Quality::missing,"no result"};
    QVERIFY(m21::valid_private_tonal_profile(s));
}

void M23AlphaDiagnosticReportTest::
m21PublicationStaysStaleAfterMalformedResult()
{
    m21::PrivatePublicationGuard guard;
    auto s=private_m21_fixture(m21::Column::prepared,1);
    s.metrics[0]={-18.0,m21::Quality::present,{}};
    QVERIFY(guard.expect(m21::Column::prepared,1));
    QVERIFY(guard.publish(s));
    QVERIFY(guard.expect(m21::Column::prepared,2));
    s.generation_token=2;
    s.metrics[0].unrounded=-11.;
    s.average_spectrum={{100.,{1.,m21::Quality::present,{}}},
                        {100.,{2.,m21::Quality::present,{}}}};
    QVERIFY(!guard.publish(s));
    const auto rejected=guard.view();
    QVERIFY(rejected.columns[0]);
    QVERIFY(rejected.columns[0]->lifecycle==m21::Lifecycle::stale);
    QVERIFY(!rejected.prepared_to_gold[0].unrounded);
    s.average_spectrum[1].hz=101.;
    QVERIFY(guard.publish(s));
    const auto restored=guard.view();
    QVERIFY(restored.columns[0]);
    QVERIFY(restored.columns[0]->lifecycle==m21::Lifecycle::current);
    QCOMPARE(*restored.columns[0]->metrics[0].unrounded,-11.);
}

void M23AlphaDiagnosticReportTest::
m21PublicationRejectsScopeOverflowAndBadRate()
{
    m21::PrivatePublicationGuard guard;
    auto s=private_m21_fixture();
    QVERIFY(guard.expect(m21::Column::prepared,1));
    s.absolute_scope_begin=
        (std::numeric_limits<std::uint64_t>::max)()-100;
    QVERIFY(!guard.publish(s));
    s.absolute_scope_begin=0;
    s.analysis_scope_label="Unknown";
    QVERIFY(!guard.publish(s));
    s.analysis_scope_label="Full Track";
    s.sample_rate_hz=32000;
    QVERIFY(!guard.publish(s));
    s.sample_rate_hz=48000;
    s.scope_frames=0;
    QVERIFY(!guard.publish(s));
    s.scope_frames=48000;
    QVERIFY(guard.publish(s));
}

void M23AlphaDiagnosticReportTest::
m21PublicationRejectsContradictoryScalarStates()
{
    m21::PrivatePublicationGuard guard;
    auto s=private_m21_fixture();
    QVERIFY(guard.expect(m21::Column::prepared,1));
    s.metrics[2]={0.0,m21::Quality::missing,"No peak"};
    QVERIFY(!guard.publish(s));
    s.metrics[2]={{},m21::Quality::present,{}};
    QVERIFY(!guard.publish(s));
    s.metrics[2]={std::numeric_limits<double>::infinity(),
                   m21::Quality::present,{}};
    QVERIFY(!guard.publish(s));
    s.metrics[2]={0.,m21::Quality::present,{}};
    QVERIFY(guard.publish(s));
    QVERIFY(!guard.publish(s));
}

void M23AlphaDiagnosticReportTest::
m21TonalRejectsOrphanPairProof()
{
    auto s=private_m21_fixture();
    QVERIFY(m21::valid_private_tonal_profile(s));
    s.common_comparison_support_id="claim-without-resolution";
    QVERIFY(!m21::valid_private_tonal_profile(s));
    s.profile_denominator=12;
    QVERIFY(m21::valid_private_tonal_profile(s));
    s.profile_denominator=0;
    s.common_comparison_support_id.clear();
    QVERIFY(m21::valid_private_tonal_profile(s));
}

void M23AlphaDiagnosticReportTest::
m21PublicationPreservesIndependentGoldScope()
{
    auto p=private_m21_fixture(m21::Column::prepared,1);
    auto gold=private_m21_fixture(m21::Column::gold,5);
    p.metrics[0]={-20.,m21::Quality::present,{}};
    gold.metrics[0]={-14.,m21::Quality::present,{}};
    gold.analysis_scope_label="Region";
    gold.absolute_scope_begin=700001;
    gold.scope_frames=88200;
    gold.sample_rate_hz=44100;
    m21::PrivatePublicationGuard guard;
    QVERIFY(guard.expect(m21::Column::prepared,1));
    QVERIFY(guard.expect(m21::Column::gold,5));
    QVERIFY(guard.publish(p));
    QVERIFY(guard.publish(gold));
    const auto current=guard.view();
    QVERIFY(current.independent_scopes);
    QCOMPARE(*current.prepared_to_gold[0].unrounded,6.);
    QCOMPARE(current.columns[1]->absolute_scope_begin,std::uint64_t{700001});
    QVERIFY(guard.expect(m21::Column::gold,6));
    const auto stale=guard.view();
    QVERIFY(stale.columns[0]->lifecycle==m21::Lifecycle::current);
    QVERIFY(stale.columns[1]->lifecycle==m21::Lifecycle::stale);
    QVERIFY(stale.prepared_to_gold[0].quality==m21::Quality::stale);
    QVERIFY(!stale.prepared_to_gold[0].unrounded);
}

void M23AlphaDiagnosticReportTest::
m21PublicationRejectsRelabeledColumnAndUnknownRole()
{
    m21::PrivatePublicationGuard guard;
    auto s=private_m21_fixture(m21::Column::prepared,1);
    QVERIFY(guard.expect(m21::Column::prepared,1));
    s.column=m21::Column::gold;
    QVERIFY(!guard.publish(s));
    s.column=static_cast<m21::Column>(255);
    QVERIFY(!guard.expect(s.column,1));
    QVERIFY(!guard.publish(s));
    s.column=m21::Column::prepared;
    QVERIFY(guard.publish(s));
    const auto readback=guard.view();
    QVERIFY(readback.columns[0]);
    QVERIFY(readback.columns[0]->column==m21::Column::prepared);
    QVERIFY(!readback.columns[1]);
}

void M23AlphaDiagnosticReportTest::
m21RenderAttemptDeduplicatesSamePhysicalOwner()
{
    M21PrivateRenderAttempt attempt;
    const auto pcm=std::make_shared<const int>(11);
    QVERIFY(!attempt.matches("epoch:21|source:A",pcm));
    attempt.remember("epoch:21|source:A",pcm);
    QVERIFY(attempt.matches("epoch:21|source:A",pcm));
    const std::shared_ptr<const int> alias=pcm;
    QVERIFY(attempt.matches("epoch:21|source:A",alias));
    QVERIFY(!attempt.matches("epoch:22|source:A",pcm));
}

void M23AlphaDiagnosticReportTest::
m21RenderAttemptAcceptsReplacementInsideSameEpoch()
{
    M21PrivateRenderAttempt attempt;
    const auto original=std::make_shared<const int>(17);
    const auto replacement=std::make_shared<const int>(17);
    attempt.remember("source:A|epoch:8|prepared",original);
    QVERIFY(attempt.matches("source:A|epoch:8|prepared",original));
    QVERIFY(!attempt.matches("source:A|epoch:8|prepared",replacement));
    attempt.remember("source:A|epoch:8|prepared",replacement);
    QVERIFY(attempt.matches("source:A|epoch:8|prepared",replacement));
    QVERIFY(!attempt.matches("source:A|epoch:8|prepared",original));
}

} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::M23AlphaDiagnosticReportTest)
#include "test_m23_alpha_diagnostic_report.moc"
