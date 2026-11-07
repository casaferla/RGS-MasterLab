#include "audition_source_selector.hpp"
#include "playback_transport_view_model.hpp"
#include "terminal_candidate_gate.hpp"
#include "terminal_measured_device_audition_ticket.hpp"
#include "m17_prepare_live_eta_policy.hpp"
#include "terminal_playback_frame_map.hpp"
#include "internal/terminal_src_frame_map_provenance.hpp"
#include "terminal_audio_session_gate.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_private_render_controller.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>
#include "terminal_private_product_session.hpp"
#include "internal/m18_product_epoch_ticket.hpp"
#include "internal/m18_checked_stage_worker_bridge.hpp"
#include "internal/m18_private_async_export_handoff.hpp"
#include "internal/m18_private_async_export_session.hpp"
#include "m18_private_export_progress_view_model.hpp"
#include <rgsml/platform/windows/windows_resource_writer.hpp>
#include "terminal_monitor_view_model.hpp"
#include "terminal_product_activation_coordinator.hpp"
#include "master_output_rate_state.hpp"
#include "source_selection_view_model.hpp"
#include "mastering_preview_controller.hpp"
#include "terminal_limiter_view_model.hpp"
#include "../platform/fake_playback_service.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/monitoring_pcm_candidate.hpp>
#include <rgsml/audio/monitoring_frame_map.hpp>
#include <rgsml/audio/monitoring_rate_memory_advisor.hpp>
#include <rgsml/audio/wav_writer.hpp>
#include <rgsml/core/resource_io.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/core/uuid.hpp>
#include <QTest>
#include <QSignalSpy>
#include <QDebug>
#include <memory>
#include <new>
#include <stdexcept>
#include <array>
#include <bit>
#include <cstddef>
#include <span>
#include <string>
#include <optional>
#include <utility>
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>
#include <thread>
#include <filesystem>
#include <numeric>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>

namespace rgsml::tests {

namespace {
// Short, real IEEE_F64 WAV serialization into an exact in-memory RESOURCE
// port. No disk permissions, Windows AudioSink or unapproved M18 delivery.
struct MatrixWavBytes {std::vector<std::byte> bytes;};
class MatrixWavSink final:public core::IResourceWriter {
public:
    explicit MatrixWavSink(std::shared_ptr<MatrixWavBytes> state)
        : state_(std::move(state)),reference_(
            *core::ResourceReference::create(
                "test.memory","m17-rate-grid",false,true,"M17 WAV test").value()) {}
    [[nodiscard]] const core::ResourceReference& reference() const noexcept override
    {return reference_;}
    [[nodiscard]] core::ResourceCapabilities capabilities() const noexcept override
    {return core::ResourceCapabilities::create(true,true,true,true);}
    [[nodiscard]] core::Result<std::uint64_t> position_bytes() const override
    {return core::Result<std::uint64_t>::success(position_);}
    [[nodiscard]] core::Result<std::size_t> write(
        std::span<const std::byte> bytes) override {
        if(closed_)return core::Result<std::size_t>::failure(
            core::Error{core::ErrorCode::InvalidState,"MATRIX_WAV_CLOSED"});
        const auto count=bytes.size();
        const auto end=position_+count;
        if(end>1024U*1024U)return core::Result<std::size_t>::failure(
            core::Error{core::ErrorCode::OutOfRange,"MATRIX_WAV_TOO_LARGE"});
        if(state_->bytes.size()<end)state_->bytes.resize(
            static_cast<std::size_t>(end));
        std::copy(bytes.begin(),bytes.end(),
                  state_->bytes.begin()+static_cast<std::ptrdiff_t>(position_));
        position_=end;
        return core::Result<std::size_t>::success(count);
    }
    [[nodiscard]] core::Status seek_bytes(std::uint64_t at) override
    {position_=at;return core::Status::success();}
    [[nodiscard]] core::Status resize_bytes(std::uint64_t size) override {
        if(size>1024U*1024U)return core::Status::failure(
            core::Error{core::ErrorCode::OutOfRange,"MATRIX_WAV_TOO_LARGE"});
        state_->bytes.resize(static_cast<std::size_t>(size));
        if(position_>size)position_=size;
        return core::Status::success();
    }
    [[nodiscard]] core::Status flush() override
    {return core::Status::success();}
    [[nodiscard]] core::Status close() override
    {closed_=true;return core::Status::success();}
private:
    std::shared_ptr<MatrixWavBytes> state_;
    core::ResourceReference reference_;
    std::uint64_t position_{0};
    bool closed_{false};
};
[[nodiscard]] std::uint32_t matrix_wav_u32(
    const std::vector<std::byte>& bytes,std::size_t offset)
{
    std::uint32_t value=0;
    for(std::size_t i=0;i<4;++i)
        value|=static_cast<std::uint32_t>(
            std::to_integer<unsigned char>(bytes[offset+i]))<<(8*i);
    return value;
}
} // namespace

class TerminalActivationIntegrationTest final:public QObject {
    Q_OBJECT
private slots:
    void acceptedQ0MasterFrameMapPreservesBothKernelFamilies();
    void audibleOnBypassRestoresExactManualSourceAndLoop();
    void finalMasterAt192kNeverBecomesDeviceRate48k();
    void sixSourceBySixTargetOfflineMasterAndDeviceRateMatrix();
    void sustainedCrossrateMasterDevice48kWithoutSecondRender();
    void privateMaster192kOn48kDeviceRemainsUninstalled();
    void staleM17TerminalJobCancelsWithoutPublishingPcm();
    void nonconformingPreviewRequiresSeparateExplicitConsent();
    void rejectedCeilingCannotAlterAuthority_data();
    void rejectedCeilingCannotAlterAuthority();
    void unchangedCeilingLeavesRevisionAndConsentUntouched();
    void m18DirectTerminalStateMutationRequiresProductEpoch();
    void m18TerminalDirectMutationUsesRealCommitLease();
    void m18DirectTerminalMutationRejectsReentrantEpochCallback();
    void m18TerminalViewModelUsesSingleDirectStateEpochAuthority();
    void verificationTrimCannotBeForgedFromCeilingOrRetainedAcrossOwners();
    void firstPrepareLiveEtaConvergesFromActualStagesWithoutHistory();
};


// First-run ETA contract: no completed Prepare, optional history, device
// calibration, audio asset, fake M16 frame denominator or extra M17 render
// is required. These are PROVISIONAL WHOLE-Prepare remaining time bounds.
void TerminalActivationIntegrationTest::
firstPrepareLiveEtaConvergesFromActualStagesWithoutHistory()
{
    using Phase=app::M17PrepareLivePhase;
    using Evidence=app::M17PrepareLiveEvidence;
    const auto forecast=[](Evidence x) {
        return app::m17_provisional_live_remaining(x);
    };
    const auto width=[](const app::M17PrepareLiveRemaining& x) {
        return x.high_ms-x.low_ms;
    };
    Evidence src{};
    src.phase=Phase::FINAL_SRC;
    src.phase_fraction_known=false;
    src.phase_elapsed_ms=3'000;
    QVERIFY(!forecast(src)); // no fabricated ETA with no measured progress
    src.phase_fraction_known=true;
    src.phase_fraction=0.05;
    const auto srcEarly=forecast(src);
    QVERIFY(srcEarly);
    QVERIFY(srcEarly->high_ms>srcEarly->low_ms);
    // Same observed SRC throughput as the fraction increases from 5 to
    // 90 percent: the unknown future stages remain broad, but the total
    // range contracts without EVER consulting three earlier Prepares.
    src.phase_fraction=0.90;
    src.phase_elapsed_ms=54'000;
    const auto srcLate=forecast(src);
    QVERIFY(srcLate);
    QVERIFY(width(*srcLate)<width(*srcEarly));

    Evidence corePhase{};
    corePhase.phase=Phase::M17_CORE;
    corePhase.phase_fraction_known=true;
    corePhase.phase_fraction=0.05;
    corePhase.phase_elapsed_ms=4'000;
    const auto coreEarly=forecast(corePhase);
    QVERIFY(coreEarly);
    corePhase.phase_fraction=0.90;
    corePhase.phase_elapsed_ms=72'000;
    const auto coreLate=forecast(corePhase);
    QVERIFY(coreLate);
    QVERIFY(width(*coreLate)<width(*coreEarly));
    QVERIFY(coreLate->high_ms<srcLate->high_ms);

    Evidence m16{};
    m16.phase=Phase::M16_PRIMARY;
    m16.phase_fraction_known=false; // M16 has no legitimate frame counter
    m16.phase_elapsed_ms=10'000;
    QVERIFY(!forecast(m16)); // cannot conjure prior CORE measurement
    m16.observed_m17_ms=80'000; // actual elapsed M17, NOT calibration
    const auto m16Early=forecast(m16);
    QVERIFY(m16Early);
    m16.phase_elapsed_ms=33'000;
    const auto m16Late=forecast(m16);
    QVERIFY(m16Late);
    QVERIFY(width(*m16Late)<width(*m16Early));
    m16.phase_elapsed_ms=90'000; // genuine unexpected slow verification
    const auto m16Overrun=forecast(m16);
    QVERIFY(m16Overrun);
    QVERIFY(m16Overrun->high_ms>m16Late->high_ms);

    Evidence trim{};
    trim.phase=Phase::VERIFICATION_TRIM;
    trim.phase_elapsed_ms=3'000;
    QVERIFY(!forecast(trim)); // no measured PRIMARY: still unknown
    trim.observed_primary_m16_ms=25'000;
    const auto trimEarly=forecast(trim);
    QVERIFY(trimEarly);
    QVERIFY(trimEarly->low_ms==0);
    QVERIFY(trimEarly->high_ms>=50'000);
    trim.phase_elapsed_ms=40'000;
    const auto trimOverrun=forecast(trim);
    QVERIFY(trimOverrun);
    QVERIFY(trimOverrun->high_ms>trimEarly->high_ms);

    Evidence secondary{};
    secondary.phase=Phase::M16_SECONDARY;
    secondary.observed_primary_m16_ms=25'000;
    secondary.phase_elapsed_ms=10'000;
    const auto secondEarly=forecast(secondary);
    QVERIFY(secondEarly);
    secondary.phase_elapsed_ms=22'000;
    const auto secondLate=forecast(secondary);
    QVERIFY(secondLate);
    QVERIFY(width(*secondLate)<width(*secondEarly));

    Evidence done{};
    done.phase=Phase::FINISHING;
    const auto finishing=forecast(done);
    QVERIFY(finishing);
    QCOMPARE(finishing->low_ms,std::int64_t{0});
    QCOMPARE(finishing->high_ms,std::int64_t{8'000});
    // The stage ETA is an observational scalar; it cannot change a
    // SOURCE, MASTER, Q0 kernel, M16 measurement or DSP authorization.
}

// FQ90 physical regression: a real original-Q0 SOURCE->MASTER result must
// be previewable even though the historic/provisional DEVICE SRC uses a
// DIFFERENT FIR SHA. Neither a made-up digest nor a frame-count mismatch
// may acquire playback authority. Source/MASTER coordinates use the same
// exact ceil half-open map, without replacing the Q0 render coefficients.
void TerminalActivationIntegrationTest::
acceptedQ0MasterFrameMapPreservesBothKernelFamilies()
{
    for(const auto pair:std::array<std::pair<std::int64_t,std::int64_t>,3>{{
        {44100,48000},{192000,48000},{48000,44100}}}) {
        auto fin=core::SampleRate::create(pair.first);
        auto fout=core::SampleRate::create(pair.second);
        auto n=core::FrameCount::create(513);
        QVERIFY(fin&&fout&&n);
        auto map=audio::PlaybackSampleRateAdapter::create({
            *fin.value(),*fout.value(),audio::ChannelLayout::STEREO_LR,
            *n.value()});
        QVERIFY(map);
        const auto gcd=std::gcd(pair.first,pair.second);
        const auto* original=audio::internal::phase11_frozen_original_fir_pin(
            static_cast<std::uint32_t>(pair.second/gcd),
            static_cast<std::uint32_t>(pair.first/gcd));
        QVERIFY(original);
        const auto total=map.value()->output_frame_count();
        // Authentic Phase-11 Q0 on the same L/M must NOT be compared
        // to the genuinely different playback-kernel SHA.
        QVERIFY(app::internal::accepted_terminal_src_frame_map_provenance(
            *fin.value(),*fout.value(),*n.value(),total,
            original->sha256,*map.value()));
        // All DEVICE ratios now have an independently frozen SHA from the
        // flat 16-FIR package. For the two legacy directions Playback and
        // Q0 remain different valid hashes; for other rates the DEVICE and
        // original MASTER use the exact same original-Q0 SHA.
        const bool historic=(pair.first==44100&&pair.second==48000)
            ||(pair.first==48000&&pair.second==44100);
        QVERIFY(!map.value()->kernel_sha256().empty());
        if(historic)
            QVERIFY(map.value()->kernel_sha256()!=std::string_view{original->sha256});
        else
            QCOMPARE(map.value()->kernel_sha256(),std::string_view{original->sha256});
        QVERIFY(app::internal::accepted_terminal_src_frame_map_provenance(
            *fin.value(),*fout.value(),*n.value(),total,
            map.value()->kernel_sha256(),*map.value()));
        QVERIFY(!app::internal::accepted_terminal_src_frame_map_provenance(
            *fin.value(),*fout.value(),*n.value(),total,
            "forged-missing-or-stale-kernel",*map.value()));
        const auto changedSource=core::FrameCount::create(n.value()->value()+1);
        const auto changedOutput=core::FrameCount::create(total.value()+1);
        QVERIFY(changedSource&&changedOutput);
        QVERIFY(!app::internal::accepted_terminal_src_frame_map_provenance(
            *fin.value(),*fout.value(),*changedSource.value(),total,
            original->sha256,*map.value()));
        QVERIFY(!app::internal::accepted_terminal_src_frame_map_provenance(
            *fin.value(),*fout.value(),*n.value(),*changedOutput.value(),
            original->sha256,*map.value()));
        // Nontrivial SOURCE boundaries always map with exact ceil.
        const auto p=map.value()->map_input_frame_to_output(
            core::FrameIndex{147});
        QVERIFY(p);
        const auto expected=(147*pair.second+pair.first-1)/pair.first;
        QCOMPARE(p.value()->value(),expected);
    }
}

// Adversarial parameter-input oracle: independent data rows run even if a
// different failure later aborts the full A/B integration scenario. No PCM,
// filename, or hardware certificate is created by these input tests.
void TerminalActivationIntegrationTest::
verificationTrimCannotBeForgedFromCeilingOrRetainedAcrossOwners()
{
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto id=dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(
        "7f000000-0000-4000-8000-000000000618").value());
    QVERIFY(id);
    auto state=app::TerminalLimiterState::create_default(
        *registry.value(),*id.value());
    QVERIFY(state);
    app::TerminalLimiterViewModel vm{state.value()};
    QCOMPARE(vm.verification_trim_status(),
        QStringLiteral("UNAVAILABLE · NOT VERIFIED"));
    // A user-set Ceiling, even at -12 dBTP, is NOT full M16 trim evidence.
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-12.0));
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.verification_trim_status(),
        QStringLiteral("UNAVAILABLE · NOT VERIFIED"));
    QSignalSpy changed{&vm,&app::TerminalLimiterViewModel::changed};
    // Values below are synthetic DATA FOR UI MAPPING only. Actual
    // production permission comes exclusively from an exact M17 owner.
    vm.setVerifiedTrimObservation(0.99,true,1.0,0.99);
    QCOMPARE(vm.verification_trim_status(),QStringLiteral("-0.09 dB · M16 CHECK"));
    QVERIFY(vm.verification_trim_details().contains(
        QStringLiteral("post-trim")));
    QVERIFY(vm.verification_trim_details().contains(
        QStringLiteral("Not a DEVICE playback meter")));
    QVERIFY(changed.count()>=1);
    vm.setVerifiedTrimObservation(1.0,false,0.0,0.0);
    QCOMPARE(vm.verification_trim_status(),QStringLiteral("0.00 dB · NO TRIM"));
    // Reject unphysical/nonfinite gains, unsupported >0.10 dB trim,
    // false "trim used" and never retain a previous successful label.
    vm.setVerifiedTrimObservation(0.5,true,1.0,0.5);
    QCOMPARE(vm.verification_trim_status(),
        QStringLiteral("UNAVAILABLE · NOT VERIFIED"));
    vm.setVerifiedTrimObservation(
        std::numeric_limits<double>::quiet_NaN(),true,1.0,1.0);
    QCOMPARE(vm.verification_trim_status(),
        QStringLiteral("UNAVAILABLE · NOT VERIFIED"));
    vm.setVerifiedTrimObservation(1.0,true,1.0,1.0);
    QCOMPARE(vm.verification_trim_status(),
        QStringLiteral("UNAVAILABLE · NOT VERIFIED"));
    vm.setVerifiedTrimObservation(0.99,true,1.0,0.99);
    QVERIFY(vm.verification_trim_status().contains(QStringLiteral("M16 CHECK")));
    vm.clearVerificationTrim(); // invalidated SOURCE or MASTER owner
    QCOMPARE(vm.verification_trim_status(),
        QStringLiteral("UNAVAILABLE · NOT VERIFIED"));
}

void TerminalActivationIntegrationTest::
m18DirectTerminalStateMutationRequiresProductEpoch()
{
    // A direct TerminalLimiterState writer must not bypass the ViewModel
    // admission while an M18 commit holds the immutable M17 owner.
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto id=dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(
        "7f000000-0000-4000-8000-0000000007a1").value());
    QVERIFY(id);
    auto state=app::TerminalLimiterState::create_default(
        *registry.value(),*id.value());
    QVERIFY(state);
    QVERIFY(state.value()->bypassed());
    const auto chainRevision=state.value()->chain().revision();
    const auto paramsRevision=state.value()->parameter_revision();
    std::size_t calls=0;
    state.value()->set_private_m18_mutation_admission(
        [&](const char* name)->core::Status {
            ++calls;
            if(!name)return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_TERMINAL_OPERATION_MISSING"});
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"M18_TERMINAL_COMMIT_BUSY"});
        });
    QVERIFY(state.value()->m18_mutation_admission_installed());
    const auto deniedCeiling=state.value()->set_ceiling_db_tp(-3.0);
    QVERIFY(!deniedCeiling);
    QCOMPARE(deniedCeiling.error()->code(),core::ErrorCode::InvalidState);
    const auto deniedBypass=state.value()->set_user_bypass(false);
    QVERIFY(!deniedBypass);
    QCOMPARE(deniedBypass.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(calls,std::size_t{2});
    QCOMPARE(state.value()->parameter_revision(),paramsRevision);
    QCOMPARE(state.value()->chain().revision(),chainRevision);
    QCOMPARE(state.value()->parameters().ceiling_db_tp(),-1.0);
    QVERIFY(state.value()->bypassed());
    // True no-ops do not manufacture a new Product generation.
    QVERIFY(state.value()->set_ceiling_db_tp(-1.0));
    QVERIFY(state.value()->set_user_bypass(true));
    QCOMPARE(calls,std::size_t{2});
    // A foreign worker is rejected BEFORE its callback can mutate state.
    bool deniedForeign=false;
    std::thread worker([&] {
        const auto outcome=state.value()->set_ceiling_db_tp(-4.0);
        deniedForeign=!outcome &&outcome.error()->code()
            ==core::ErrorCode::InvalidState;
    });
    worker.join();
    QVERIFY(deniedForeign);
    QCOMPARE(calls,std::size_t{2});
    state.value()->set_private_m18_mutation_admission(
        [](const char*)->core::Status {
            throw std::runtime_error("M18_TERMINAL_ADMISSION_THROWN");
        });
    QVERIFY(!state.value()->set_ceiling_db_tp(-5.0));
    QCOMPARE(state.value()->parameter_revision(),paramsRevision);
    app::TerminalLimiterViewModel vm{state.value()};
    QSignalSpy published{&vm,&app::TerminalLimiterViewModel::acceptedStateChanged};
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-6.0));
    // Direct state guard sees the same denied operation when the VM
    // callback is not installed; no accepted render signal is emitted.
    QVERIFY(!vm.commitDraft());
    QCOMPARE(vm.validation_message(),
        QStringLiteral("Limiter change blocked by active export"));
    QCOMPARE(published.count(),0);
    QCOMPARE(state.value()->parameter_revision(),paramsRevision);
    std::size_t ambiguousVmCalls=0;
    vm.set_private_m18_processing_mutation_admission([&] {
        ++ambiguousVmCalls;
        return true;
    });
    // Concurrently installed status callbacks are not "two approvals":
    // their product epochs might differ. Fail closed before calling either.
    QVERIFY(!vm.commitDraft());
    QCOMPARE(ambiguousVmCalls,std::size_t{0});
    QCOMPARE(state.value()->parameter_revision(),paramsRevision);
    vm.setBypass(false);
    QVERIFY(state.value()->bypassed());
    QCOMPARE(ambiguousVmCalls,std::size_t{0});
    QCOMPARE(published.count(),0);
    vm.set_private_m18_processing_mutation_admission({});
    // Every Qt-bound editor path, including draft-only changes, refuses a
    // foreign thread once the Product's terminal epoch is installed.
    QSignalSpy observedUi{&vm,&app::TerminalLimiterViewModel::changed};
    const auto beforeDraft=vm.draft_ceiling_db_tp();
    bool foreignDraftDenied=false,foreignCommitDenied=false;
    std::thread illegalEditor([&] {
        foreignDraftDenied=!vm.setDraftFieldValue(
            QStringLiteral("ceilingDbTp"),-4.0);
        foreignCommitDenied=!vm.commitDraft();
        vm.resetToDefault();
        vm.setBypass(false);
        vm.cancelDraft();
    });
    illegalEditor.join();
    QVERIFY(foreignDraftDenied);
    QVERIFY(foreignCommitDenied);
    QCOMPARE(vm.draft_ceiling_db_tp(),beforeDraft);
    QCOMPARE(observedUi.count(),0);
    QCOMPARE(published.count(),0);
    QCOMPARE(state.value()->parameter_revision(),paramsRevision);
    QVERIFY(state.value()->bypassed());
    // Legacy unguarded M17 functionality remains intact when the
    // preproduction gate is not installed.
    state.value()->set_private_m18_mutation_admission({});
    QVERIFY(state.value()->set_ceiling_db_tp(-3.0));
    QVERIFY(state.value()->set_user_bypass(false));
    QCOMPARE(state.value()->parameters().ceiling_db_tp(),-3.0);
    QCOMPARE(state.value()->parameter_revision(),paramsRevision+1);
    QVERIFY(!state.value()->bypassed());
}

void TerminalActivationIntegrationTest::
m18TerminalDirectMutationUsesRealCommitLease()
{
    using platform::windows::internal::M18ProductEpochTicketAuthority;
    using platform::windows::internal::M18ProductExportIdentity;
    const M18ProductExportIdentity id{41U,42U,43U,0U,48000U};
    M18ProductEpochTicketAuthority authority{600U,id};
    QVERIFY(authority.try_transition(600U,601U,id,true));
    const auto ticket=authority.capture(id);
    QVERIFY(ticket);
    auto lease=ticket.value()->commit_lease_factory()();
    QVERIFY(lease);
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto limiterId=dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(
        "7f000000-0000-4000-8000-0000000007a2").value());
    QVERIFY(limiterId);
    auto state=app::TerminalLimiterState::create_default(
        *registry.value(),*limiterId.value());
    QVERIFY(state);
    std::size_t attempted=0;
    state.value()->set_private_m18_mutation_admission(
        [&](const char*)->core::Status {
            ++attempted;
            return authority.try_transition(601U,602U,id,false);
        });
    const auto initialChain=state.value()->chain().revision();
    const auto initialParam=state.value()->parameter_revision();
    QVERIFY(!state.value()->set_user_bypass(false));
    QVERIFY(!state.value()->set_ceiling_db_tp(-3.0));
    QCOMPARE(attempted,std::size_t{2});
    QCOMPARE(state.value()->chain().revision(),initialChain);
    QCOMPARE(state.value()->parameter_revision(),initialParam);
    QVERIFY(state.value()->bypassed());
    QVERIFY((*lease.value())->revalidate_current());
    lease.value()->reset();
    // After physical commit is finished, exactly one authoritative edit
    // can advance the Product epoch and revoke the old M18 worker ticket.
    QVERIFY(state.value()->set_ceiling_db_tp(-3.0));
    QCOMPARE(attempted,std::size_t{3});
    QCOMPARE(state.value()->parameter_revision(),initialParam+1);
    QVERIFY(!ticket.value()->commit_lease_factory()());
    // A stale expected epoch cannot subsequently toggle BYPASS.
    QVERIFY(!state.value()->set_user_bypass(false));
    QVERIFY(state.value()->bypassed());
    QCOMPARE(attempted,std::size_t{4});
    state.value()->set_private_m18_mutation_admission({});
}

void TerminalActivationIntegrationTest::
m18DirectTerminalMutationRejectsReentrantEpochCallback()
{
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto id=dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(
        "7f000000-0000-4000-8000-0000000007a3").value());
    QVERIFY(id);
    auto state=app::TerminalLimiterState::create_default(
        *registry.value(),*id.value());
    QVERIFY(state);
    std::size_t calls=0;
    bool nestedDenied=false;
    const auto initialCeilingRevision=state.value()->parameter_revision();
    const auto initialChainRevision=state.value()->chain().revision();
    state.value()->set_private_m18_mutation_admission(
        [&](const char*)->core::Status {
            ++calls;
            if(calls==1) {
                // A malicious/synchronous Product callback must not
                // change Ceiling underneath the outer BYPASS admission.
                auto nested=state.value()->set_ceiling_db_tp(-6.0);
                nestedDenied=!nested &&nested.error()->code()
                    ==core::ErrorCode::InvalidState;
            }
            return core::Status::success();
        });
    QVERIFY(state.value()->set_user_bypass(false));
    QVERIFY(nestedDenied);
    QCOMPARE(calls,std::size_t{1});
    QVERIFY(!state.value()->bypassed());
    QCOMPARE(state.value()->parameter_revision(),initialCeilingRevision);
    QCOMPARE(state.value()->parameters().ceiling_db_tp(),-1.0);
    QVERIFY(state.value()->chain().revision()>initialChainRevision);
    // The guard has been released: a second genuine, non-reentrant
    // edit may be admitted normally instead of staying permanently busy.
    QVERIFY(state.value()->set_ceiling_db_tp(-4.0));
    QCOMPARE(calls,std::size_t{2});
    QCOMPARE(state.value()->parameter_revision(),initialCeilingRevision+1);
    QCOMPARE(state.value()->parameters().ceiling_db_tp(),-4.0);
    state.value()->set_private_m18_mutation_admission({});
}

void TerminalActivationIntegrationTest::
m18TerminalViewModelUsesSingleDirectStateEpochAuthority()
{
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto id=dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(
        "7f000000-0000-4000-8000-0000000007a4").value());
    QVERIFY(id);
    auto state=app::TerminalLimiterState::create_default(
        *registry.value(),*id.value());
    QVERIFY(state);
    app::TerminalLimiterViewModel vm{state.value()};
    std::size_t admitted=0;
    bool available=true;
    state.value()->set_private_m18_mutation_admission(
        [&](const char* operation)->core::Status {
            if(!operation ||!available)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,"M18_TERMINAL_EPOCH_BUSY"});
            ++admitted;
            return core::Status::success();
        });
    QSignalSpy accepted{&vm,&app::TerminalLimiterViewModel::acceptedStateChanged};
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-2.0));
    QVERIFY(vm.commitDraft());
    QCOMPARE(admitted,std::size_t{1});
    QCOMPARE(accepted.count(),1);
    QVERIFY(vm.commitDraft()); // no-op must not revoke Product epoch
    QCOMPARE(admitted,std::size_t{1});
    QCOMPARE(accepted.count(),1);
    vm.setBypass(false);
    QCOMPARE(admitted,std::size_t{2});
    QCOMPARE(accepted.count(),2);
    QVERIFY(!vm.bypass());
    // A denied real reset leaves both canonical M17 parameter and UI
    // accepted-signal sequence intact.
    available=false;
    vm.resetToDefault();
    QCOMPARE(vm.validation_message(),
        QStringLiteral("Limiter reset blocked by active export"));
    QCOMPARE(admitted,std::size_t{2});
    QCOMPARE(accepted.count(),2);
    QCOMPARE(state.value()->parameters().ceiling_db_tp(),-2.0);
    available=true;
    vm.resetToDefault();
    QVERIFY(vm.validation_message().isEmpty());
    QCOMPARE(admitted,std::size_t{3});
    QCOMPARE(accepted.count(),3);
    QCOMPARE(state.value()->parameters().ceiling_db_tp(),-1.0);
    state.value()->set_private_m18_mutation_admission({});
}

void TerminalActivationIntegrationTest::rejectedCeilingCannotAlterAuthority_data()
{
    QTest::addColumn<double>("invalidCeiling");
    QTest::newRow("below-minimum") << -12.01;
    QTest::newRow("above-zero") << 0.01;
    QTest::newRow("positive-infinity") << std::numeric_limits<double>::infinity();
    QTest::newRow("negative-infinity") << -std::numeric_limits<double>::infinity();
    QTest::newRow("quiet-nan") << std::numeric_limits<double>::quiet_NaN();
}

void TerminalActivationIntegrationTest::rejectedCeilingCannotAlterAuthority()
{
    QFETCH(double,invalidCeiling);
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto id=dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(
        "7f000000-0000-4000-8000-000000000617").value());
    QVERIFY(id);
    auto state=app::TerminalLimiterState::create_default(*registry.value(),*id.value());
    QVERIFY(state);
    app::TerminalLimiterViewModel vm{state.value()};
    QSignalSpy accepted{&vm,&app::TerminalLimiterViewModel::acceptedStateChanged};
    QSignalSpy unsafe{&vm,&app::TerminalLimiterViewModel::previewNonconformingRequested};
    QVERIFY(accepted.isValid()&&unsafe.isValid());
    const auto revision=state.value()->parameter_revision();
    qInfo().noquote()<<"M17_EDGE_CASE=INVALID_CEILING_NO_COMMIT";
    QVERIFY(!vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),invalidCeiling));
    QVERIFY(!vm.commitDraft());
    QCOMPARE(state.value()->parameter_revision(),revision);
    QCOMPARE(state.value()->parameters().ceiling_db_tp(),-1.0);
    QCOMPARE(accepted.count(),0);
    QCOMPARE(unsafe.count(),0);
    vm.cancelDraft();
    QVERIFY(vm.validation_message().isEmpty());
    QCOMPARE(state.value()->parameter_revision(),revision);
}

void TerminalActivationIntegrationTest::unchangedCeilingLeavesRevisionAndConsentUntouched()
{
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto id=dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(
        "7f000000-0000-4000-8000-000000000618").value());
    QVERIFY(id);
    auto state=app::TerminalLimiterState::create_default(*registry.value(),*id.value());
    QVERIFY(state);
    app::TerminalLimiterViewModel vm{state.value()};
    QSignalSpy accepted{&vm,&app::TerminalLimiterViewModel::acceptedStateChanged};
    QSignalSpy unverified{&vm,&app::TerminalLimiterViewModel::previewNonconformingRequested};
    QSignalSpy verified{&vm,&app::TerminalLimiterViewModel::previewRequested};
    QVERIFY(accepted.isValid()&&unverified.isValid()&&verified.isValid());
    qInfo().noquote()<<"M17_EDGE_CASE=NOOP_CEILING_RESET";
    vm.resetToDefault();
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-1.0));
    QVERIFY(vm.commitDraft());
    QCOMPARE(state.value()->parameter_revision(),std::uint64_t{0});
    QCOMPARE(accepted.count(),0);
    QVERIFY(state.value()->set_user_bypass(false));
    vm.setAudioStatus(QStringLiteral("READY · PREVIEW AVAILABLE"));
    vm.previewNonconforming(); // A verified result never grants unsafe consent.
    QCOMPARE(unverified.count(),0);
    vm.previewPrepared();
    QCOMPARE(verified.count(),1);
    vm.setAudioStatus(QStringLiteral("ACTIVE · AUDITION UNQUALIFIED"));
    vm.previewPrepared(); // No double-install by a second Preview click.
    QCOMPARE(verified.count(),1);
    vm.setAudioStatus(QStringLiteral("NON-CONFORMING · -11.76 dBTP"));
    vm.previewPrepared(); // A failed Ceiling never becomes verified.
    QCOMPARE(verified.count(),1);
    vm.previewNonconforming();
    QCOMPARE(unverified.count(),1);
    QVERIFY(state.value()->set_user_bypass(true));
    vm.previewNonconforming(); // Bypass cannot trigger unsafe audio.
    QCOMPARE(unverified.count(),1);
    QCOMPARE(accepted.count(),0);
}


void TerminalActivationIntegrationTest::
nonconformingPreviewRequiresSeparateExplicitConsent()
{
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto instance=dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(
        "7f000000-0000-4000-8000-000000000517").value());
    QVERIFY(instance);
    auto state=app::TerminalLimiterState::create_default(
        *registry.value(),*instance.value());
    QVERIFY(state);
    app::TerminalLimiterViewModel vm{state.value()};
    QSignalSpy unsafe{&vm,
        &app::TerminalLimiterViewModel::previewNonconformingRequested};
    QSignalSpy verified{&vm,
        &app::TerminalLimiterViewModel::previewRequested};
    QVERIFY(unsafe.isValid()&&verified.isValid());
    vm.setAudioStatus(QStringLiteral("NEEDS PREPARATION"));
    vm.setNonconformingDirectPreviewAvailable(true);
    QVERIFY(!vm.nonconforming_direct_preview_available());
    vm.previewNonconforming();
    QCOMPARE(unsafe.count(),0);
    QVERIFY(state.value()->set_user_bypass(false));
    vm.setAudioStatus(QStringLiteral(
        "NON-CONFORMING · -11.76 dBTP (target -12.00)"));
    QVERIFY(!vm.nonconforming_direct_preview_available());
    vm.setNonconformingDirectPreviewAvailable(true);
    QVERIFY(vm.nonconforming_direct_preview_available());
    // Direct and DEVICE-rate SRC permissions must never be conflated.
    QVERIFY(!vm.nonconforming_device_preview_available());
    vm.setNonconformingDevicePreviewAvailable(true);
    QVERIFY(vm.nonconforming_device_preview_available());
    const double ceiling=std::pow(10.0,-12.0/20.0);
    const double over=ceiling*std::pow(10.0,0.24/20.0);
    vm.setNonconformingObservation(over,ceiling,-12.0);
    QCOMPARE(vm.nonconforming_peak_text(),
        QStringLiteral("-11.76 dBTP"));
    QCOMPARE(vm.nonconforming_target_text(),
        QStringLiteral("-12.00 dBTP"));
    QCOMPARE(vm.nonconforming_excess_text(),QStringLiteral("+0.24 dB"));
    // One positive binary64 ULP must never display as +0.00 dB.
    vm.setNonconformingObservation(
        std::nextafter(ceiling,std::numeric_limits<double>::infinity()),
        ceiling,-12.0);
    QCOMPARE(vm.nonconforming_peak_text(),QStringLiteral("-12.00 dBTP"));
    QCOMPARE(vm.nonconforming_excess_text(),QStringLiteral("<0.01 dB"));
    vm.setNonconformingObservation(ceiling,ceiling,-12.0);
    QVERIFY(vm.nonconforming_excess_text().isEmpty());
    vm.setNonconformingObservation(over,ceiling,-12.0);
    qInfo().noquote()<<"M17_EDGE_CASE=NONCONFORMING_EXPLICIT_CONSENT_ONLY";
    QCOMPARE(unsafe.count(),0); // Status publication NEVER auto-auditions.
    QSignalSpy cancelled{&vm,&app::TerminalLimiterViewModel::cancelRequested};
    QVERIFY(cancelled.isValid());
    vm.cancelPreparation();
    QCOMPARE(cancelled.count(),0); // A retained PCM is not Preparing.
    vm.setNonconformingDevicePreparing(true);
    QVERIFY(vm.nonconforming_device_preparing());
    QVERIFY(!vm.nonconforming_device_preview_available());
    vm.cancelPreparation();
    QCOMPARE(cancelled.count(),1); // Explicitly running DEVICE FIR is cancelable.
    vm.setNonconformingDevicePreparing(false);
    QVERIFY(!vm.nonconforming_device_preparing());
    vm.cancelPreparation();
    QCOMPARE(cancelled.count(),1); // Ending DEVICE job revokes its Cancel.
    vm.previewPrepared();
    QCOMPARE(verified.count(),0); // Not a verified READY candidate.
    vm.previewNonconforming();
    QCOMPARE(unsafe.count(),1); // Explicit action only.
    vm.setAudioStatus(QStringLiteral("READY · PREVIEW AVAILABLE"));
    QVERIFY(!vm.nonconforming_direct_preview_available());
    QVERIFY(!vm.nonconforming_device_preview_available());
    vm.cancelPreparation();
    QCOMPARE(cancelled.count(),1); // READY is not cancellable.
    vm.setAudioStatus(QStringLiteral("Preparing audio"));
    vm.cancelPreparation();
    QCOMPARE(cancelled.count(),2); // Ordinary full M17 Prepare is distinct.
    QVERIFY(state.value()->set_user_bypass(true));
    vm.previewNonconforming();
    QCOMPARE(unsafe.count(),1); // No audition from Bypass.
}


// Pre-FQ sustained-medium fixture: the 432-route table above proves short
// real PCM paths, NOT multi-block handoff, processing at realistic frame
// counts, or physical Windows playback. Use bounded 1–3 second source
// programmes at three representative clocks, including two real crossrates.
// The immutable verified MASTER must survive DEVICE SRC bit-for-bit.
// No synthetic fake M16 verification, no device-rate re-render, no DAC claim.
void TerminalActivationIntegrationTest::
sustainedCrossrateMasterDevice48kWithoutSecondRender()
{
    struct Case {
        std::int64_t sourceHz;
        std::int64_t masterHz;
        std::int64_t frames;
    };
    constexpr std::array<Case,3> scenarios{{
        {48000,48000,3*48000},
        {44100,192000,2*44100},
        {192000,48000,192000}
    }};
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    // DEVICE SRC has a strict maximum of 1,024 frames per emission
    // block. The sustained test must honor the SAME production contract;
    // longer PROGRAMMES are exercised by many legal output blocks.
    const auto block=core::FrameCount::create(1024);
    const auto deviceRate=core::SampleRate::create(48000);
    const auto uuid=core::Uuid::parse(
        "7f000000-0000-4000-8000-000000008037");
    QVERIFY(registry&&block&&deviceRate&&uuid);
    QCOMPARE(block.value()->value(),
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    const auto instance=dsp::ModuleInstanceId::from_uuid(*uuid.value());
    QVERIFY(instance);
    auto limiter=app::TerminalLimiterState::create_default(
        *registry.value(),*instance.value());
    QVERIFY(limiter);
    QVERIFY(limiter.value()->set_user_bypass(false));
    const auto bindings=limiter.value()->execution_bindings();
    std::size_t genuineMasters=0;
    std::size_t deviceCandidates=0;
    for(const auto& scenario:scenarios) {
        const auto sourceRate=core::SampleRate::create(scenario.sourceHz);
        const auto masterRate=core::SampleRate::create(scenario.masterHz);
        const auto frames=core::FrameCount::create(scenario.frames);
        QVERIFY(sourceRate&&masterRate&&frames);
        const auto fmt=audio::AudioFormat::create(
            *sourceRate.value(),audio::ChannelLayout::STEREO_LR);
        QVERIFY(fmt);
        auto input=audio::AudioBuffer::create(
            *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},*frames.value());
        QVERIFY(input);
        for(std::size_t ch=0;ch<2;++ch) {
            const auto samples=input.value()->mutable_view().channel(ch);
            QVERIFY(samples);
            for(std::size_t i=0;i<samples.value()->size();++i) {
                // Every 4,096-frame boundary and BOTH physical ends have
                // nonzero content, with intermittent transients and silence.
                const double x=(i==0||i+1==samples.value()->size()
                    ||i%4096==0)?0.32:(i%173<37?0.0:0.045);
                (*samples.value())[i]=ch==0?x:-0.8*x;
            }
        }
        auto chain=dsp::ProcessingChain::create(
            *registry.value(),{dsp::ProcessingStage::MASTER,
                               dsp::ChainSegment::MANUAL});
        QVERIFY(chain);
        const auto request=render::RenderRequest::create(
            input.value()->view(),input.value()->view().absolute_range(),
            *chain.value(),{},*block.value());
        QVERIFY(request);
        auto manualResult=render::render_preview(
            *request.value(),*registry.value());
        QVERIFY2(manualResult,manualResult?""
            :manualResult.error()->message().c_str());
        auto manual=std::make_shared<const render::RenderResult>(
            std::move(*manualResult.value()));
        const auto verified=render::
            render_true_peak_limiter_owned_final_master_candidate(
                manual,*chain.value(),*frames.value(),
                manual->chain_revision(),*masterRate.value(),
                limiter.value()->chain(),bindings,*registry.value(),
                *block.value());
        QVERIFY2(verified,verified?""
            :verified.error()->message().c_str());
        QVERIFY(render::validate_final_master_encoding_rate(
            *verified.value(),*masterRate.value()));
        const auto master=verified.value()->output.terminal.checked.terminal
            .compensated_pcm.view();
        QCOMPARE(master.format().sample_rate(),*masterRate.value());
        QCOMPARE(verified.value()->output.used_candidate_src,
            scenario.sourceHz!=scenario.masterHz);
        const auto expectedMasterFrames=
            (scenario.frames/scenario.sourceHz)*scenario.masterHz
            +((scenario.frames%scenario.sourceHz)*scenario.masterHz
               +scenario.sourceHz-1)/scenario.sourceHz;
        QCOMPARE(master.frame_count().value(),expectedMasterFrames);
        std::uint64_t before=UINT64_C(0xCBF29CE484222325);
        for(std::size_t ch=0;ch<2;++ch) {
            const auto samples=master.channel(ch);
            QVERIFY(samples);
            for(const double value:*samples.value()) {
                QVERIFY(std::isfinite(value));
                before^=std::bit_cast<std::uint64_t>(value);
                before*=UINT64_C(0x100000001B3);
            }
        }
        ++genuineMasters;
        const auto monitor=audio::materialize_monitoring_pcm_candidate(
            master,*deviceRate.value(),*block.value(),
            [](const audio::MonitoringPcmResourceProjection&)
                ->core::Status {return core::Status::success();});
        QVERIFY2(monitor,monitor?"":monitor.error()->message().c_str());
        QVERIFY(audio::validate_monitoring_pcm_candidate(*monitor.value()));
        QCOMPARE(monitor.value()->immutable_master_rate,*masterRate.value());
        QCOMPARE(monitor.value()->device_rate,*deviceRate.value());
        const auto devicePcm=monitor.value()->device_pcm.view();
        QCOMPARE(devicePcm.format().sample_rate(),*deviceRate.value());
        QCOMPARE(devicePcm.frame_count().value(),scenario.frames
            *(48000LL)/scenario.sourceHz);
        // A second render would have to replace this verified MASTER;
        // the whole immutable PCM remains bit-exactly unchanged.
        std::uint64_t after=UINT64_C(0xCBF29CE484222325);
        for(std::size_t ch=0;ch<2;++ch) {
            const auto samples=master.channel(ch);
            QVERIFY(samples);
            for(const double value:*samples.value()) {
                after^=std::bit_cast<std::uint64_t>(value);
                after*=UINT64_C(0x100000001B3);
            }
        }
        QCOMPARE(after,before);
        const auto timeline=audio::MonitoringFrameMap::create(
            *monitor.value());
        QVERIFY(timeline);
        const auto position=timeline.value()->source_to_master(
            core::FrameIndex{scenario.frames/2},
            *sourceRate.value(),*frames.value());
        QVERIFY(position);
        QVERIFY(timeline.value()->master_to_device(*position.value()));
        ++deviceCandidates;
    }
    QCOMPARE(genuineMasters,std::size_t{3});
    QCOMPARE(deviceCandidates,std::size_t{3});
    qInfo().noquote()
        <<"M17_EDGE_CASE=SUSTAINED_REAL_PCM_3_PATHS_MASTER_DEVICE48_PRESERVED";
}

void TerminalActivationIntegrationTest::
sixSourceBySixTargetOfflineMasterAndDeviceRateMatrix()
{
    // 36 independent, genuine source-rate to M17 MASTER-rate renders.
    // A separate set of 36 conversions makes DEVICE PCM from exact-rate
    // verified masters. These are short non-zero PCM functional tests,
    // NOT a hardware audition, Windows 150% screenshot, or M18 delivery.
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    const auto frames=core::FrameCount::create(64);
    const auto block=core::FrameCount::create(64);
    QVERIFY(registry&&frames&&block);
    const auto uuid=core::Uuid::parse(
        "7f000000-0000-4000-8000-000000008036");
    QVERIFY(uuid);
    auto instance=dsp::ModuleInstanceId::from_uuid(*uuid.value());
    QVERIFY(instance);
    auto limiter=app::TerminalLimiterState::create_default(
        *registry.value(),*instance.value());
    QVERIFY(limiter);
    QVERIFY(limiter.value()->set_user_bypass(false));
    const auto bindings=limiter.value()->execution_bindings();
    std::size_t masterPairs=0;
    std::size_t wavPairs=0;
    std::size_t devicePairs=0;
    std::size_t equalSourceMasterDevicePairs=0;
    for(const auto sourceHz:rates) {
        const auto sourceRate=core::SampleRate::create(sourceHz);
        QVERIFY(sourceRate);
        auto manualChain=dsp::ProcessingChain::create(
            *registry.value(),{dsp::ProcessingStage::MASTER,
                               dsp::ChainSegment::MANUAL});
        QVERIFY(manualChain);
        auto format=audio::AudioFormat::create(
            *sourceRate.value(),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto source=audio::AudioBuffer::create(
            *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},*frames.value());
        QVERIFY(source);
        for(std::size_t channel=0;channel<2;++channel) {
            auto lane=source.value()->mutable_view().channel(channel);
            QVERIFY(lane);
            for(std::size_t i=0;i<lane.value()->size();++i)
                (*lane.value())[i]=((i%9==0)?0.12:0.055)
                    *(channel==0?1.0:-0.8);
        }
        auto request=render::RenderRequest::create(
            source.value()->view(),source.value()->view().absolute_range(),
            *manualChain.value(),{},*block.value());
        QVERIFY(request);
        auto rendered=render::render_preview(
            *request.value(),*registry.value());
        QVERIFY(rendered);
        auto manual=std::make_shared<const render::RenderResult>(
            std::move(*rendered.value()));
        for(const auto finalHz:rates) {
            const auto finalRate=core::SampleRate::create(finalHz);
            QVERIFY(finalRate);
            auto out=render::render_true_peak_limiter_owned_final_master_candidate(
                manual,*manualChain.value(),*frames.value(),
                manual->chain_revision(),*finalRate.value(),
                limiter.value()->chain(),bindings,*registry.value(),*block.value());
            QVERIFY2(out,out?"":out.error()->message().c_str());
            QCOMPARE(out.value()->master_sample_rate,*finalRate.value());
            QCOMPARE(out.value()->output.output_rate,*finalRate.value());
            QCOMPARE(out.value()->output.used_candidate_src,sourceHz!=finalHz);
            QVERIFY(render::validate_final_master_encoding_rate(
                *out.value(),*finalRate.value()));
            const auto master=out.value()->output.terminal.checked.terminal
                .compensated_pcm.view();
            QCOMPARE(master.format().sample_rate(),*finalRate.value());
            QCOMPARE(master.timebase().frame_domain_id(),
                audio::FrameDomainId::OUTPUT_RATE);
            QVERIFY(master.frame_count().value()>0);
            // Snapshot the exact IEEE-754 bits of the fully verified
            // MASTER; NO DEVICE materializer may write into its owner.
            std::vector<std::uint64_t> originalMasterBits;
            for(std::size_t ch=0;ch<2;++ch) {
                const auto samples=master.channel(ch);
                QVERIFY(samples);
                for(const double value:*samples.value())
                    originalMasterBits.push_back(
                        std::bit_cast<std::uint64_t>(value));
            }
            ++masterPairs;
            // Actual WAV bytes, rather than an export-rate intention:
            // IEEE F64 WavWriter is already implemented independently of
            // M18's later PCM24/dither/product export UI.
            auto wavBytes=std::make_shared<MatrixWavBytes>();
            const audio::WavWriteSpec wavSpec{
                master.format(),master.frame_count(),
                audio::WavSampleFormat::IEEE_F64};
            auto writer=audio::WavWriter::open(
                std::make_unique<MatrixWavSink>(wavBytes),wavSpec);
            QVERIFY(writer);
            QVERIFY((*writer.value())->write_frames(master));
            QVERIFY((*writer.value())->finalize());
            QVERIFY((*writer.value())->close());
            QVERIFY(wavBytes->bytes.size()>=58);
            QCOMPARE(matrix_wav_u32(wavBytes->bytes,24),
                static_cast<std::uint32_t>(finalHz));
            QCOMPARE(matrix_wav_u32(wavBytes->bytes,54),
                static_cast<std::uint32_t>(
                    master.frame_count().value()*2*sizeof(double)));
            ++wavPairs;
            // Exercise the FULL composed physical rate topology:
            // 6 SOURCE rates x 6 MASTER rates x 6 DEVICE rates = 216.
            // Prior 36+36 tests covered only same SOURCE/MASTER DEVICE
            // conversion, not the actual SOURCE!=MASTER!=DEVICE path.
            // Each device buffer must be derived from THIS M17 MASTER,
            // not from the source or another verified candidate.
            for(const auto deviceHz:rates) {
                const auto deviceRate=core::SampleRate::create(deviceHz);
                QVERIFY(deviceRate);
                const auto plan=audio::plan_master_monitoring_rate(
                    *finalRate.value(),*deviceRate.value());
                QVERIFY(plan);
                auto monitor=audio::materialize_monitoring_pcm_candidate(
                    master,*deviceRate.value(),*block.value(),
                    [](const audio::MonitoringPcmResourceProjection&)
                        ->core::Status {
                        return core::Status::success();
                    });
                QVERIFY2(monitor,monitor?"":monitor.error()->message().c_str());
                QVERIFY(audio::validate_monitoring_pcm_candidate(
                    *monitor.value()));
                QCOMPARE(monitor.value()->immutable_master_rate,
                         *finalRate.value());
                QCOMPARE(monitor.value()->device_rate,*deviceRate.value());
                QCOMPARE(monitor.value()->device_src_applied,
                    finalHz!=deviceHz);
                QCOMPARE(monitor.value()->device_pcm.view().format().sample_rate(),
                         *deviceRate.value());
                // Real complete M17 Final SRC (not a synthetic measured
                // stage) must admit SOURCE->MASTER->DEVICE playhead and
                // half-open Loop mapping for ALL 216 stereo rate triplets.
                const auto timeline=audio::MonitoringFrameMap::create(
                    *monitor.value());
                QVERIFY(timeline);
                const auto sourceCue=timeline.value()->source_to_master(
                    core::FrameIndex{32},*sourceRate.value(),*frames.value());
                QVERIFY2(sourceCue,sourceCue?""
                    :sourceCue.error()->message().c_str());
                const auto deviceCue=timeline.value()->master_to_device(
                    *sourceCue.value());
                QVERIFY(deviceCue);
                const auto sourceRegion=core::FrameRange::create(
                    core::FrameIndex{8},core::FrameIndex{48});
                QVERIFY(sourceRegion);
                const auto masterRegion=timeline.value()->source_loop_to_master(
                    *sourceRegion.value(),*sourceRate.value(),*frames.value());
                QVERIFY(masterRegion);
                const auto deviceRegion=timeline.value()->master_loop_to_device(
                    *masterRegion.value());
                QVERIFY(deviceRegion);
                const auto entireMasterRegion=core::FrameRange::create(
                    core::FrameIndex{0},
                    core::FrameIndex{master.frame_count().value()});
                QVERIFY(entireMasterRegion);
                const auto entireSourceReturn=
                    timeline.value()->master_loop_to_source(
                        *entireMasterRegion.value(),
                        *sourceRate.value(),*frames.value());
                QVERIFY(entireSourceReturn);
                QCOMPARE(entireSourceReturn.value()->begin(),
                         core::FrameIndex{0});
                QCOMPARE(entireSourceReturn.value()->end(),
                         core::FrameIndex{frames.value()->value()});
                const auto masterFrames=master.frame_count().value();
                const auto expectedDeviceFrames=
                    (masterFrames/finalHz)*deviceHz
                    +((masterFrames%finalHz)*deviceHz+finalHz-1)/finalHz;
                QCOMPARE(monitor.value()->device_pcm.view().frame_count().value(),
                         expectedDeviceFrames);
                // The device-rate allocation must also be distinct from
                // the immutable MASTER buffer for SRC paths.
                std::vector<std::uint64_t> afterDeviceMasterBits;
                for(std::size_t ch=0;ch<2;++ch) {
                    const auto samples=master.channel(ch);
                    QVERIFY(samples);
                    for(const double value:*samples.value())
                        afterDeviceMasterBits.push_back(
                            std::bit_cast<std::uint64_t>(value));
                }
                QCOMPARE(afterDeviceMasterBits,originalMasterBits);
                QCOMPARE(master.format().sample_rate(),*finalRate.value());
                if(sourceHz==finalHz)++equalSourceMasterDevicePairs;
                ++devicePairs;
            }
        }
    }
    QCOMPARE(masterPairs,std::size_t{36});
    QCOMPARE(wavPairs,std::size_t{36});
    QCOMPARE(devicePairs,std::size_t{216});
    QCOMPARE(equalSourceMasterDevicePairs,std::size_t{36});

    // MONO is a separate full product pipeline, not a mere playback
    // DeviceFormat capability bit. Genuine M17 output-rate render and
    // standalone listening PCM are exercised for another 6x6x6=216 paths.
    // Reuse no stereo buffer or cached terminal stage in this loop.
    std::size_t monoMasterPairs=0;
    std::size_t monoDevicePairs=0;
    for(const auto sourceHz:rates) {
        const auto sourceRate=core::SampleRate::create(sourceHz);
        QVERIFY(sourceRate);
        const auto monoFormat=audio::AudioFormat::create(
            *sourceRate.value(),audio::ChannelLayout::MONO_C);
        QVERIFY(monoFormat);
        auto monoSource=audio::AudioBuffer::create(
            *monoFormat.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},*frames.value());
        QVERIFY(monoSource);
        auto samples=monoSource.value()->mutable_view().channel(0);
        QVERIFY(samples);
        for(std::size_t i=0;i<samples.value()->size();++i)
            (*samples.value())[i]=(i%13==0?-0.061:0.031);
        auto monoChain=dsp::ProcessingChain::create(
            *registry.value(),{dsp::ProcessingStage::MASTER,
                               dsp::ChainSegment::MANUAL});
        QVERIFY(monoChain);
        auto monoRequest=render::RenderRequest::create(
            monoSource.value()->view(),
            monoSource.value()->view().absolute_range(),
            *monoChain.value(),{},*block.value());
        QVERIFY(monoRequest);
        auto monoRendered=render::render_preview(
            *monoRequest.value(),*registry.value());
        QVERIFY(monoRendered);
        auto monoManual=std::make_shared<const render::RenderResult>(
            std::move(*monoRendered.value()));
        for(const auto masterHz:rates) {
            const auto masterRate=core::SampleRate::create(masterHz);
            QVERIFY(masterRate);
            auto final=render::render_true_peak_limiter_owned_final_master_candidate(
                monoManual,*monoChain.value(),*frames.value(),
                monoManual->chain_revision(),*masterRate.value(),
                limiter.value()->chain(),bindings,*registry.value(),
                *block.value());
            QVERIFY2(final,final?"":final.error()->message().c_str());
            QVERIFY(render::validate_final_master_encoding_rate(
                *final.value(),*masterRate.value()));
            const auto master=final.value()->output.terminal.checked.terminal
                .compensated_pcm.view();
            QCOMPARE(master.format().sample_rate(),*masterRate.value());
            QCOMPARE(master.format().channel_layout(),
                audio::ChannelLayout::MONO_C);
            QCOMPARE(master.timebase().frame_domain_id(),
                audio::FrameDomainId::OUTPUT_RATE);
            QCOMPARE(master.absolute_start_frame(),core::FrameIndex{0});
            QCOMPARE(final.value()->output.used_candidate_src,
                sourceHz!=masterHz);
            const auto original=master.channel(0);
            QVERIFY(original);
            std::vector<std::uint64_t> before;
            before.reserve(original.value()->size());
            for(const double v:*original.value()) {
                QVERIFY(std::isfinite(v));
                before.push_back(std::bit_cast<std::uint64_t>(v));
            }
            ++monoMasterPairs;
            for(const auto deviceHz:rates) {
                const auto deviceRate=core::SampleRate::create(deviceHz);
                QVERIFY(deviceRate);
                auto monitor=audio::materialize_monitoring_pcm_candidate(
                    master,*deviceRate.value(),*block.value(),
                    [](const audio::MonitoringPcmResourceProjection&)
                        ->core::Status {
                        return core::Status::success();
                    });
                QVERIFY2(monitor,monitor?"":monitor.error()->message().c_str());
                QVERIFY(audio::validate_monitoring_pcm_candidate(*monitor.value()));
                QCOMPARE(monitor.value()->immutable_master_rate,
                    *masterRate.value());
                QCOMPARE(monitor.value()->immutable_master_frames,
                    master.frame_count());
                QCOMPARE(monitor.value()->device_rate,*deviceRate.value());
                QCOMPARE(monitor.value()->device_src_applied,
                    masterHz!=deviceHz);
                // Apply the same SOURCE/MANUAL half-open loop and cue
                // contracts to all 216 MONO rate triplets. There is no
                // stereo-only assumption in MonitoringFrameMap.
                const auto timeline=audio::MonitoringFrameMap::create(
                    *monitor.value());
                QVERIFY(timeline);
                const auto masterCue=timeline.value()->source_to_master(
                    core::FrameIndex{32},*sourceRate.value(),*frames.value());
                QVERIFY(masterCue);
                QVERIFY(timeline.value()->master_to_device(*masterCue.value()));
                const auto sourceRegion=core::FrameRange::create(
                    core::FrameIndex{8},core::FrameIndex{48});
                QVERIFY(sourceRegion);
                const auto masterRegion=timeline.value()->source_loop_to_master(
                    *sourceRegion.value(),*sourceRate.value(),*frames.value());
                QVERIFY(masterRegion);
                QVERIFY(timeline.value()->master_loop_to_device(
                    *masterRegion.value()));
                const auto fullMaster=core::FrameRange::create(
                    core::FrameIndex{0},
                    core::FrameIndex{master.frame_count().value()});
                QVERIFY(fullMaster);
                const auto sourceReturn=timeline.value()->master_loop_to_source(
                    *fullMaster.value(),*sourceRate.value(),*frames.value());
                QVERIFY(sourceReturn);
                QCOMPARE(sourceReturn.value()->begin(),core::FrameIndex{0});
                QCOMPARE(sourceReturn.value()->end(),
                         core::FrameIndex{frames.value()->value()});
                const auto devicePcm=monitor.value()->device_pcm.view();
                QCOMPARE(devicePcm.format().sample_rate(),*deviceRate.value());
                QCOMPARE(devicePcm.format().channel_layout(),
                    audio::ChannelLayout::MONO_C);
                const auto masterCount=master.frame_count().value();
                const auto expectedDeviceCount=
                    (masterCount/masterHz)*deviceHz
                    +((masterCount%masterHz)*deviceHz+masterHz-1)/masterHz;
                QCOMPARE(devicePcm.frame_count().value(),expectedDeviceCount);
                const auto emitted=devicePcm.channel(0);
                QVERIFY(emitted);
                for(const double v:*emitted.value())QVERIFY(std::isfinite(v));
                const auto unchanged=master.channel(0);
                QVERIFY(unchanged);
                QCOMPARE(unchanged.value()->size(),before.size());
                for(std::size_t i=0;i<before.size();++i)
                    QCOMPARE(std::bit_cast<std::uint64_t>(
                        (*unchanged.value())[i]),before[i]);
                ++monoDevicePairs;
            }
        }
    }
    QCOMPARE(monoMasterPairs,std::size_t{36});
    QCOMPARE(monoDevicePairs,std::size_t{216});
    // Retain the historical CI attestation for the 36 legacy equal
    // SOURCE/MASTER pair subset; assert its count independently above.
    qInfo().noquote()<<"M17_RATE_MATRIX=36_MASTER_PLUS_36_IEEE_F64_WAV_PLUS_36_DEVICE_PCM";
    qInfo().noquote()<<"M17_RATE_MATRIX_FULL_COMPOSED_CHAIN=216_SOURCE_MASTER_DEVICE_PCM";
    qInfo().noquote()<<"M17_RATE_MATRIX_MONO=216_SOURCE_MASTER_DEVICE_PCM";
    qInfo().noquote()<<"M17_EDGE_CASE=RATE_GRID_432_TRIPLES_MONO_STEREO_MASTER_DEVICE";
}

void TerminalActivationIntegrationTest::finalMasterAt192kNeverBecomesDeviceRate48k()
{
    // Product Final Master contract, NOT Windows live-playback qualification:
    // a 48 kHz device may never override the exact user's 192 kHz master.
    // No device is consulted by render_true_peak_limiter_owned_final_master_candidate.
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    const auto sourceRate=core::SampleRate::create(192000);
    const auto deviceRate=core::SampleRate::create(48000);
    const auto frames=core::FrameCount::create(513);
    const auto block=core::FrameCount::create(128);
    QVERIFY(registry&&sourceRate&&deviceRate&&frames&&block);

    auto manualChain=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manualChain);
    auto fmt=audio::AudioFormat::create(*sourceRate.value(),
        audio::ChannelLayout::STEREO_LR);
    QVERIFY(fmt);
    auto source=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(source);
    for(std::size_t ch=0;ch<2;++ch) {
        auto lane=source.value()->mutable_view().channel(ch);
        QVERIFY(lane);
        for(std::size_t i=0;i<513;++i)
            (*lane.value())[i]=(i%19==0 ? 0.35 : 0.14)
                *(ch==0 ? 1.0 : -0.75);
    }
    auto request=render::RenderRequest::create(
        source.value()->view(),source.value()->view().absolute_range(),
        *manualChain.value(),{},*block.value());
    QVERIFY(request);
    auto rendered=render::render_preview(*request.value(),*registry.value());
    QVERIFY(rendered);
    auto manual=std::make_shared<const render::RenderResult>(
        std::move(*rendered.value()));
    auto instance=dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(
        "7f000000-0000-4000-8000-000000000517").value());
    QVERIFY(instance);
    auto limiter=app::TerminalLimiterState::create_default(
        *registry.value(),*instance.value());
    QVERIFY(limiter);
    QVERIFY(limiter.value()->set_user_bypass(false));
    const auto bindings=limiter.value()->execution_bindings();

    // Same-rate MASTER is fully independent of an audition-only device.
    auto master=render::render_true_peak_limiter_owned_final_master_candidate(
        manual,*manualChain.value(),*frames.value(),
        manual->chain_revision(),*sourceRate.value(),
        limiter.value()->chain(),bindings,*registry.value(),*block.value());
    QVERIFY2(master,master.error()
        ?master.error()->message().c_str():"192 kHz master failed");
    QCOMPARE(master.value()->master_sample_rate,*sourceRate.value());
    QCOMPARE(master.value()->output.output_rate,*sourceRate.value());
    QVERIFY(!master.value()->output.used_candidate_src);
    const auto& checked=master.value()->output.terminal.checked;
    QVERIFY(checked.final_check.has_value());
    const auto pcm=checked.terminal.compensated_pcm.view();
    QCOMPARE(pcm.format().sample_rate(),*sourceRate.value());
    QCOMPARE(pcm.frame_count(),*frames.value());
    QCOMPARE(pcm.timebase().frame_domain_id(),
        audio::FrameDomainId::OUTPUT_RATE);
    QVERIFY(render::validate_final_master_encoding_rate(
        *master.value(),*sourceRate.value()));
    // Substituting the 48 kHz playback device for the encoder must fail.
    QVERIFY(!render::validate_final_master_encoding_rate(
        *master.value(),*deviceRate.value()));

    // 192 -> 48 is now an explicitly requested OTHER FINAL MASTER,
    // produced by PRE-terminal offline SRC; it is NOT a monitoring
    // conversion applied after the above immutable 192-kHz M17 master.
    auto differentMaster=render::render_true_peak_limiter_owned_final_master_candidate(
        manual,*manualChain.value(),*frames.value(),
        manual->chain_revision(),*deviceRate.value(),
        limiter.value()->chain(),bindings,*registry.value(),*block.value());
    QVERIFY2(differentMaster,differentMaster?""
        :differentMaster.error()->message().c_str());
    QCOMPARE(differentMaster.value()->master_sample_rate,*deviceRate.value());
    QCOMPARE(differentMaster.value()->output.output_rate,*deviceRate.value());
    QVERIFY(differentMaster.value()->output.used_candidate_src);
    QVERIFY(render::validate_final_master_encoding_rate(
        *differentMaster.value(),*deviceRate.value()));
    QVERIFY(!render::validate_final_master_encoding_rate(
        *differentMaster.value(),*sourceRate.value()));
    QCOMPARE(master.value()->master_sample_rate,*sourceRate.value());

    // The optional recovery seam preserves real upstream ownership and never
    // invents a non-conforming result when the verified master is valid.
    std::optional<render::TruePeakLimiterOwnedMeasuredNonconformingCandidate>
        recovered;
    auto diagnostic=render::render_true_peak_limiter_owned_output_rate_candidate(
        manual,*manualChain.value(),*frames.value(),manual->chain_revision(),
        *sourceRate.value(),limiter.value()->chain(),bindings,
        *registry.value(),*block.value(),{},nullptr,true,&recovered);
    QVERIFY(diagnostic);
    QVERIFY(!recovered);
    // Only a genuinely unsupported target remains fail-closed. A valid
    // selected master rate (including 48 kHz) is never a failure merely
    // because the Source PCM here happens to be 192 kHz.
    const auto invalidRate=core::SampleRate::create(32000);
    QVERIFY(invalidRate);
    auto invalidSrc=render::render_true_peak_limiter_owned_output_rate_candidate(
        manual,*manualChain.value(),*frames.value(),manual->chain_revision(),
        *invalidRate.value(),limiter.value()->chain(),bindings,
        *registry.value(),*block.value(),{},nullptr,true,&recovered);
    QVERIFY(!invalidSrc);
    QVERIFY(!recovered);

    // M17 CI structural-only fixtures: deliberately NOT real M16
    // measurements. Production admission reads the CandidateGate stage on
    // the owning QObject thread, rechecks the exact stage after conversion.
    using Staged=app::TerminalCandidateGate::StagedMeasuredCandidate;
    const auto makeSyntheticStage=[&](
        double db,double ceiling,double measuredPeak,
        render::M17NonconformingReason reason,
        analysis::TruePeakQualificationStatus qualification,
        std::optional<core::SampleRate> stageRate=std::nullopt,
        bool forgeMissingSourceSrcProof=false)
        ->std::shared_ptr<const Staged> {
        const auto finalHz=stageRate.value_or(*sourceRate.value());
        auto targetFormat=audio::AudioFormat::create(
            finalHz,pcm.format().channel_layout());
        if(!targetFormat)return {};
        // Synthetic TEST-ONLY geometry follows the actual rational
        // SOURCE->MASTER duration contract, unlike prior equal-frame
        // fixtures that concealed invalid cross-rate admission.
        const auto sourceN=pcm.frame_count().value();
        const auto sourceHz=sourceRate.value()->value();
        const auto masterHz=finalHz.value();
        const auto masterN=(sourceN*masterHz+sourceHz-1)/sourceHz;
        const auto targetCount=core::FrameCount::create(masterN);
        if(!targetCount)return {};
        auto original=audio::AudioBuffer::create(
            *targetFormat.value(),audio::FrameDomainId::OUTPUT_RATE,
            core::FrameIndex{0},*targetCount.value());
        if(!original)return {};
        for(std::size_t ch=0;ch<pcm.format().channel_count();++ch) {
            const auto from=pcm.channel(ch);
            auto to=original.value()->mutable_view().channel(ch);
            if(!from||!to)return {};
            for(std::size_t k=0;k<to.value()->size();++k) {
                const auto sourceIndex=std::min<std::size_t>(
                    from.value()->size()-1,k*static_cast<std::size_t>(sourceHz)
                        /static_cast<std::size_t>(masterHz));
                (*to.value())[k]=(*from.value())[sourceIndex];
            }
        }
        std::vector<dsp::TruePeakLimiterAppliedGainFrame> syntheticGain;
        syntheticGain.reserve(static_cast<std::size_t>(masterN));
        for(std::int64_t k=0;k<masterN;++k)
            syntheticGain.push_back(
                checked.terminal.applied_gain_frames[std::min<std::size_t>(
                    checked.terminal.applied_gain_frames.size()-1,
                    static_cast<std::size_t>(k*sourceHz/masterHz))]);
        render::TruePeakLimiterTerminalCandidate taggedPcm{
            std::move(*original.value()),std::move(syntheticGain),0,false};
        render::TruePeakLimiterMeasuredNonconformingCandidate fakeMiss{
            std::move(taggedPcm),db,ceiling,measuredPeak,reason,
            qualification};
        render::TruePeakLimiterOwnedMeasuredNonconformingCandidate fakeOwned{
            manual,std::move(fakeMiss),*instance.value(),
            limiter.value()->chain().revision(),finalHz,
            *frames.value(),manual->chain_revision(),
            finalHz!=*sourceRate.value() && !forgeMissingSourceSrcProof,
            finalHz!=*sourceRate.value() && !forgeMissingSourceSrcProof
                ?std::string_view{"SYNTHETIC_TEST_ONLY_NOT_REAL_ORIGINAL_FIR"}
                :std::string_view{}};
        return std::make_shared<const Staged>(
            Staged{std::move(fakeOwned),core::RealizationId{9001},manual});
    };
    constexpr auto guarded=render::M17NonconformingReason::TRIM_GUARD_EXCEEDED;
    constexpr auto nominal=analysis::TruePeakQualificationStatus::
        SUPPORTED_UNQUALIFIED;
    const double expectedCeiling=std::pow(10.0,-7.0/20.0);
    const auto captured=makeSyntheticStage(
        -7.0,expectedCeiling,0.51,guarded,nominal);
    QVERIFY(captured);
    const auto masterPcm=captured->rendered.terminal.terminal
        .compensated_pcm.view();
    auto converted=audio::materialize_monitoring_pcm_candidate(
        masterPcm,*deviceRate.value(),*block.value(),
        [](const audio::MonitoringPcmResourceProjection&)
            ->core::Status {return core::Status::success();});
    QVERIFY2(converted,converted?"":converted.error()->message().c_str());
    auto unfinished=std::make_shared<const audio::MonitoringPcmCandidate>(
        std::move(*converted.value()));
    using app::admit_measured_device_audition_ticket;
    const app::MeasuredDeviceEndpointObservation freshEndpoint{
        "fixture-endpoint-A",*deviceRate.value()};
    QVERIFY(!admit_measured_device_audition_ticket(
        captured,captured,unfinished,false,freshEndpoint));
    QVERIFY(!admit_measured_device_audition_ticket(
        captured,{},unfinished,true,freshEndpoint));
    QVERIFY(!admit_measured_device_audition_ticket(
        captured,captured,unfinished,true,freshEndpoint)); // missing endpoint witness
    const auto shortCount=core::FrameCount::create(1);
    const auto deviceFmt=audio::AudioFormat::create(
        *deviceRate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(shortCount&&deviceFmt);
    auto shortPcm=audio::AudioBuffer::create(
        *deviceFmt.value(),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*shortCount.value());
    QVERIFY(shortPcm);
    auto invalidDevice=std::make_shared<const audio::MonitoringPcmCandidate>(
        audio::MonitoringPcmCandidate{
            std::move(*shortPcm.value()),*sourceRate.value(),
            *deviceRate.value(),*frames.value(),true,"fixture-endpoint-A"});
    // A device buffer with a false rate/frame count cannot be admitted.
    QVERIFY(!admit_measured_device_audition_ticket(
        captured,captured,invalidDevice,true,freshEndpoint));
    // A second properly typed real conversion is cheap (513 frames) and
    // carries only an opaque simulated endpoint ID, never LIVE authority.
    auto second=audio::materialize_monitoring_pcm_candidate(
        masterPcm,*deviceRate.value(),*block.value());
    QVERIFY(second);
    second.value()->windows_prepared_endpoint_id="fixture-endpoint-A";
    auto deviceOwned=std::make_shared<const audio::MonitoringPcmCandidate>(
        std::move(*second.value()));
    const auto ticket=admit_measured_device_audition_ticket(
        captured,captured,deviceOwned,true,freshEndpoint);
    QVERIFY(ticket);
    QCOMPARE(ticket.value()->measured_master(),captured);
    QCOMPARE(ticket.value()->audition_device_pcm(),deviceOwned);
    QCOMPARE(ticket.value()->original_manual_realization_id().value,
             std::uint64_t{9001});
    QCOMPARE(ticket.value()->audition_device_pcm()->device_rate,
             *deviceRate.value());
    QCOMPARE(ticket.value()->measured_master()->rendered.output_rate,
             *sourceRate.value());
    QVERIFY(!ticket.value()->audition_device_pcm()->windows_prepared_endpoint_id.empty());
    // CI #1005: source-cue, transport and half-open SOURCE loop projection
    // into a distinct anonymous DEVICE PCM. Current fixture has equal
    // SOURCE and measured MASTER clocks (192k) with a 48k endpoint.
    // No fake M16 VERIFIED result and no QAudioSink are published here.
    const auto sourceRegion=core::FrameRange::create(
        core::FrameIndex{64},core::FrameIndex{192});
    QVERIFY(sourceRegion);
    // Pure NO_SOURCE proof is intentionally stricter than an empty
    // RealizationId alone: a stale position, loop or duration means the
    // previous DEVICE owner has not been authoritatively retired.
    // Actual SOURCE selector normalizes EOF to the beginning. Without
    // this measured Return canonicalization the Windows SOURCE sink is
    // correctly restored at frame zero, but an old postcheck demanded N,
    // producing a false RECOVERY_REQUIRED after real track completion.
    const auto sourceEof=app::canonical_measured_source_return_cue(
        core::FrameIndex{frames.value()->value()},*frames.value(),std::nullopt);
    QVERIFY(sourceEof);
    QCOMPARE(*sourceEof.value(),core::FrameIndex{0});
    const auto middle=app::canonical_measured_source_return_cue(
        core::FrameIndex{128},*frames.value(),std::nullopt);
    QVERIFY(middle);
    QCOMPARE(*middle.value(),core::FrameIndex{128});
    const auto selectedLoop=core::FrameRange::create(
        core::FrameIndex{64},core::FrameIndex{192});
    QVERIFY(selectedLoop);
    const auto loopEnd=app::canonical_measured_source_return_cue(
        core::FrameIndex{192},*frames.value(),*selectedLoop.value());
    QVERIFY(loopEnd);
    QCOMPARE(*loopEnd.value(),core::FrameIndex{64});
    const auto loopInside=app::canonical_measured_source_return_cue(
        core::FrameIndex{128},*frames.value(),*selectedLoop.value());
    QVERIFY(loopInside);
    QCOMPARE(*loopInside.value(),core::FrameIndex{128});
    const auto outside=app::canonical_measured_source_return_cue(
        core::FrameIndex{frames.value()->value()+1},
        *frames.value(),std::nullopt);
    QVERIFY(!outside);
    core::PlaybackSnapshot emptyDevice{};
    emptyDevice.state=core::PlaybackState::NO_SOURCE;
    emptyDevice.position=core::FrameIndex{0};
    emptyDevice.audibleRealization={
        core::AudibleHandoffPhase::UNAVAILABLE,std::nullopt};
    QVERIFY(app::measured_empty_sink_snapshot_proven(emptyDevice));
    auto staleCursor=emptyDevice;
    staleCursor.position=core::FrameIndex{1};
    QVERIFY(!app::measured_empty_sink_snapshot_proven(staleCursor));
    auto leftoverDuration=emptyDevice;
    leftoverDuration.duration=*frames.value();
    QVERIFY(!app::measured_empty_sink_snapshot_proven(leftoverDuration));
    auto leftoverLoop=emptyDevice;
    leftoverLoop.loop=*sourceRegion.value();
    QVERIFY(!app::measured_empty_sink_snapshot_proven(leftoverLoop));
    auto alienIdentity=emptyDevice;
    alienIdentity.audibleRealization={
        core::AudibleHandoffPhase::NEW,core::RealizationId{9001}};
    QVERIFY(!app::measured_empty_sink_snapshot_proven(alienIdentity));
    auto stillPaused=emptyDevice;
    stillPaused.state=core::PlaybackState::PAUSED;
    QVERIFY(!app::measured_empty_sink_snapshot_proven(stillPaused));
    core::PlaybackSnapshot manualSnapshot{};
    manualSnapshot.state=core::PlaybackState::PLAYING;
    manualSnapshot.position=core::FrameIndex{128};
    manualSnapshot.duration=*frames.value();
    manualSnapshot.loop=*sourceRegion.value();
    manualSnapshot.audibleRealization={
        core::AudibleHandoffPhase::NEW,core::RealizationId{9001}};
    const auto planned=ticket.value()->plan_handoff(
        captured,manualSnapshot,*sourceRegion.value(),freshEndpoint,true);
    QVERIFY2(planned,planned?"":planned.error()->message().c_str());
    QCOMPARE(planned.value()->manual_owner,core::RealizationId{9001});
    QCOMPARE(planned.value()->source_state,core::PlaybackState::PLAYING);
    QCOMPARE(planned.value()->source_cue,core::FrameIndex{128});
    QCOMPARE(planned.value()->master_cue,core::FrameIndex{128});
    QCOMPARE(planned.value()->device_cue,core::FrameIndex{32});
    QCOMPARE(planned.value()->source_loop,std::optional{*sourceRegion.value()});
    QCOMPARE(planned.value()->master_loop,std::optional{*sourceRegion.value()});
    const auto expectedDeviceRegion=core::FrameRange::create(
        core::FrameIndex{16},core::FrameIndex{48});
    QVERIFY(expectedDeviceRegion);
    QCOMPARE(planned.value()->device_loop,
             std::optional{*expectedDeviceRegion.value()});
    // Exact rational seek contract: at 192 kHz MASTER -> 48 kHz DEVICE,
    // request MASTER frame 1 maps to DEVICE frame 1, but DEVICE->MASTER
    // display projects back to frame 4. Post-install transport checks
    // must compare DEVICE frames, never a false +/-1 MASTER tolerance.
    const auto seekMap=audio::MonitoringFrameMap::create(*deviceOwned);
    QVERIFY(seekMap);
    for(const auto masterIndex:std::array<std::int64_t,4>{1,2,3,4}) {
        const auto expectedDevice=seekMap.value()->master_to_device(
            core::FrameIndex{masterIndex});
        QVERIFY(expectedDevice);
        QCOMPARE(expectedDevice.value()->value(),std::int64_t{1});
        const auto reportedMaster=seekMap.value()->device_to_master_cursor(
            *expectedDevice.value());
        QVERIFY(reportedMaster);
        QCOMPARE(reportedMaster.value()->value(),std::int64_t{4});
    }
    qInfo().noquote()
        <<"M17_MEASURED_SEEK_COMPARE_DEVICE_NOT_MASTER_FRAMES=PASS";
    // Each plan is tied to SOURCE seek and loop-wrap epochs. Natural
    // forward playback is not itself a new epoch and must rebase its cue.
    auto advanced=manualSnapshot;
    advanced.position=core::FrameIndex{132};
    const auto freshPlan=ticket.value()->rebase_handoff(
        *planned.value(),captured,advanced,*sourceRegion.value(),
        freshEndpoint,true);
    QVERIFY(freshPlan);
    QCOMPARE(freshPlan.value()->source_cue,core::FrameIndex{132});
    QCOMPARE(freshPlan.value()->master_cue,core::FrameIndex{132});
    QCOMPARE(freshPlan.value()->device_cue,core::FrameIndex{33});
    auto seeked=advanced;
    seeked.seekSerial=1;
    QVERIFY(!ticket.value()->rebase_handoff(
        *planned.value(),captured,seeked,*sourceRegion.value(),
        freshEndpoint,true));
    auto restarted=advanced;
    restarted.traversalSerial=1;
    QVERIFY(!ticket.value()->rebase_handoff(
        *planned.value(),captured,restarted,*sourceRegion.value(),
        freshEndpoint,true));
    auto wrapped=advanced;
    wrapped.loopWrapCount=1;
    QVERIFY(!ticket.value()->rebase_handoff(
        *planned.value(),captured,wrapped,*sourceRegion.value(),
        freshEndpoint,true));
    auto paused=advanced;
    paused.state=core::PlaybackState::PAUSED;
    QVERIFY(!ticket.value()->rebase_handoff(
        *planned.value(),captured,paused,*sourceRegion.value(),
        freshEndpoint,true));
    auto rewound=manualSnapshot;
    rewound.position=core::FrameIndex{127};
    QVERIFY(!ticket.value()->rebase_handoff(
        *planned.value(),captured,rewound,*sourceRegion.value(),
        freshEndpoint,true));
    QVERIFY(!ticket.value()->rebase_handoff(
        *planned.value(),captured,advanced,*sourceRegion.value(),
        freshEndpoint,false));
    QVERIFY(!ticket.value()->rebase_handoff(
        *planned.value(),{},advanced,*sourceRegion.value(),
        freshEndpoint,true));
    QVERIFY(!ticket.value()->rebase_handoff(
        *planned.value(),captured,advanced,std::nullopt,
        freshEndpoint,true));

    QCOMPARE(planned.value()->source_rate,*sourceRate.value());
    QCOMPARE(planned.value()->master_rate,*sourceRate.value());
    QCOMPARE(planned.value()->device_rate,*deviceRate.value());
    // No consent, revoked measured stage, replaced endpoint, stale MANUAL,
    // changed loop/seek or unowned transport may be silently accepted.
    QVERIFY(!ticket.value()->plan_handoff(
        captured,manualSnapshot,*sourceRegion.value(),freshEndpoint,false));
    QVERIFY(!ticket.value()->plan_handoff(
        {},manualSnapshot,*sourceRegion.value(),freshEndpoint,true));
    const app::MeasuredDeviceEndpointObservation swappedDevice{
        "fixture-endpoint-B",*deviceRate.value()};
    QVERIFY(!ticket.value()->plan_handoff(
        captured,manualSnapshot,*sourceRegion.value(),swappedDevice,true));
    auto staleManual=manualSnapshot;
    staleManual.audibleRealization.realizationId=core::RealizationId{9002};
    QVERIFY(!ticket.value()->plan_handoff(
        captured,staleManual,*sourceRegion.value(),freshEndpoint,true));
    auto staleLoop=manualSnapshot;
    staleLoop.loop.reset();
    QVERIFY(!ticket.value()->plan_handoff(
        captured,staleLoop,*sourceRegion.value(),freshEndpoint,true));
    auto stalePosition=manualSnapshot;
    stalePosition.position=core::FrameIndex{514};
    QVERIFY(!ticket.value()->plan_handoff(
        captured,stalePosition,*sourceRegion.value(),freshEndpoint,true));
    auto unowned=manualSnapshot;
    unowned.state=core::PlaybackState::NO_SOURCE;
    QVERIFY(!ticket.value()->plan_handoff(
        captured,unowned,*sourceRegion.value(),freshEndpoint,true));
    auto wrongDuration=manualSnapshot;
    wrongDuration.duration=*shortCount.value();
    QVERIFY(!ticket.value()->plan_handoff(
        captured,wrongDuration,*sourceRegion.value(),freshEndpoint,true));
    qInfo().noquote()
        <<"M17_NONCONFORMING_SOURCE_MASTER_DEVICE_HANDOFF_PLAN=PASS";
    // Adversarial metadata: a geometrically valid converted PCM alone
    // must never let a forged TP/Ceiling record enter even an UNQUALIFIED
    // audition ticket. These regressions are intentionally far stricter
    // than simple format/rate checks; none touches frozen M16 arithmetic.
    const auto invalidCeiling=makeSyntheticStage(
        -7.0,0.44,0.51,guarded,nominal);
    QVERIFY(invalidCeiling);
    QVERIFY(!admit_measured_device_audition_ticket(
        invalidCeiling,invalidCeiling,deviceOwned,true,freshEndpoint));
    const auto invalidQualification=makeSyntheticStage(
        -7.0,expectedCeiling,0.51,guarded,
        analysis::TruePeakQualificationStatus::QUALIFIED);
    QVERIFY(invalidQualification);
    QVERIFY(!admit_measured_device_audition_ticket(
        invalidQualification,invalidQualification,deviceOwned,true,freshEndpoint));
    const auto invalidReason=makeSyntheticStage(
        -7.0,expectedCeiling,0.51,
        static_cast<render::M17NonconformingReason>(255),nominal);
    QVERIFY(invalidReason);
    QVERIFY(!admit_measured_device_audition_ticket(
        invalidReason,invalidReason,deviceOwned,true,freshEndpoint));
    const auto invalidRange=makeSyntheticStage(
        -12.0001,expectedCeiling,0.51,guarded,nominal);
    QVERIFY(invalidRange);
    QVERIFY(!admit_measured_device_audition_ticket(
        invalidRange,invalidRange,deviceOwned,true,freshEndpoint));
    const auto noMiss=makeSyntheticStage(
        -7.0,expectedCeiling,expectedCeiling,guarded,nominal);
    QVERIFY(noMiss);
    QVERIFY(!admit_measured_device_audition_ticket(
        noMiss,noMiss,deviceOwned,true,freshEndpoint));

    // SOURCE/M17 rate remains unchanged. A *synthetic* measured TP miss is
    // retimed OFFLINE into separate 48-kHz DEVICE PCM for every MASTER rate.
    // This is neither true M16 measurement nor a physical Windows audition.
    std::size_t measuredCases=0;
    for(const auto hz : {44100,48000,88200,96000,176400,192000}) {
        const auto masterRate=core::SampleRate::create(hz);
        QVERIFY(masterRate);
        const auto stage=makeSyntheticStage(
            -7.0,expectedCeiling,0.51,guarded,nominal,*masterRate.value());
        QVERIFY(stage);
        const auto immutable=stage->rendered.terminal.terminal
            .compensated_pcm.view();
        QCOMPARE(immutable.format().sample_rate(),*masterRate.value());
        std::vector<std::uint64_t> originalBits;
        for(std::size_t ch=0;ch<2;++ch) {
            const auto lane=immutable.channel(ch);
            QVERIFY(lane);
            for(const double value:*lane.value())
                originalBits.push_back(std::bit_cast<std::uint64_t>(value));
        }
        auto conversion=audio::materialize_monitoring_pcm_candidate(
            immutable,*deviceRate.value(),*block.value());
        QVERIFY2(conversion,conversion?""
            :conversion.error()->message().c_str());
        conversion.value()->windows_prepared_endpoint_id=freshEndpoint.id;
        auto device=std::make_shared<const audio::MonitoringPcmCandidate>(
            std::move(*conversion.value()));
        const auto perRateTicket=admit_measured_device_audition_ticket(
            stage,stage,device,true,freshEndpoint);
        QVERIFY(perRateTicket);
        QCOMPARE(perRateTicket.value()->measured_master(),stage);
        QCOMPARE(perRateTicket.value()->audition_device_pcm(),device);
        QCOMPARE(device->device_rate,*deviceRate.value());
        QCOMPARE(device->immutable_master_rate,*masterRate.value());
        const auto expectedMasterFrames=(
            pcm.frame_count().value()*hz+sourceRate.value()->value()-1)
                /sourceRate.value()->value();
        QCOMPARE(immutable.frame_count().value(),expectedMasterFrames);
        QCOMPARE(device->immutable_master_frames.value(),expectedMasterFrames);
        QCOMPARE(stage->rendered.programme_frames,pcm.frame_count());
        QCOMPARE(stage->rendered.used_candidate_src,hz!=192000);
        if(hz==48000) {
            // The former fake six-rate suite passed a cross-rate stage
            // with used_candidate_src=false and no Q0 provenance, because
            // it incorrectly assigned equal SOURCE/MASTER frame counts.
            // Admission must now reject that forged crossrate witness.
            const auto forged=makeSyntheticStage(
                -7.0,expectedCeiling,0.51,guarded,nominal,
                *masterRate.value(),true);
            QVERIFY(forged);
            const auto forgedPcm=forged->rendered.terminal.terminal
                .compensated_pcm.view();
            auto forgedConversion=audio::materialize_monitoring_pcm_candidate(
                forgedPcm,*deviceRate.value(),*block.value());
            QVERIFY(forgedConversion);
            forgedConversion.value()->windows_prepared_endpoint_id
                =freshEndpoint.id;
            const auto forgedDevice=std::make_shared<
                const audio::MonitoringPcmCandidate>(
                    std::move(*forgedConversion.value()));
            QVERIFY(!admit_measured_device_audition_ticket(
                forged,forged,forgedDevice,true,freshEndpoint));
        }

        // Exercise all SIX rate-accurate SOURCE -> MASTER -> DEVICE
        // transport plans against the real rational frame-domain contract,
        // not just six independent PCM admission/conversion witnesses.
        // Same manual realization and SOURCE Loop must survive every rate.
        const auto sourceLoop=core::FrameRange::create(
            core::FrameIndex{64},core::FrameIndex{384});
        QVERIFY(sourceLoop);
        core::PlaybackSnapshot transport{};
        transport.state=core::PlaybackState::PLAYING;
        transport.position=core::FrameIndex{128};
        transport.duration=*frames.value();
        transport.loop=*sourceLoop.value();
        transport.audibleRealization={
            core::AudibleHandoffPhase::NEW,core::RealizationId{9001}};
        const auto plan=perRateTicket.value()->plan_handoff(
            stage,transport,*sourceLoop.value(),freshEndpoint,true);
        QVERIFY2(plan,plan?"":plan.error()->message().c_str());
        QCOMPARE(plan.value()->manual_owner,core::RealizationId{9001});
        QCOMPARE(plan.value()->source_rate,*sourceRate.value());
        QCOMPARE(plan.value()->master_rate,*masterRate.value());
        QCOMPARE(plan.value()->device_rate,*deviceRate.value());
        QCOMPARE(plan.value()->source_cue,core::FrameIndex{128});
        QCOMPARE(plan.value()->source_loop,std::optional{*sourceLoop.value()});
        QVERIFY(plan.value()->master_loop);
        QVERIFY(plan.value()->device_loop);
        const auto rational=audio::MonitoringFrameMap::create(*device);
        QVERIFY(rational);
        const auto mappedMaster=rational.value()->source_to_master(
            core::FrameIndex{128},*sourceRate.value(),*frames.value());
        QVERIFY(mappedMaster);
        const auto mappedDevice=rational.value()->master_to_device(
            *mappedMaster.value());
        QVERIFY(mappedDevice);
        QCOMPARE(plan.value()->master_cue,*mappedMaster.value());
        QCOMPARE(plan.value()->device_cue,*mappedDevice.value());
        const auto expectedMasterLoop=rational.value()->source_loop_to_master(
            *sourceLoop.value(),*sourceRate.value(),*frames.value());
        QVERIFY(expectedMasterLoop);
        const auto expectedDeviceLoop=rational.value()->master_loop_to_device(
            *expectedMasterLoop.value());
        QVERIFY(expectedDeviceLoop);
        QCOMPARE(plan.value()->master_loop,
                 std::optional{*expectedMasterLoop.value()});
        QCOMPARE(plan.value()->device_loop,
                 std::optional{*expectedDeviceLoop.value()});
        // Fake anonymous Windows DEVICE worker snapshot. It must report
        // exactly one DEVICE-domain timebase and MUST NOT acquire M16 LIVE
        // or a verified audible RealizationId. The immutable measured
        // MASTER remains separate across all six physical-rate candidates.
        core::PlaybackSnapshot fakeDevice{};
        fakeDevice.state=core::PlaybackState::STOPPED;
        fakeDevice.position=plan.value()->device_cue;
        fakeDevice.duration=device->device_pcm.view().frame_count();
        fakeDevice.loop=plan.value()->device_loop;
        fakeDevice.audibleRealization={
            core::AudibleHandoffPhase::UNAVAILABLE,std::nullopt};
        for(const auto state: {
            core::PlaybackState::STOPPED,
            core::PlaybackState::PLAYING,
            core::PlaybackState::PAUSED}) {
            fakeDevice.state=state;
            const auto displayed=rational.value()->project_display_snapshot(
                fakeDevice,plan.value()->master_loop);
            QVERIFY(displayed);
            QCOMPARE(displayed.value()->state,state);
            QCOMPARE(displayed.value()->device_position,plan.value()->device_cue);
            QCOMPARE(displayed.value()->master_duration,
                     device->immutable_master_frames);
            QCOMPARE(displayed.value()->device_duration,
                     device->device_pcm.view().frame_count());
            QCOMPARE(displayed.value()->master_rate,*masterRate.value());
            QCOMPARE(displayed.value()->device_rate,*deviceRate.value());
            QCOMPARE(displayed.value()->requested_master_loop,
                     plan.value()->master_loop);
        }
        auto foreignDevice=fakeDevice;
        foreignDevice.audibleRealization={
            core::AudibleHandoffPhase::NEW,core::RealizationId{9999}};
        QVERIFY(!rational.value()->project_display_snapshot(
            foreignDevice,plan.value()->master_loop));
        auto changedDeviceLoop=fakeDevice;
        changedDeviceLoop.loop.reset();
        QVERIFY(!rational.value()->project_display_snapshot(
            changedDeviceLoop,plan.value()->master_loop));
        auto changedDeviceDuration=fakeDevice;
        changedDeviceDuration.duration=*shortCount.value();
        QVERIFY(!rational.value()->project_display_snapshot(
            changedDeviceDuration,plan.value()->master_loop));
        auto stoppedNoSource=fakeDevice;
        stoppedNoSource.state=core::PlaybackState::NO_SOURCE;
        QVERIFY(!rational.value()->project_display_snapshot(
            stoppedNoSource,plan.value()->master_loop));
        // The inverse MASTER loop projection must accept the same
        // rounding-correct programme duration and never invent a SOURCE
        // frame outside the real MANUAL owner. Different sample grids
        // cannot promise bit-identical loop boundaries after round trips.
        const auto backToSource=rational.value()->master_loop_to_source(
            *expectedMasterLoop.value(),*sourceRate.value(),*frames.value());
        QVERIFY2(backToSource,backToSource?""
            :backToSource.error()->message().c_str());
        QVERIFY(backToSource.value()->begin()>=core::FrameIndex{0});
        QVERIFY(backToSource.value()->end()
            <=core::FrameIndex{frames.value()->value()});
        QVERIFY(backToSource.value()->begin()<backToSource.value()->end());
        if(hz==192000)
            QCOMPARE(*backToSource.value(),*sourceLoop.value());
        // Full-range Loop must survive the original SOURCE endpoint even
        // when the final rounded MASTER frame projects *past* the raw
        // SOURCE end at 192->48 (129 MASTER frames project to 516 SOURCE,
        // but the true original end is SOURCE frame 513).
        const auto fullMasterRegion=core::FrameRange::create(
            core::FrameIndex{0},
            core::FrameIndex{device->immutable_master_frames.value()});
        QVERIFY(fullMasterRegion);
        const auto fullSourceReturn=rational.value()->master_loop_to_source(
            *fullMasterRegion.value(),*sourceRate.value(),*frames.value());
        QVERIFY(fullSourceReturn);
        QCOMPARE(fullSourceReturn.value()->begin(),core::FrameIndex{0});
        QCOMPARE(fullSourceReturn.value()->end(),
                 core::FrameIndex{frames.value()->value()});
        const auto wrongSourceCount=core::FrameCount::create(700);
        QVERIFY(wrongSourceCount);
        QVERIFY(!rational.value()->source_to_master(
            core::FrameIndex{0},*sourceRate.value(),*wrongSourceCount.value()));
        QVERIFY(!rational.value()->master_loop_to_source(
            *expectedMasterLoop.value(),*sourceRate.value(),
            *wrongSourceCount.value()));
        auto rateAdvanced=transport;
        rateAdvanced.position=core::FrameIndex{132};
        const auto rebased=perRateTicket.value()->rebase_handoff(
            *plan.value(),stage,rateAdvanced,*sourceLoop.value(),
            freshEndpoint,true);
        QVERIFY(rebased);
        QCOMPARE(rebased.value()->source_cue,core::FrameIndex{132});
        auto seekChanged=rateAdvanced;
        seekChanged.seekSerial+=1;
        QVERIFY(!perRateTicket.value()->rebase_handoff(
            *plan.value(),stage,seekChanged,*sourceLoop.value(),
            freshEndpoint,true));
        auto loopWrap=rateAdvanced;
        loopWrap.loopWrapCount+=1;
        QVERIFY(!perRateTicket.value()->rebase_handoff(
            *plan.value(),stage,loopWrap,*sourceLoop.value(),
            freshEndpoint,true));
        auto ratePaused=transport;
        ratePaused.state=core::PlaybackState::PAUSED;
        QVERIFY(perRateTicket.value()->plan_handoff(
            stage,ratePaused,*sourceLoop.value(),freshEndpoint,true));
        auto rateStopped=transport;
        rateStopped.state=core::PlaybackState::STOPPED;
        QVERIFY(perRateTicket.value()->plan_handoff(
            stage,rateStopped,*sourceLoop.value(),freshEndpoint,true));

        QCOMPARE(device->device_src_applied,hz!=48000);
        // A fresh owner-thread check must accept only the SAME measured
        // stage, MANUAL realization, DEVICE rate and physical endpoint.
        QVERIFY(perRateTicket.value()->recheck_preinstall(
            stage,core::RealizationId{9001},freshEndpoint,true));
        QVERIFY(!perRateTicket.value()->recheck_preinstall(
            stage,core::RealizationId{9001},freshEndpoint,false));
        QVERIFY(!perRateTicket.value()->recheck_preinstall(
            {},core::RealizationId{9001},freshEndpoint,true));
        QVERIFY(!perRateTicket.value()->recheck_preinstall(
            stage,core::RealizationId{9002},freshEndpoint,true));
        // Same physical rate but a different endpoint is still stale.
        const app::MeasuredDeviceEndpointObservation hotplug{
            "fixture-endpoint-B",*deviceRate.value()};
        QVERIFY(!admit_measured_device_audition_ticket(
            stage,stage,device,true,hotplug));
        QVERIFY(!perRateTicket.value()->recheck_preinstall(
            stage,core::RealizationId{9001},hotplug,true));
        const app::MeasuredDeviceEndpointObservation rateChanged{
            freshEndpoint.id,*sourceRate.value()};
        QVERIFY(!admit_measured_device_audition_ticket(
            stage,stage,device,true,rateChanged));
        QVERIFY(!perRateTicket.value()->recheck_preinstall(
            stage,core::RealizationId{9001},rateChanged,true));
        QVERIFY(!admit_measured_device_audition_ticket(
            stage,stage,device,false,freshEndpoint));
        std::vector<std::uint64_t> afterBits;
        for(std::size_t ch=0;ch<2;++ch) {
            const auto lane=immutable.channel(ch);
            QVERIFY(lane);
            for(const double value:*lane.value())
                afterBits.push_back(std::bit_cast<std::uint64_t>(value));
        }
        QCOMPARE(afterBits,originalBits);
        ++measuredCases;
    }
    QCOMPARE(measuredCases,std::size_t{6});
    qInfo().noquote()
        <<"M17_MEASURED_NONCONFORMING_6_TO_48K_OFFLINE_ADMISSION=6";
}

void TerminalActivationIntegrationTest::audibleOnBypassRestoresExactManualSourceAndLoop()
{
    using namespace rgsml;
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto chain=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto previewChainId=*core::Uuid::parse(
        "7f000000-0000-4000-8000-000000000418").value();
    const auto gainId=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("7f000000-0000-4000-8000-000000000419").value());
    const auto eqId=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("7f000000-0000-4000-8000-000000000420").value());
    QVERIFY(gainId&&eqId);
    auto upstream=app::MasteringChainState::create_default(
        *registry.value(),previewChainId,*gainId.value(),*eqId.value());
    QVERIFY(upstream);
    auto limiterId=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("7f000000-0000-4000-8000-000000000417").value());
    QVERIFY(limiterId);
    auto limiter=app::TerminalLimiterState::create_default(
        *registry.value(),*limiterId.value());
    QVERIFY(limiter);
    app::TerminalLimiterViewModel vm{limiter.value()};
    const auto sourceRate=core::SampleRate::create(44100);
    const auto deviceRate=core::SampleRate::create(48000);
    const auto frames=core::FrameCount::create(4410);
    const auto block=core::FrameCount::create(257);
    QVERIFY(sourceRate&&deviceRate&&frames&&block);
    const auto format=audio::AudioFormat::create(
        *sourceRate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(format);
    auto source=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(source);
    // Real non-zero PCM: an Input Gain edit must change the accepted
    // MANUAL samples, not merely swap two identical zero-filled owners.
    for(std::size_t ch=0;ch<2;++ch) {
        auto lane=source.value()->mutable_view().channel(ch);
        QVERIFY(lane);
        for(auto& sample:*lane.value())
            sample=ch==0 ? 0.125 : -0.0625;
    }
    const auto renderPrepared=[&]() -> render::RenderResult {
        auto request=render::RenderRequest::create(
            source.value()->view(),source.value()->view().absolute_range(),
            *chain.value(),{},*block.value());
        Q_ASSERT(request);
        auto output=render::render_preview(*request.value(),*registry.value());
        Q_ASSERT(output);
        return std::move(*output.value());
    };

    const auto renderManual=[&]() -> render::RenderResult {
        auto request=render::RenderRequest::create(
            source.value()->view(),source.value()->view().absolute_range(),
            upstream.value()->chain(),upstream.value()->execution_bindings(),
            *block.value());
        Q_ASSERT(request);
        auto output=render::render_preview(*request.value(),*registry.value());
        Q_ASSERT(output);
        return std::move(*output.value());
    };

    auto service=std::make_unique<FakePlaybackService>();
    auto* device=service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    transport.set_pcm_prepare_handler(
        [device](audio::AudioBufferView view,
            std::shared_ptr<const void> lease,
            std::optional<core::RealizationId> id)->core::Status {
            if(!lease)return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"Missing MANUAL PCM lease."});
            device->state=core::PlaybackState::STOPPED;
            device->position=view.absolute_start_frame();
            device->duration=view.frame_count();
            device->loop.reset();
            device->audibleRealization={
                core::AudibleHandoffPhase::NEW,id};
            return core::Status::success();
        });
    // Manual replacement uses a separate no-terminal SOURCE handoff seam.
    transport.set_pcm_handoff_handler(
        [device](audio::AudioBufferView view,
            std::shared_ptr<const void> lease,
            std::optional<core::RealizationId> id)->core::Status {
            if(!lease)return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,"Missing successor MANUAL lease."});
            device->state=core::PlaybackState::STOPPED;
            device->position=view.absolute_start_frame();
            device->duration=view.frame_count();
            device->loop.reset();
            device->audibleRealization={core::AudibleHandoffPhase::NEW,id};
            return core::Status::success();
        });
    app::AuditionSourceSelector selector{&transport};
    const auto region=core::FrameRange::create(
        core::FrameIndex{500},core::FrameIndex{3500});
    QVERIFY(region);
    std::optional<core::FrameRange> desiredLoop=*region.value();
    selector.set_source_loop_provider([&]{return desiredLoop;});
    QVERIFY(selector.set_prepared_realization(renderPrepared()));
    QVERIFY(selector.set_processed_realization(renderManual()));
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    const auto original=selector.processed_realization_snapshot();
    const auto originalId=selector.processed_realization_id();
    QVERIFY(original&&originalId);
    QCOMPARE(device->loop,std::optional{*region.value()});
    QVERIFY(transport.seek_source_frame(core::FrameIndex{1450}));
    transport.playOrResume();
    QCOMPARE(device->state,core::PlaybackState::PLAYING);

    app::TerminalCandidateGate candidates;
    app::TerminalAudioSessionGate audioGate{
        candidates,*device,
        [device,deviceRate](audio::AudioBufferView pcm,
            std::shared_ptr<const void> lease,
            core::RealizationId id)->core::Status {
            if(!lease||id.value==0
               ||pcm.format().sample_rate()!=*deviceRate.value()
               ||pcm.timebase().frame_domain_id()
                    !=audio::FrameDomainId::OUTPUT_RATE)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,"Unowned OUTPUT_RATE M17 PCM."});
            device->state=core::PlaybackState::STOPPED;
            device->position=core::FrameIndex{0};
            device->duration=pcm.frame_count();
            device->loop.reset();
            device->audibleRealization={
                core::AudibleHandoffPhase::NEW,id};
            return core::Status::success();
        }};
    app::TerminalPrivatePublicationCoordinator publisher{
        candidates,audioGate,transport,
        [&selector](std::shared_ptr<const render::RenderResult> prepared,
            std::shared_ptr<const render::RenderResult> manual,
            std::optional<core::RealizationId> id) {
            return selector.reserve_private_terminal_realization_id(
                prepared,manual,id);
        }};
    app::TerminalPrivateRenderController renderer{
        candidates,*limiter.value(),
        [&selector]{return selector.prepared_realization_snapshot();},
        [&selector]{return selector.processed_realization_snapshot();},
        [&selector]{return selector.processed_realization_id();},
        [&]{return upstream.value()->chain();}};
    // Private original 14 Q0 files are intentionally NOT present in
    // CI. The one-time folder enrollment must refuse network URLs and
    // directories without the actual source-pinned FIR family.
    QVERIFY(!renderer.full_original_q0_family_ready());
    QCOMPARE(renderer.property("originalQ0Ready").toBool(),false);
    QVERIFY(!renderer.remember_existing_original_q0_folder(
        QUrl{QStringLiteral("https://invalid.example/private-firs")}));
    QVERIFY(!renderer.remember_existing_original_q0_folder(
        QUrl::fromLocalFile(QDir::tempPath())));
    QVERIFY(!renderer.full_original_q0_family_ready());
    QVERIFY(renderer.original_q0_status().contains(
        QStringLiteral("not the verified original")));
    // Negative enrollment checks above require NO original family.
    // Positive 44.1->48 activation below now obeys exactly the SAME
    // 14-file original Q0 admission as all other nonidentity rates.
    // Load the genuine pinned repository assets, never stub the policy
    // or invent coefficients to force an artificial READY.
    // Source is tests/unit/app/<fixture>.cpp: four parents reach the
    // repository root, not tests/. Never weaken the source SHA admission.
    const auto repoRoot=std::filesystem::path{__FILE__}
        .parent_path().parent_path().parent_path().parent_path();
    renderer.set_original_q0_fir_root(repoRoot/"resources"/"fir");
    QVERIFY(renderer.full_original_q0_family_ready());
    app::TerminalPrivateProductSession product{
        renderer,publisher,candidates,transport,
        [&selector]{return selector.prepared_realization_snapshot();},
        [&selector]{return selector.processed_realization_snapshot();},
        [&selector]{return selector.processed_realization_id();},
        [&]{return desiredLoop;},
        [&selector]{return selector.active_target()
            ==app::AuditionTarget::PROCESSED;}};
    product.set_preferred_output_rate_probe(
        [deviceRate](audio::ChannelLayout)->core::Result<core::SampleRate>{
            return core::Result<core::SampleRate>::success(*deviceRate.value());
        });
    app::TerminalProductActivationCoordinator coordinator{
        *limiter.value(),vm,selector,transport,product};
    // Mirror the REAL app/src/main.cpp signal hookup. The historical test
    // manually fed RENDERING/READY and could not detect a broken preview
    // signal, asynchronous publication or upstream gain revision.
    app::MasteringPreviewController preview{
        upstream.value(),
        [&selector]{return selector.prepared_realization_snapshot();},
        [&selector](render::RenderResult output) {
            return selector.set_processed_realization(std::move(output));
        }};
    QObject::connect(&preview,&app::MasteringPreviewController::changed,
                     &coordinator,[&preview,&coordinator] {
        coordinator.manual_preview_changed(preview.preview_status());
    });
    QVERIFY(limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("BYPASS"));
    vm.setBypass(false);
    // Armed is NOT preparing. Play still uses the exact upstream MANUAL PCM.
    QVERIFY(!limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QVERIFY(!coordinator.device_active());
    vm.requestPrepare();
    QCOMPARE(vm.audio_status(),QStringLiteral("Preparing audio"));
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    // Completed rendering stays PRIVATE; no automatic switch on Play or ON.
    device->position=core::FrameIndex{1600};
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    QCOMPARE(vm.preparation_fraction(),1.0);
    QVERIFY(vm.preparation_determinate());
    QCOMPARE(vm.preparation_stage(),QStringLiteral("Verified MASTER ready"));
    const auto initialPreparedPcm=candidates.staged();
    QVERIFY(initialPreparedPcm);
    // A verified MASTER is not a measured nonconforming candidate.
    // No fabricated unsafe direct-preview availability.
    QVERIFY(product.current_master_matches_device_for_preview());
    QVERIFY(!product.current_measured_matches_device_for_preview());
    // Strongly typed unqualified admission must reject even a genuine
    // VERIFIED M17 MASTER: this code cannot borrow LIVE/GR authority.
    QVERIFY(!product.capture_measured_master_for_device_conversion());
    // M16-VERIFIED MASTER must NEVER acquire a measured anonymous sink
    // merely because both use the Windows monitoring adapter.
    QVERIFY(!product.measured_device_active());
    QVERIFY(!product.measured_device_snapshot());
    QVERIFY(!product.return_measured_device_audition());
    QVERIFY(!product.control_measured_device_audition({
        platform::windows::MonitoringTransportAction::PLAY}));
    const app::MeasuredDeviceEndpointObservation neverAdmit{
        "fixture-endpoint-A",*deviceRate.value()};
    const auto forbidden=product.admit_measured_device_conversion(
        {},{},true,neverAdmit);
    QVERIFY(!forbidden);
    // The pre-install boundary itself cannot be borrowed by a VERIFIED
    // MASTER (nor can it create a measured candidate from empty input).
    // Negative test only: there is no measured nonconforming ticket here.
    // A worker may compute FIR from captured immutable PCM, but it may
    // never observe or modify QObject session state. Admission must be
    // performed again on the owning Qt thread after conversion finishes.
    bool workerCaptureDenied=false;
    bool workerAdmissionDenied=false;
    std::thread foreignOwner([&] {
        workerCaptureDenied=
            !product.capture_measured_master_for_device_conversion();
        const auto wrongThread=product.admit_measured_device_conversion(
            {},{},true,app::MeasuredDeviceEndpointObservation{
            "fixture-endpoint-A",*deviceRate.value()});
        workerAdmissionDenied=!wrongThread
            &&wrongThread.error()->code()==core::ErrorCode::InvalidState;
    });
    foreignOwner.join();
    QVERIFY(workerCaptureDenied);
    QVERIFY(workerAdmissionDenied);
    // P0 bridge returns ONLY the exact currently verified MASTER at the
    // selected PROJECT rate. DEVICE must not implicitly retarget it.
    const auto firstRenderCount=renderer.render_invocation_count();
    QCOMPARE(firstRenderCount,std::uint64_t{1});
    QCOMPARE(product.borrow_verified_m17_master_for_device(
        *deviceRate.value()),initialPreparedPcm);
    QVERIFY(!product.borrow_verified_m17_master_for_device(
        *sourceRate.value()));
    QCOMPARE(renderer.render_invocation_count(),firstRenderCount);
    // Real distinct-rate DEVICE PCM from the previously completed M17
    // master, without ever invoking another M17/M16 whole-resource job.
    const auto offlineBlock=core::FrameCount::create(
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    QVERIFY(offlineBlock);
    std::size_t borrowedSrcCalls=0;
    const app::TerminalPrivateProductSession::MonitoringMaterializer
        convertedFromExisting=[&](
            audio::AudioBufferView verifiedPcm)
            ->core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>> {
            using Owned=std::shared_ptr<const audio::MonitoringPcmCandidate>;
            using Result=core::Result<Owned>;
            ++borrowedSrcCalls;
            if(verifiedPcm.format().sample_rate()!=*deviceRate.value())
                return Result::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M17_REUSE_TEST_WRONG_VERIFIED_PCM_RATE"});
            auto converted=audio::materialize_monitoring_pcm_candidate(
                verifiedPcm,*sourceRate.value(),*offlineBlock.value());
            if(!converted)return Result::failure(*converted.error());
            return Result::success(
                std::make_shared<const audio::MonitoringPcmCandidate>(
                    std::move(*converted.value())));
        };
    auto wrongMaster=product.materialize_borrowed_verified_m17_monitoring(
        *sourceRate.value(),convertedFromExisting);
    QVERIFY(!wrongMaster);
    QCOMPARE(borrowedSrcCalls,std::size_t{0});
    auto exactReuse=product.materialize_borrowed_verified_m17_monitoring(
        *deviceRate.value(),convertedFromExisting);
    QVERIFY2(exactReuse,exactReuse?"":exactReuse.error()->message().c_str());
    QCOMPARE(borrowedSrcCalls,std::size_t{1});
    QCOMPARE((*exactReuse.value())->immutable_master_rate,*deviceRate.value());
    QCOMPARE((*exactReuse.value())->device_rate,*sourceRate.value());
    QCOMPARE((*exactReuse.value())->immutable_master_frames.value(),
             std::int64_t{4800});
    QCOMPARE((*exactReuse.value())->device_pcm.view().frame_count().value(),
             std::int64_t{4410});
    // A block one frame above Playback SRC's hard maximum must fail
    // before creating DEVICE PCM; it must not invalidate this MASTER.
    const auto oversizedBlock=core::FrameCount::create(
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames+1);
    QVERIFY(oversizedBlock);
    auto oversized=audio::materialize_monitoring_pcm_candidate(
        initialPreparedPcm->rendered.terminal.checked.terminal
            .compensated_pcm.view(),
        *sourceRate.value(),*oversizedBlock.value());
    QVERIFY(!oversized);
    QCOMPARE(oversized.error()->code(),core::ErrorCode::InvalidArgument);
    QCOMPARE(candidates.staged(),initialPreparedPcm);
    QCOMPARE(renderer.render_invocation_count(),firstRenderCount);
    // The P0 borrowed MASTER must also route through the existing Windows
    // admission callback, including an explicit memory policy. The fake
    // observation proves the wired contract; not physical RAM evidence.
    constexpr std::uint64_t kBorrowedReserve=UINT64_C(32)*1024*1024;
    const auto acceptBorrowed=[](
        const audio::MonitoringPcmPreallocationAdvice& advice)
        ->core::Status {
        if(advice.kind!=audio::MonitoringMemoryAdviceKind::RECOMMENDED_MEMORY_ONLY)
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "M17_REUSE_EXPECTS_TEST_RECOMMENDED_MEMORY"});
        return core::Status::success();
    };
    auto preflightMissing=product
        .materialize_borrowed_verified_m17_monitoring_with_preflight(
            *deviceRate.value(),*offlineBlock.value(),kBorrowedReserve,
            acceptBorrowed);
    QVERIFY(!preflightMissing);
    std::size_t borrowedPreflightCalls=0;
    std::size_t borrowedPolicyCalls=0;
    product.set_monitoring_preflight_materializer(
        [&](audio::AudioBufferView input,core::FrameCount requestedBlock,
            std::uint64_t otherReserve,
            const app::TerminalPrivateProductSession::
                MonitoringPreallocationDecision& policy)
            ->core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>> {
            using Result=core::Result<
                std::shared_ptr<const audio::MonitoringPcmCandidate>>;
            ++borrowedPreflightCalls;
            if(requestedBlock!=*offlineBlock.value()
               ||otherReserve!=kBorrowedReserve)
                return Result::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M17_REUSE_TEST_PREFLIGHT_PARAMETERS_CHANGED"});
            auto recommendation=audio::advise_imminent_monitoring_pcm_allocation(
                input.format().sample_rate(),input.frame_count(),
                *sourceRate.value(),input.format().channel_layout(),
                audio::MonitoringMemoryObservation{
                    UINT64_C(4)*1024*1024*1024,
                    UINT64_C(5)*1024*1024*1024,
                    otherReserve});
            if(!recommendation)return Result::failure(*recommendation.error());
            ++borrowedPolicyCalls;
            const auto allowed=policy(*recommendation.value());
            if(!allowed)return Result::failure(*allowed.error());
            return convertedFromExisting(input);
        });
    auto refusedByPolicy=product
        .materialize_borrowed_verified_m17_monitoring_with_preflight(
            *deviceRate.value(),*offlineBlock.value(),kBorrowedReserve,
            [](const audio::MonitoringPcmPreallocationAdvice&)->core::Status {
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M17_REUSE_EXPLICIT_POLICY_REJECTS"});
            });
    QVERIFY(!refusedByPolicy);
    QCOMPARE(borrowedPreflightCalls,std::size_t{1});
    QCOMPARE(borrowedPolicyCalls,std::size_t{1});
    QCOMPARE(borrowedSrcCalls,std::size_t{1});
    auto allowedByPolicy=product
        .materialize_borrowed_verified_m17_monitoring_with_preflight(
            *deviceRate.value(),*offlineBlock.value(),kBorrowedReserve,
            acceptBorrowed);
    QVERIFY2(allowedByPolicy,allowedByPolicy?""
        :allowedByPolicy.error()->message().c_str());
    QCOMPARE(borrowedPreflightCalls,std::size_t{2});
    QCOMPARE(borrowedPolicyCalls,std::size_t{2});
    QCOMPARE(borrowedSrcCalls,std::size_t{2});
    QCOMPARE(renderer.render_invocation_count(),firstRenderCount);
    QCOMPARE(candidates.staged(),initialPreparedPcm);
    // Encode WAV directly from the SAME staged immutable verified PCM
    // used by the DEVICE SRC test. Encoding must not invoke M17 again.
    // This is an isolated product-owner test seam, NOT yet export UI.
    const auto immutableWavMaster=initialPreparedPcm->rendered.terminal.checked
        .terminal.compensated_pcm.view();
    auto reusedMasterWav=std::make_shared<MatrixWavBytes>();
    const audio::WavWriteSpec reusedWavSpec{
        immutableWavMaster.format(),immutableWavMaster.frame_count(),
        audio::WavSampleFormat::IEEE_F64};
    auto reusableWriter=audio::WavWriter::open(
        std::make_unique<MatrixWavSink>(reusedMasterWav),reusedWavSpec);
    QVERIFY(reusableWriter);
    QVERIFY((*reusableWriter.value())->write_frames(immutableWavMaster));
    QVERIFY((*reusableWriter.value())->finalize());
    QVERIFY((*reusableWriter.value())->close());
    QCOMPARE(matrix_wav_u32(reusedMasterWav->bytes,24),
        std::uint32_t{48000});
    QCOMPARE(matrix_wav_u32(reusedMasterWav->bytes,54),
        std::uint32_t{4800*2*8});
    QCOMPARE(candidates.staged(),initialPreparedPcm);
    QCOMPARE(renderer.render_invocation_count(),firstRenderCount);
    QVERIFY(!product.private_id());
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QSignalSpy renderChanges{&renderer,&app::TerminalPrivateRenderController::changed};
    QVERIFY(renderChanges.isValid());
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    qInfo().noquote()<<"M17_EDGE_CASE=READY_BYPASS_RETAINS_EXACT_PCM";
    vm.setBypass(true);
    QCOMPARE(product.status(),QStringLiteral("COMPARISON_VERIFIED"));
    QCOMPARE(candidates.staged(),initialPreparedPcm);
    QCOMPARE(product.borrow_verified_m17_master_for_device(
        *deviceRate.value()),initialPreparedPcm);
    QCOMPARE(renderer.render_invocation_count(),firstRenderCount);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QVERIFY(!audioGate.has_session());
    vm.previewPrepared();
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    vm.setBypass(false);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(candidates.staged(),initialPreparedPcm);
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    vm.previewPrepared();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QVERIFY(coordinator.device_active());
    // The physical FQ #96 showed direct M17 audible playback with no
    // Return to Source affordance. Test the explicit exact-owner action,
    // not a BYPASS proxy or MONITOR cancel.
    QVERIFY(coordinator.direct_return_available());
    QCOMPARE(device->audibleRealization.phase,
             core::AudibleHandoffPhase::NEW);
    QVERIFY(device->audibleRealization.realizationId!=originalId);
    const auto preReturnRenderCount=renderer.render_invocation_count();
    const auto exactPrivateMaster=candidates.staged();
    QVERIFY(coordinator.returnToSource());
    QVERIFY(!coordinator.direct_return_available());
    QVERIFY(!coordinator.device_active());
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(device->loop,std::optional{*region.value()});
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(candidates.staged(),exactPrivateMaster);
    QCOMPARE(renderer.render_invocation_count(),preReturnRenderCount);
    QVERIFY(!coordinator.returnToSource()); // No direct lease left.
    QVERIFY(!limiter.value()->bypassed()); // Return is not a bypass flip.
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    vm.previewPrepared();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QVERIFY(coordinator.direct_return_available());
    QCOMPARE(renderer.render_invocation_count(),preReturnRenderCount);
    // INSTALLED -> Master Audition requires explicit exact-owner retirement.
    // The user starts audition directly, not a second whole-resource Prepare.
    // This fixture intentionally lacks DEVICE-monitor preflight: the
    // handoff must still safely retain the MASTER when allocation is denied.
    app::TerminalMonitorViewModel handoffUi{product,transport};
    handoffUi.set_installed_m17_handoff(
        [&coordinator] {
            return coordinator.can_handoff_installed_to_monitor();
        },
        [&coordinator] {
            return coordinator.handoff_installed_to_monitor();
        },
        [&coordinator] {
            coordinator.restore_ready_after_monitor_return();
        });
    const auto verifiedBeforeHandoff=candidates.staged();
    const auto renderBeforeHandoff=renderer.render_invocation_count();
    QVERIFY(handoffUi.can_install());
    // The preflight is configured, but this fixture intentionally does not
    // install an independent monitoring output. The failed command must
    // recover M17 READY (not a misleading NEEDS PREPARATION) and retain the
    // exact original checked PCM for a retry without another M17 render.
    QVERIFY(!handoffUi.installMonitor(false));
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    QCOMPARE(candidates.staged(),verifiedBeforeHandoff);
    QCOMPARE(product.verified_master_rate_for_monitoring(),
             std::optional{*deviceRate.value()});
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QVERIFY(!coordinator.device_active());
    QVERIFY(handoffUi.can_install()); // retry allowed without reprepare
    QCOMPARE(renderer.render_invocation_count(),renderBeforeHandoff);
    // One-button recovery: the same M17 candidate must be previewable
    // directly after DEVICE admission refusal, with no compensating
    // BYPASS flip and no second full MASTER Prepare.
    QVERIFY(!limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    vm.previewPrepared();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QVERIFY(coordinator.device_active());
    QCOMPARE(candidates.staged(),verifiedBeforeHandoff);
    QCOMPARE(renderer.render_invocation_count(),renderBeforeHandoff);
    QVERIFY(handoffUi.can_install()); // new explicit owner retirement required
    QCOMPARE(renderer.render_invocation_count(),renderBeforeHandoff);
    // MAIN TRANSPORT POSITIVE PATH: explicit installed M17 Preview -> SAME
    // verified MASTER -> independent 44.1-kHz DEVICE PCM -> owner-exact Return.
    // A new Windows sink owns the audition frames, not M17's 48-kHz MASTER;
    // no second Prepare or new SOURCE/terminal DSP RealizationId is minted.
    const auto positivePreflightRenderCount=renderer.render_invocation_count();
    const auto positiveMasterOwner=candidates.staged();
    QVERIFY(positiveMasterOwner);
    std::size_t positivePreflightCount=0;
    std::shared_ptr<const audio::MonitoringPcmCandidate> activeMasterDevice;
    std::optional<core::FrameRange> requestedMasterLoop;
    product.set_monitoring_preflight_materializer(
        [&](audio::AudioBufferView view,core::FrameCount block,
            std::uint64_t reserved,
            const app::TerminalPrivateProductSession::
                MonitoringPreallocationDecision& decision)
            ->core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>> {
            using Result=core::Result<
                std::shared_ptr<const audio::MonitoringPcmCandidate>>;
            ++positivePreflightCount;
            if(block!=*offlineBlock.value()||reserved!=0)
                return Result::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "MAIN_MASTER_TEST_WRONG_MONITOR_BLOCK_OR_RESERVATION"});
            auto advice=audio::advise_imminent_monitoring_pcm_allocation(
                view.format().sample_rate(),view.frame_count(),
                *sourceRate.value(),view.format().channel_layout(),
                audio::MonitoringMemoryObservation{
                    UINT64_C(4)*1024*1024*1024,
                    UINT64_C(5)*1024*1024*1024,reserved});
            if(!advice)return Result::failure(*advice.error());
            const auto allowed=decision(*advice.value());
            if(!allowed)return Result::failure(*allowed.error());
            return convertedFromExisting(view);
        });
    product.set_private_monitor_transport(
        [device,&activeMasterDevice](auto candidate)->core::Status {
            if(!candidate||activeMasterDevice)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "MAIN_MASTER_DEVICE_ALREADY_OWNED"});
            activeMasterDevice=candidate;
            device->state=core::PlaybackState::STOPPED;
            device->position=core::FrameIndex{0};
            device->duration=candidate->device_pcm.view().frame_count();
            device->loop.reset();
            device->audibleRealization={
                core::AudibleHandoffPhase::UNAVAILABLE,std::nullopt};
            return core::Status::success();
        },
        [device,&activeMasterDevice](auto candidate)->core::Result<bool> {
            return core::Result<bool>::success(candidate
                &&activeMasterDevice==candidate
                &&device->state!=core::PlaybackState::NO_SOURCE
                &&device->duration==std::optional{
                    candidate->device_pcm.view().frame_count()}
                &&device->audibleRealization.phase
                    ==core::AudibleHandoffPhase::UNAVAILABLE
                &&!device->audibleRealization.realizationId);
        },
        [device,&activeMasterDevice](auto candidate)->core::Status {
            if(!candidate||candidate!=activeMasterDevice)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "MAIN_MASTER_DEVICE_RETIRE_OWNER_MISMATCH"});
            activeMasterDevice.reset();
            return device->clear();
        },
        [&selector](auto manual,core::RealizationId id,
            core::FrameIndex cue)->core::Status {
            return selector.restore_manual_processed_after_terminal(
                std::move(manual),id,cue);
        });
    product.set_private_monitor_controls(
        [device,&activeMasterDevice,&requestedMasterLoop]
            (auto owner,auto command)->core::Status {
            if(!owner||owner!=activeMasterDevice)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "MAIN_MASTER_TRANSPORT_OWNER_MISMATCH"});
            auto map=audio::MonitoringFrameMap::create(*owner);
            if(!map)return core::Status::failure(*map.error());
            using Action=platform::windows::MonitoringTransportAction;
            switch(command.action) {
            case Action::PLAY:return device->play();
            case Action::PAUSE:return device->pause();
            case Action::STOP:return device->stop();
            case Action::SEEK_MASTER: {
                auto mapped=map.value()->master_to_device(command.master_frame);
                if(!mapped)return core::Status::failure(*mapped.error());
                return device->seek(*mapped.value());
            }
            case Action::SET_MASTER_LOOP: {
                std::optional<core::FrameRange> loop;
                if(command.master_loop) {
                    auto mapped=map.value()->master_loop_to_device(
                        *command.master_loop);
                    if(!mapped)return core::Status::failure(*mapped.error());
                    loop=*mapped.value();
                }
                const auto status=device->set_loop(loop);
                if(status)requestedMasterLoop=command.master_loop;
                return status;
            }
            }
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidArgument,
                "MAIN_MASTER_UNKNOWN_TRANSPORT_COMMAND"});
        },
        [device,&activeMasterDevice,&requestedMasterLoop](auto owner)
            ->core::Result<audio::MonitoringMasterDisplaySnapshot> {
            using Snapshot=audio::MonitoringMasterDisplaySnapshot;
            if(!owner||owner!=activeMasterDevice)
                return core::Result<Snapshot>::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "MAIN_MASTER_DISPLAY_OWNER_MISMATCH"});
            auto map=audio::MonitoringFrameMap::create(*owner);
            if(!map)return core::Result<Snapshot>::failure(*map.error());
            auto playback=device->snapshot();
            if(!playback)return core::Result<Snapshot>::failure(
                *playback.error());
            return map.value()->project_display_snapshot(
                *playback.value(),requestedMasterLoop);
        });
    QVERIFY(handoffUi.can_install());
    QVERIFY(handoffUi.installMonitor(false));
    QVERIFY(handoffUi.monitor_exclusive());
    QVERIFY(handoffUi.monitor_controllable());
    QCOMPARE(product.status(),QStringLiteral("MONITOR_PRIVATE_PLAYING"));
    QCOMPARE(positivePreflightCount,std::size_t{1});
    QCOMPARE(renderer.render_invocation_count(),positivePreflightRenderCount);
    QCOMPARE(candidates.staged(),positiveMasterOwner);
    QVERIFY(activeMasterDevice);
    QCOMPARE(activeMasterDevice->immutable_master_rate,*deviceRate.value());
    QCOMPARE(activeMasterDevice->device_rate,*sourceRate.value());
    QVERIFY(!device->audibleRealization.realizationId);
    QVERIFY(!coordinator.device_active());
    // The main physical transport drives MASTER-specific controls through
    // the existing typed monitor VM; SOURCE cannot be silently selected.
    QVERIFY(handoffUi.pause());
    QCOMPARE(handoffUi.transport_state(),QStringLiteral("PAUSED"));
    QVERIFY(handoffUi.play());
    QCOMPARE(handoffUi.transport_state(),QStringLiteral("PLAYING"));
    // Close the exact independent DEVICE owner and restore the MANUAL source
    // without consuming the immutable MASTER. M17 READY can be previewed
    // immediately, with the same renderer invocation and no second job.
    QVERIFY(handoffUi.returnManual());
    QVERIFY(!activeMasterDevice);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(candidates.staged(),positiveMasterOwner);
    QCOMPARE(renderer.render_invocation_count(),positivePreflightRenderCount);
    vm.previewPrepared();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QCOMPARE(candidates.staged(),positiveMasterOwner);
    QCOMPARE(renderer.render_invocation_count(),positivePreflightRenderCount);
    // Product cannot silently borrow an already installed terminal sink;
    // an explicit ownership-retirement transaction is mandatory.
    QVERIFY(!product.borrow_verified_m17_master_for_device(
        *deviceRate.value()));
    auto rejectedInstalledReuse=
        product.materialize_borrowed_verified_m17_monitoring(
            *deviceRate.value(),convertedFromExisting);
    QVERIFY(!rejectedInstalledReuse);
    QCOMPARE(borrowedSrcCalls,std::size_t{3});
    QCOMPARE(renderer.render_invocation_count(),firstRenderCount);
    QCOMPARE(vm.audio_status(),
        QStringLiteral("ACTIVE · AUDITION UNQUALIFIED"));
    auto firstTerminalId=device->audibleRealization.realizationId;
    QVERIFY(firstTerminalId);
    QVERIFY(firstTerminalId!=originalId);
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->duration->value(),std::int64_t{4800});
    QCOMPARE(selector.processed_realization_snapshot(),original);
    vm.setBypass(true);
    QVERIFY(limiter.value()->bypassed());
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(device->position,core::FrameIndex{1600});
    QCOMPARE(device->loop,std::optional{*region.value()});
    QVERIFY(!product.private_id());
    QVERIFY(candidates.staged()); // Retained, NEVER audible on Bypass.
    QCOMPARE(product.status(),QStringLiteral("COMPARISON_VERIFIED"));
    QCOMPARE(candidates.staged(),initialPreparedPcm);
    QCOMPARE(renderer.status(),QStringLiteral("READY_PRIVATE"));
    QVERIFY(!audioGate.has_session());
    // More than one A/B switch must keep the same immutable PCM but issue a
    // new device RealizationId on every explicit terminal installation.
    qInfo().noquote()<<"M17_EDGE_CASE=REPEATED_AB_NEW_DEVICE_IDS_NO_RERENDER";
    auto previousTerminalId=firstTerminalId;
    const auto renderCountBeforeCycles=renderChanges.count();
    for(int n=0;n<3;++n) {
        vm.setBypass(false);
        QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
        QCOMPARE(device->audibleRealization.realizationId,originalId);
        vm.previewPrepared();
        QCOMPARE(product.status(),QStringLiteral("PRIVATE_INSTALLED"));
        const auto nextId=device->audibleRealization.realizationId;
        QVERIFY(nextId && nextId!=previousTerminalId && nextId!=originalId);
        QCOMPARE(candidates.staged(),initialPreparedPcm);
        previousTerminalId=nextId;
        vm.previewPrepared(); // Never install twice on accidental double click.
        QCOMPARE(device->audibleRealization.realizationId,nextId);
        vm.setBypass(true);
        QCOMPARE(product.status(),QStringLiteral("COMPARISON_VERIFIED"));
        QCOMPARE(device->audibleRealization.realizationId,originalId);
        QVERIFY(!audioGate.has_session());
    }
    QCOMPARE(renderChanges.count(),renderCountBeforeCycles);
    QCOMPARE(renderer.render_invocation_count(),firstRenderCount);
    QCOMPARE(product.borrow_verified_m17_master_for_device(
        *deviceRate.value()),initialPreparedPcm);

    // READY is already completed and MUST survive Cancel: no discarded
    // minute-long M17 work and no silent terminal handoff. A user can
    // preview the exact staged PCM after this no-op, then bypass explicitly.
    const int renderingEventsBeforeRearm=renderChanges.count();
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(candidates.staged(),initialPreparedPcm);
    QCOMPARE(renderChanges.count(),renderingEventsBeforeRearm);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    vm.cancelPreparation();
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    QCOMPARE(vm.preparation_fraction(),1.0);
    QVERIFY(vm.preparation_determinate());
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QVERIFY(!product.private_id());
    QVERIFY(candidates.staged());
    vm.previewPrepared();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QVERIFY(coordinator.device_active());
    QCOMPARE(candidates.staged(),initialPreparedPcm);
    vm.setBypass(true);
    QVERIFY(limiter.value()->bypassed());
    QCOMPARE(product.status(),QStringLiteral("COMPARISON_VERIFIED"));
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QVERIFY(!product.private_id());
    QVERIFY(candidates.staged());
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    // An A/B cache is bound to the physical OUTPUT_RATE. When the real
    // device probe changes from 48 kHz to 44.1 kHz, neither a silent SRC
    // nor an accidental M17 handoff is allowed, even for retained PCM.
    vm.setBypass(true);
    QCOMPARE(product.status(),QStringLiteral("COMPARISON_VERIFIED"));
    QCOMPARE(candidates.staged(),initialPreparedPcm);
    product.set_preferred_output_rate_probe(
        [sourceRate](audio::ChannelLayout)->core::Result<core::SampleRate> {
            return core::Result<core::SampleRate>::success(*sourceRate.value());
        });
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());
    QVERIFY(!audioGate.has_session());
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(device->loop,std::optional{*region.value()});
    product.set_preferred_output_rate_probe(
        [deviceRate](audio::ChannelLayout)->core::Result<core::SampleRate> {
            return core::Result<core::SampleRate>::success(*deviceRate.value());
        });
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    // Edits to the current value must NOT lose a minute-long accepted render.
    qInfo().noquote()<<"M17_EDGE_CASE=NOOP_CEILING_RETAINS_AB";
    const auto currentAdmitted=candidates.staged();
    const auto revisionBeforeNoop=limiter.value()->parameter_revision();
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-1.0));
    QVERIFY(vm.commitDraft());
    QCOMPARE(limiter.value()->parameter_revision(),revisionBeforeNoop);
    QCOMPARE(candidates.staged(),currentAdmitted);
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));

    // A SOURCE Loop edit during bypass must not silently reuse cached
    // output under a stale loop/map on the next ON transition.
    qInfo().noquote()<<"M17_EDGE_CASE=LOOP_CHANGED_DURING_CACHED_BYPASS";
    vm.setBypass(true);
    QCOMPARE(product.status(),QStringLiteral("COMPARISON_VERIFIED"));
    const auto movedLoop=core::FrameRange::create(
        core::FrameIndex{600},core::FrameIndex{3400});
    QVERIFY(movedLoop);
    desiredLoop=*movedLoop.value();
    QVERIFY(transport.set_loop_source_range(desiredLoop));
    coordinator.loop_context_changed();
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());
    QVERIFY(!product.borrow_verified_m17_master_for_device(
        *deviceRate.value()));
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    desiredLoop=*region.value();
    QVERIFY(transport.set_loop_source_range(desiredLoop));
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);

    // A saved PCM must not cross selection of a different audible target.
    qInfo().noquote()<<"M17_EDGE_CASE=TARGET_CHANGE_DROPS_CACHED_PCM";
    vm.setBypass(true);
    QCOMPARE(product.status(),QStringLiteral("COMPARISON_VERIFIED"));
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);

    // An actual parameter edit makes the OLD A/B PCM stale.
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-2.0));
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());
    // Reverting to the exact original numeric Ceiling does NOT restore
    // the old PCM: the parameter revision proves A->B->A was an edit.
    qInfo().noquote()<<"M17_EDGE_CASE=CEILING_ROUNDTRIP_NEVER_REVIVES_PCM";
    const auto revisionAfterChange=limiter.value()->parameter_revision();
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-1.0));
    QVERIFY(vm.commitDraft());
    QCOMPARE(limiter.value()->parameter_revision(),revisionAfterChange+1);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());

    // Cancel only an ACTIVE preparation; the stale worker must never
    // publish a candidate after cancellation or replace the MANUAL sink.
    vm.requestPrepare();
    QCOMPARE(vm.audio_status(),QStringLiteral("Preparing audio"));
    vm.cancelPreparation();
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QVERIFY(!product.private_id());
    QVERIFY(!candidates.staged());
    QTest::qWait(75); // Let an already-running worker attempt stale delivery.
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());
    QCOMPARE(device->audibleRealization.realizationId,originalId);

    // A SOURCE seek during a minute-long Prepare is NOT a new PCM owner:
    // the completed handoff must use the latest live source cue and loop.
    // This is preparation/cue safety, NOT yet cached MANUAL/M17 A/B.
    vm.requestPrepare();
    QCOMPARE(vm.audio_status(),QStringLiteral("Preparing audio"));
    QVERIFY(transport.seek_source_frame(core::FrameIndex{2200}));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(transport.source_display_snapshot().value()->position,
             core::FrameIndex{2200});
    vm.previewPrepared();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("PRIVATE_INSTALLED"),12000);
    QVERIFY(coordinator.device_active());
    QCOMPARE(transport.source_display_snapshot().value()->loop,
             std::optional{*region.value()});
    vm.setBypass(true);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(device->position,core::FrameIndex{2200});
    QCOMPARE(device->loop,std::optional{*region.value()});
    QVERIFY(!product.private_id());
    QVERIFY(candidates.staged());
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));

    // PAUSED stays paused while re-auditioning the retained PCM; Preview Prepared installs an
    // OUTPUT_RATE owner only after an explicit request.
    QVERIFY(transport.seek_source_frame(core::FrameIndex{2600}));
    transport.pause();
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    vm.previewPrepared();
    QVERIFY(coordinator.device_active());
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    const auto pausedTerminalId=device->audibleRealization.realizationId;
    QVERIFY(pausedTerminalId && pausedTerminalId!=firstTerminalId);
    qInfo().noquote()<<"M17_EDGE_CASE=STOPPED_AB_TRANSPORT";
    transport.stop();
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    vm.setBypass(true);
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    vm.setBypass(false);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    vm.previewPrepared();
    QCOMPARE(product.status(),QStringLiteral("PRIVATE_INSTALLED"));
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    // Re-establish PAUSED for the existing Ceiling-edit regression below.
    transport.playOrResume();
    transport.pause();
    QCOMPARE(device->state,core::PlaybackState::PAUSED);

    // Committing Ceiling must invalidate the prepared result but MUST NOT
    // schedule another full-resource M17 job automatically.
    QVERIFY(vm.setDraftFieldValue(QStringLiteral("ceilingDbTp"),-6.0));
    QVERIFY(vm.commitDraft());
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->audibleRealization.realizationId,originalId);
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    vm.previewPrepared();
    QVERIFY(coordinator.device_active());
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QVERIFY(device->audibleRealization.realizationId!=pausedTerminalId);
    QVERIFY(transport.seek_source_frame(core::FrameIndex{3100}));
    vm.setBypass(true);
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(device->position,core::FrameIndex{3100});
    QCOMPARE(device->audibleRealization.realizationId,originalId);

    // Upstream Input Gain changes retire M17 before MANUAL publication.
    // Neither preview completion nor Live Play can silently reprepare.
    vm.setBypass(false);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    vm.previewPrepared();
    QVERIFY(coordinator.device_active());
    auto gain3=dsp::GainParameters::create(+3.0);
    QVERIFY(gain3);
    QVERIFY(upstream.value()->set_gain_parameters(*gain3.value()));
    preview.request_preview();
    QCOMPARE(preview.preview_status(),QStringLiteral("RENDERING"));
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QVERIFY(!coordinator.device_active());
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!limiter.value()->bypassed());
    QTRY_COMPARE_WITH_TIMEOUT(preview.preview_status(),
        QStringLiteral("READY"),12000);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    const auto updatedManualId=selector.processed_realization_id();
    const auto updatedManual=selector.processed_realization_snapshot();
    QVERIFY(updatedManualId&&updatedManual&&updatedManualId!=originalId);
    const auto oldLane=original->view().channel(0);
    const auto newLane=updatedManual->view().channel(0);
    QVERIFY(oldLane&&newLane);
    QVERIFY((*oldLane.value())[0]!=(*newLane.value())[0]);
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    QCOMPARE(device->audibleRealization.realizationId,updatedManualId);

    // SOURCE Loop edits on a READY whole-track private Prepare update only
    // the live transport handoff. They MUST NOT invalidate the immutable PCM
    // or require a second full-resource M17 Prepare.
    const auto shiftedRegion=core::FrameRange::create(
        core::FrameIndex{700},core::FrameIndex{3300});
    QVERIFY(shiftedRegion);
    desiredLoop=*shiftedRegion.value();
    QVERIFY(transport.set_loop_source_range(desiredLoop));
    coordinator.loop_context_changed();
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QVERIFY(!coordinator.device_active());
    QVERIFY(!product.private_id());
    vm.previewPrepared();
    QVERIFY(coordinator.device_active());
    QCOMPARE(transport.source_display_snapshot().value()->loop,desiredLoop);

    // A second edit invalidates an already installed result, leaving
    // ordinary preview functional and M17 visibly due for preparation.
    auto gainDown=dsp::GainParameters::create(-3.0);
    QVERIFY(gainDown);
    QVERIFY(upstream.value()->set_gain_parameters(*gainDown.value()));
    preview.request_preview();
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QTRY_COMPARE_WITH_TIMEOUT(preview.preview_status(),
        QStringLiteral("READY"),12000);
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!limiter.value()->bypassed());
    vm.setBypass(true);
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());

    // An authentic rate-probe failure remains a real error, not CANCELLED.
    product.set_preferred_output_rate_probe(
        [](audio::ChannelLayout)->core::Result<core::SampleRate> {
            return core::Result<core::SampleRate>::failure(core::Error{
                core::ErrorCode::UnsupportedOperation,
                "Synthetic audition rate-probe failure."});
        });
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    vm.requestPrepare();
    QVERIFY(limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("ERROR · BYPASS"));
    QVERIFY(vm.validation_message().contains(
        QStringLiteral("Synthetic audition rate-probe failure")));
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());

    // A newly accepted SOURCE may clear a stale AUDIO error after a
    // fail-closed M17 rejection. It does NOT activate, prepare, install
    // or claim any output; no previous private owner is resurrected.
    coordinator.source_committed_successfully();
    QCOMPARE(vm.audio_status(),QStringLiteral("BYPASS · NEW SOURCE"));
    QCOMPARE(vm.validation_message(),QString{});
    QVERIFY(limiter.value()->bypassed());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());

    // A PREPARED target is automatically switched to the real accepted
    // PROCESSED owner before the M17 Prepare preflight. The deliberately
    // failing DEVICE rate probe still fails closed, but target mismatch
    // must NOT be the reason or leave PREPARED selected.
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    vm.requestPrepare();
    QCOMPARE(selector.active_target(),std::optional{app::AuditionTarget::PROCESSED});
    QVERIFY(limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("ERROR · BYPASS"));
    QVERIFY(vm.validation_message().contains(
        QStringLiteral("Synthetic audition rate-probe failure")));
    QVERIFY(!vm.validation_message().contains(
        QStringLiteral("requires PROCESSED audition target")));
    QVERIFY(!coordinator.device_active());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());

    // The source and selected master here are both 44.1 kHz. The
    // deliberately failing DEVICE probe must not affect the independent
    // master-only request, and must not publish into any audio sink.
    // The positive cross-rate 44.1 -> 192 original-Q0 render below
    // exercises this same immutable MASTER path without granting FQ release.
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    // The second Input Gain edit earlier in this fixture publishes a NEW
    // MANUAL realization. The previous updatedManualId is now stale.
    // Master-only Prepare must retain the exact CURRENT audible owner.
    const auto currentManualId=selector.processed_realization_id();
    QVERIFY(currentManualId);
    QVERIFY(currentManualId!=updatedManualId);
    QCOMPARE(device->audibleRealization.realizationId,currentManualId);
    QVERIFY(limiter.value()->set_user_bypass(false));
    const auto masterRate=core::SampleRate::create(44100);
    const auto highMasterRate=core::SampleRate::create(192000);
    const auto invalidRate=core::SampleRate::create(32000);
    QVERIFY(masterRate&&highMasterRate&&invalidRate);
    const auto unsupported=product.request_private_for_final_master(
        *invalidRate.value());
    QVERIFY(!unsupported);
    QCOMPARE(unsupported.error()->code(),
             core::ErrorCode::UnsupportedOperation);
    QVERIFY(product.request_private_for_final_master(*masterRate.value()));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"),12000);
    const auto stagedMaster=candidates.staged();
    QVERIFY(stagedMaster);
    QCOMPARE(stagedMaster->rendered.output_rate,*masterRate.value());
    QCOMPARE(stagedMaster->rendered.terminal.checked.terminal
        .compensated_pcm.view().format().sample_rate(),*masterRate.value());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QVERIFY(!coordinator.device_active());
    QCOMPARE(device->audibleRealization.realizationId,currentManualId);
    QVERIFY(!product.install_ready_private());
    QVERIFY(!product.install_measured_private());
    QVERIFY(product.cancel());
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());
    QCOMPARE(device->audibleRealization.realizationId,currentManualId);

    // Uniform original-Q0 admission: 44.1 -> 192 is a supported,
    // authenticated nonidentity MASTER rate just like 44.1 -> 48.
    // A formerly experimental/rejected fixture MUST now finish an actual
    // M17+M16 private render, retaining SOURCE and never touching DEVICE.
    const auto beforeHighRate=renderer.render_invocation_count();
    QVERIFY(product.request_private_for_final_master(*highMasterRate.value()));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"),30000);
    QCOMPARE(renderer.render_invocation_count(),beforeHighRate+1);
    const auto highRateMaster=candidates.staged();
    QVERIFY(highRateMaster);
    QCOMPARE(highRateMaster->rendered.output_rate,*highMasterRate.value());
    QCOMPARE(highRateMaster->rendered.terminal.checked.terminal
        .compensated_pcm.view().format().sample_rate(),*highMasterRate.value());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QCOMPARE(device->audibleRealization.realizationId,currentManualId);
    QVERIFY(product.cancel());
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());
    QCOMPARE(device->audibleRealization.realizationId,currentManualId);

    // Production M17 now chooses the persisted PROJECT MASTER target,
    // not the DAC clock. The 44.1k MASTER on a probed 48k device is
    // checked and retained for DEVICE SRC but never directly installed.
    app::MasterOutputRateState projectMaster;
    projectMaster.source_committed(44100);
    coordinator.set_master_rate_state(projectMaster);
    product.set_preferred_output_rate_probe(
        [deviceRate](audio::ChannelLayout)->core::Result<core::SampleRate>{
            return core::Result<core::SampleRate>::success(*deviceRate.value());
        });
    vm.setBypass(true);
    vm.setBypass(false);
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    const auto firstProjectMaster=candidates.staged();
    QVERIFY(firstProjectMaster);
    QCOMPARE(firstProjectMaster->rendered.output_rate,*masterRate.value());
    QCOMPARE(vm.audio_status(),
        QStringLiteral("MASTER VERIFIED · LISTEN TO MASTER"));
    QVERIFY(!product.current_master_matches_device_for_preview());
    const auto callsBeforeIgnoredPreview=renderer.render_invocation_count();
    vm.previewPrepared(); // disallowed: project MASTER != physical DEVICE
    QCOMPARE(renderer.render_invocation_count(),callsBeforeIgnoredPreview);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(device->audibleRealization.realizationId,currentManualId);
    // Coordinator is subscribed to Project rate *revision* only. A lease
    // eligibility update must NOT become a false rate edit and retire the
    // same immutable M17 candidate, even when QML bindings refresh.
    const auto retainedReadyMaster=candidates.staged();
    const auto stableRevision=projectMaster.rate_revision();
    bool deviceOwnsMaster=true;
    projectMaster.set_target_selection_guard([&deviceOwnsMaster]() noexcept {
        return !deviceOwnsMaster;
    });
    projectMaster.refresh_selection_availability();
    QVERIFY(!projectMaster.can_edit_target());
    QVERIFY(!projectMaster.choose_master_rate(48000));
    QCOMPARE(projectMaster.rate_revision(),stableRevision);
    QCOMPARE(candidates.staged(),retainedReadyMaster);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(renderer.render_invocation_count(),callsBeforeIgnoredPreview);
    deviceOwnsMaster=false;
    projectMaster.refresh_selection_availability();
    QCOMPARE(candidates.staged(),retainedReadyMaster);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(renderer.render_invocation_count(),callsBeforeIgnoredPreview);
    // A revised Project target invalidates, it does NOT silently reprepare.
    QVERIFY(projectMaster.choose_master_rate(48000));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(device->audibleRealization.realizationId,currentManualId);
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    QCOMPARE(candidates.staged()->rendered.output_rate,*deviceRate.value());
    QCOMPARE(vm.audio_status(),QStringLiteral("READY · PREVIEW AVAILABLE"));
    vm.setBypass(true);
}

void TerminalActivationIntegrationTest::
privateMaster192kOn48kDeviceRemainsUninstalled()
{
    // Separate fresh 192-kHz SOURCE fixture: the identity MASTER
    // uses no SRC FIR, while the independent physical 48-kHz DEVICE
    // remains untouched. Cross-rate original-Q0 policy is tested above.
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    auto masterRate=core::SampleRate::create(192000);
    auto deviceRate=core::SampleRate::create(48000);
    auto frames=core::FrameCount::create(1920);
    auto block=core::FrameCount::create(128);
    QVERIFY(registry&&masterRate&&deviceRate&&frames&&block);
    auto chain=dsp::ProcessingChain::create(
        *registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    auto format=audio::AudioFormat::create(
        *masterRate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(format);
    auto source=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(source);
    for(std::size_t ch=0;ch<2;++ch) {
        auto lane=source.value()->mutable_view().channel(ch);
        QVERIFY(lane);
        for(std::size_t i=0;i<1920;++i)
            (*lane.value())[i]=(i%29==0 ? 0.22:0.08)
                *(ch==0 ? 1.0:-0.6);
    }
    const auto renderSource=[&]()->render::RenderResult {
        auto request=render::RenderRequest::create(
            source.value()->view(),source.value()->view().absolute_range(),
            *chain.value(),{},*block.value());
        Q_ASSERT(request);
        auto result=render::render_preview(
            *request.value(),*registry.value());
        Q_ASSERT(result);
        return std::move(*result.value());
    };
    auto service=std::make_unique<FakePlaybackService>();
    auto* device=service.get();
    app::PlaybackTransportViewModel transport{std::move(service)};
    transport.set_pcm_prepare_handler(
        [device](audio::AudioBufferView view,
            std::shared_ptr<const void> lease,
            std::optional<core::RealizationId> id)->core::Status {
            if(!lease||!id||view.frame_count().value()<=0)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "Unowned MANUAL PCM in 192-kHz fixture."});
            device->state=core::PlaybackState::STOPPED;
            device->position=core::FrameIndex{0};
            device->duration=view.frame_count();
            device->loop.reset();
            device->audibleRealization={
                core::AudibleHandoffPhase::NEW,id};
            return core::Status::success();
        });
    app::AuditionSourceSelector selector{&transport};
    QVERIFY(selector.set_prepared_realization(renderSource()));
    QVERIFY(selector.set_processed_realization(renderSource()));
    QVERIFY(selector.switch_to(app::AuditionTarget::PROCESSED));
    const auto manualId=selector.processed_realization_id();
    QVERIFY(manualId);
    auto limiterId=dsp::ModuleInstanceId::from_uuid(*core::Uuid::parse(
        "7f000000-0000-4000-8000-000000000619").value());
    QVERIFY(limiterId);
    auto limiter=app::TerminalLimiterState::create_default(
        *registry.value(),*limiterId.value());
    QVERIFY(limiter);
    QVERIFY(limiter.value()->set_user_bypass(false));
    app::TerminalCandidateGate candidates;
    std::size_t sinkInstalls=0;
    app::TerminalAudioSessionGate audioGate{
        candidates,*device,
        [&sinkInstalls](audio::AudioBufferView,
            std::shared_ptr<const void>,
            core::RealizationId)->core::Status {
            ++sinkInstalls;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "Master-only PCM may not reach a device sink."});
        }};
    app::TerminalPrivatePublicationCoordinator publisher{
        candidates,audioGate,transport,
        [&selector](std::shared_ptr<const render::RenderResult> prepared,
            std::shared_ptr<const render::RenderResult> manual,
            std::optional<core::RealizationId> id) {
            return selector.reserve_private_terminal_realization_id(
                prepared,manual,id);
        }};
    app::TerminalPrivateRenderController renderer{
        candidates,*limiter.value(),
        [&selector]{return selector.prepared_realization_snapshot();},
        [&selector]{return selector.processed_realization_snapshot();},
        [&selector]{return selector.processed_realization_id();},
        [&chain]{return *chain.value();}};
    std::optional<core::FrameRange> selectedSourceLoop;
    selector.set_source_loop_provider([&selectedSourceLoop] {
        return selectedSourceLoop;
    });
    // Session-level Selected provider can independently refuse audible
    // PROCESSED after device rejection. The real initial-source selector
    // denial is separately tested across 44.1/48/192 kHz WAV fixtures.
    bool productAudibleSelectionAccepted=true;
    app::TerminalPrivateProductSession product{
        renderer,publisher,candidates,transport,
        [&selector]{return selector.prepared_realization_snapshot();},
        [&selector]{return selector.processed_realization_snapshot();},
        [&selector]{return selector.processed_realization_id();},
        [&selectedSourceLoop]{return selectedSourceLoop;},
        [&selector,&productAudibleSelectionAccepted] {
            return productAudibleSelectionAccepted
                &&selector.active_target()==app::AuditionTarget::PROCESSED;
        }};
    app::TerminalMonitorViewModel monitorUi{product,transport};
    monitorUi.set_offline_master_authorizer([]{
        return core::Status::success();
    });
    QVERIFY(monitorUi.can_prepare());
    QVERIFY(!monitorUi.can_prepare_offline()); // normal MANUAL uses M17
    QVERIFY(!monitorUi.can_dismiss());
    // A real normal M17 READY candidate at the selected MASTER clock
    // must already be consumable by Master Audition without invoking a
    // second terminal renderer job. No device is installed in this check.
    const auto beforeNormalM17=renderer.render_invocation_count();
    QVERIFY(product.request_private(*masterRate.value(),true));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    QCOMPARE(renderer.render_invocation_count(),beforeNormalM17+1);
    // Actual accepted worker timings, not a projected 50ms microbenchmark.
    auto observedPrepare=renderer.last_approved_prepare_timing();
    QVERIFY(observedPrepare.has_value());
    QCOMPARE(observedPrepare->master_rate,*masterRate.value());
    QCOMPARE(observedPrepare->source_rate,*masterRate.value());
    QVERIFY(observedPrepare->source_frames.value()>0);
    QVERIFY(observedPrepare->queue_us>=0);
    QVERIFY(observedPrepare->render_wall_us>0);
    QVERIFY(observedPrepare->m17_core_us>=0);
    QVERIFY(observedPrepare->m16_primary_us>=0);
    QVERIFY(app::TerminalPrivateRenderController::approved_timing_eligible(
        *observedPrepare));
    // A real accepted MASTER with missing SRC telemetry must NOT
    // become a false whole-Prepare timing observation for another rate.
    {
        auto incomplete=*observedPrepare;
        auto otherSource=core::SampleRate::create(44100);
        QVERIFY(otherSource);
        incomplete.source_rate=*otherSource.value();
        incomplete.src_us=-1;
        QVERIFY(!app::TerminalPrivateRenderController::approved_timing_eligible(
            incomplete));
        incomplete.src_us=0;
        QVERIFY(app::TerminalPrivateRenderController::approved_timing_eligible(
            incomplete));
        incomplete.queue_us=-1;
        QVERIFY(!app::TerminalPrivateRenderController::approved_timing_eligible(
            incomplete));
        incomplete.queue_us=std::numeric_limits<std::int64_t>::max();
        QVERIFY(!app::TerminalPrivateRenderController::approved_timing_eligible(
            incomplete));
        incomplete.queue_us=0;
        incomplete.m16_primary_us=-1;
        QVERIFY(!app::TerminalPrivateRenderController::approved_timing_eligible(
            incomplete));
    }
    QVERIFY(!renderer.exact_historical_prepare_range(
        observedPrepare->source_rate,observedPrepare->master_rate,
        observedPrepare->source_frames,observedPrepare->layout,
        observedPrepare->ceiling_db_tp,
        observedPrepare->manual_chain_revision,
        observedPrepare->terminal_chain_revision,
        observedPrepare->terminal_parameter_revision).has_value());
    QVERIFY(product.verified_master_rate_for_monitoring());
    QCOMPARE(*product.verified_master_rate_for_monitoring(),
        *masterRate.value());

    // M18 PRIVATE PRODUCT authority: exact M17 owner and explicit consent.
    // Never writes an actual WAV or grants a GUI export permission.
    {
        auto src=core::ResourceReference::create(
            "test.memory","m18-product-source",true,false,
            "Synthetic M18 source identity");
        auto dst=core::ResourceReference::create(
            "test.memory","m18-product-destination",false,true,
            "Synthetic M18 destination identity");
        QVERIFY(src && dst);
        bool consentCurrent=true;
        std::size_t consentChecks=0;
        const auto approve=[&]()->core::Status {
            ++consentChecks;
            if(!consentCurrent)
                return core::Status::failure(core::Error{
                    core::ErrorCode::AccessDenied,
                    "M18_TEST_EXPLICIT_CONSENT_REVOKED"});
            return core::Status::success();
        };
        auto noConsent=product.make_private_m18_synchronous_export_request(
            *masterRate.value(),*src.value(),*dst.value(),
            audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0,{});
        QVERIFY(!noConsent);
        QCOMPARE(noConsent.error()->code(),core::ErrorCode::AccessDenied);
        auto wrongRate=product.make_private_m18_synchronous_export_request(
            *deviceRate.value(),*src.value(),*dst.value(),
            audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0,approve);
        QVERIFY(!wrongRate);
        auto wrongSeed=product.make_private_m18_synchronous_export_request(
            *masterRate.value(),*src.value(),*dst.value(),
            audio::WavSampleFormat::PCM_S24,std::nullopt,-1.0,approve);
        QVERIFY(!wrongSeed);
        const auto authorized=product.make_private_m18_synchronous_export_request(
            *masterRate.value(),*src.value(),*dst.value(),
            audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0,approve);
        QVERIFY2(authorized,authorized?"":authorized.error()->message().c_str());
        QCOMPARE(authorized.value()->sample_format(),
                 audio::WavSampleFormat::PCM_S24);
        QVERIFY(authorized.value()->dither_seed()
                ==std::optional<std::uint64_t>{UINT64_C(42)});
        const auto original=candidates.staged();
        QVERIFY(original);
        // The real Product session, not a forged stage pointer, now
        // supplies an exact verified M17 snapshot for a future async
        // worker request. No Qt object may be read by that worker.
        const auto productStage=
            product.capture_m18_verified_stage_for_worker(
                *masterRate.value());
        QCOMPARE(productStage,original);
        QVERIFY(!product.capture_m18_verified_stage_for_worker(
            *deviceRate.value())); // DAC rate is not MASTER rate.
        bool workerDeniedStage=false;
        bool workerDeniedGateReads=false;
        std::thread illicitOwner([&] {
            workerDeniedStage=
                !product.capture_m18_verified_stage_for_worker(
                    *masterRate.value());
            // The candidate's shared_ptr is not an atomic worker-side
            // getter. Only the already retained immutable original may
            // be carried into M18 asynchronous file export.
            workerDeniedGateReads=
                !candidates.staged()&&!candidates.staged_measured();
        });
        illicitOwner.join();
        QVERIFY(workerDeniedStage);
        QVERIFY(workerDeniedGateReads);
        QCOMPARE(candidates.staged(),original);
        QCOMPARE(product.capture_m18_verified_stage_for_worker(
            *masterRate.value()),original);
        // PREPRODUCTION owner-thread mutation admission: a held export
        // epoch must deny Product cancellation without clearing the true
        // M17 MASTER or changing the audible manual predecessor.
        std::size_t deniedMutations=0;
        product.set_private_m18_mutation_admission(
            [&](const char* operation)->core::Status {
                if(operation &&*operation)++deniedMutations;
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_TEST_COMMIT_BUSY_MUTATION_DENIED"});
            });
        // A background export worker cannot silently disarm the real
        // Product QObject authority before cancellation is admitted.
        std::thread foreignGateRemoval([&] {
            product.set_private_m18_mutation_admission({});
        });
        foreignGateRemoval.join();
        const auto cancelDenied=product.cancel();
        QVERIFY(!cancelDenied);
        QCOMPARE(cancelDenied.error()->code(),core::ErrorCode::InvalidState);
        QVERIFY(candidates.staged()==original);
        QVERIFY(!product.adopt_pending_source_loop_without_rerender());
        QVERIFY(candidates.staged()==original);
        QCOMPARE(deniedMutations,std::size_t{2});
        // Clear the inert seam: the existing M17 product test must then
        // continue with exact old behavior and no new M18 UI activation.
        product.set_private_m18_mutation_admission({});
        std::size_t deniedUpstreamMutations=0;
        selector.set_private_m18_mutation_admission(
            [&](const char* operation)->core::Status {
                if(operation &&*operation)++deniedUpstreamMutations;
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M18_TEST_UPSTREAM_SOURCE_BUSY"});
            });
        const auto switchDenied=selector.switch_to(
            app::AuditionTarget::PREPARED);
        QVERIFY(!switchDenied);
        QCOMPARE(switchDenied.error()->code(),core::ErrorCode::InvalidState);
        QVERIFY(selector.active_target()
                ==std::optional{app::AuditionTarget::PROCESSED});
        QVERIFY(candidates.staged()==original);
        QCOMPARE(deniedUpstreamMutations,std::size_t{1});
        selector.set_private_m18_mutation_admission({});
        // Rate edits use a separate mutation-only Product admission, not
        // a side-effecting query from the QML canEditTarget property.
        app::MasterOutputRateState guardedRate;
        guardedRate.source_committed(48000);
        const auto oldRevision=guardedRate.rate_revision();
        std::size_t deniedRateChanges=0;
        guardedRate.set_private_m18_rate_mutation_admission([&] {
            ++deniedRateChanges;
            return false;
        });
        QVERIFY(!guardedRate.choose_master_rate(44100));
        QCOMPARE(guardedRate.master_rate_hz(),qint64{48000});
        QCOMPARE(guardedRate.rate_revision(),oldRevision);
        QCOMPARE(deniedRateChanges,std::size_t{1});
        guardedRate.set_private_m18_rate_mutation_admission({});
        QVERIFY(guardedRate.choose_master_rate(44100));
        QCOMPARE(guardedRate.rate_revision(),oldRevision+1);

        // One real Qt-free M18 epoch authority serves all THREE dormant
        // Product mutation ports: terminal cancellation, upstream SOURCE
        // selection and Project MASTER-rate choice. No callback is wired
        // into the production application by this regression.
        using platform::windows::internal::M18ProductEpochTicketAuthority;
        using platform::windows::internal::M18ProductExportIdentity;
        const M18ProductExportIdentity verifiedId{
            1101U,1102U,1103U,
            original->rendered.manual_chain_revision,
            static_cast<std::uint32_t>(masterRate.value()->value())};
        // M17 initial MANUAL revision can legitimately be zero; it
        // remains a concrete identity, not an unset generation marker.
        QCOMPARE(verifiedId.chain_revision,
                 original->rendered.manual_chain_revision);
        QVERIFY(verifiedId.valid());
        M18ProductEpochTicketAuthority sharedEpoch{777U,verifiedId};
        QVERIFY(sharedEpoch.try_transition(777U,778U,verifiedId,true));
        auto masterTicket=sharedEpoch.capture(verifiedId);
        QVERIFY(masterTicket);
        // Exercise the REAL future Product-facing seam against an actual
        // SourceSelectionViewModel, a genuine disk WAV and the current M17
        // Product session, without QML, exporter invocation or device.
        const auto m17View=original->rendered.terminal.checked.terminal
            .compensated_pcm.view();
        auto sourceBytes=std::make_shared<MatrixWavBytes>();
        const audio::WavWriteSpec realSourceSpec{
            m17View.format(),m17View.frame_count(),
            audio::WavSampleFormat::IEEE_F64};
        auto sourceWriter=audio::WavWriter::open(
            std::make_unique<MatrixWavSink>(sourceBytes),realSourceSpec);
        QVERIFY(sourceWriter);
        QVERIFY((*sourceWriter.value())->write_frames(m17View));
        QVERIFY((*sourceWriter.value())->finalize());
        QVERIFY((*sourceWriter.value())->close());
        QTemporaryDir m18ProductSourceDir;QVERIFY(m18ProductSourceDir.isValid());
        const auto selectedSourcePath=m18ProductSourceDir.filePath(
            QStringLiteral("selected-source-master.wav"));
        QFile selectedSourceFile{selectedSourcePath};
        QVERIFY(selectedSourceFile.open(
            QIODevice::WriteOnly|QIODevice::NewOnly));
        const auto sourceLength=
            static_cast<qint64>(sourceBytes->bytes.size());
        QCOMPARE(selectedSourceFile.write(
            reinterpret_cast<const char*>(sourceBytes->bytes.data()),
            sourceLength),sourceLength);
        selectedSourceFile.close();
        app::SourceSelectionViewModel selectedSource;
        selectedSource.selectSource(QUrl::fromLocalFile(selectedSourcePath));
        QVERIFY(selectedSource.has_source());
        const auto* liveSource=selectedSource.source_resource();
        QVERIFY(liveSource);
        app::MasterOutputRateState selectedProjectRate;
        selectedProjectRate.source_committed(
            selectedSource.sample_rate_hz());
        QCOMPARE(selectedProjectRate.master_rate_hz(),
                 static_cast<qint64>(masterRate.value()->value()));
        // Product live handoff additionally binds the exact monotonic
        // SOURCE/MASTER selector revision. The legacy standalone ticket
        // remains usable only by the isolated pure request builder.
        auto boundIdentity=verifiedId;
        boundIdentity.master_rate_revision=
            selectedProjectRate.rate_revision();
        M18ProductEpochTicketAuthority boundOwner{900U,boundIdentity};
        QVERIFY(boundOwner.try_transition(
            900U,901U,boundIdentity,true));
        auto boundTicket=boundOwner.capture(boundIdentity);
        QVERIFY(boundTicket);
        const auto deniedUnboundRevision=
            app::internal::make_m18_checked_stage_worker_request_from_live_product(
                product,selectedSource,selectedProjectRate,candidates,
                *masterTicket.value(),liveSource->reference(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!deniedUnboundRevision);
        const auto correctProductWorkerRequest=
            app::internal::make_m18_checked_stage_worker_request_from_live_product(
                product,selectedSource,selectedProjectRate,candidates,
                *boundTicket.value(),liveSource->reference(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(correctProductWorkerRequest);
        // M18 private vertical slice: use the REAL M17 Product session and
        // SOURCE selection, freeze the exact stage on Qt owner thread, then
        // run the actual Windows transactional export on a detached worker.
        // No QML/export menu is enabled. Test-owned Product ticket is
        // explicit and the source/destination are real local filesystem
        // references; worker never dereferences a QObject.
        for(const auto targetFormat:{
                audio::WavSampleFormat::PCM_S24,
                audio::WavSampleFormat::IEEE_F32}) {
            const auto filename=QStringLiteral("product-m18-%1.wav")
                .arg(static_cast<int>(targetFormat));
            const auto path=m18ProductSourceDir.filePath(filename);
            const auto destination=
                platform::windows::WindowsResourceWriter::
                    make_write_reference(
                        path.toUtf8().toStdString(),
                        filename.toUtf8().toStdString());
            QVERIFY(destination);
            const auto ditherSeed=
                targetFormat==audio::WavSampleFormat::PCM_S24
                    ?std::optional<std::uint64_t>{UINT64_C(42)}
                    :std::nullopt;
            auto worker=app::internal::
                launch_m18_async_export_from_live_product(
                    product,selectedSource,selectedProjectRate,candidates,
                    *boundTicket.value(),liveSource->reference(),
                    *destination.value(),targetFormat,ditherSeed,0.0);
            QVERIFY(worker);
            QCOMPARE((*worker.value())->total_frames(),
                     static_cast<std::uint64_t>(
                         m17View.frame_count().value()));
            QTRY_VERIFY_WITH_TIMEOUT(
                (*worker.value())->finished(),30000);
            const auto done=(*worker.value())->result_if_finished();
            QVERIFY(done);
            QVERIFY2(static_cast<bool>(*done),
                done->error()?done->error()->message().c_str()
                             :"no Product export evidence");
            QCOMPARE(done->value()->sample_format(),targetFormat);
            QCOMPARE(done->value()->frame_count().value(),
                     m17View.frame_count().value());
            QCOMPARE(done->value()->encoded_file_sha256().size(),
                     std::size_t{64});
            QVERIFY(QFileInfo::exists(path));
            const auto before=QFileInfo{path}.size();
            QVERIFY(before>0);
            // The live Product pipeline never authorizes overwriting its
            // own previous verified export, even on a second click.
            const auto duplicate=app::internal::
                launch_m18_async_export_from_live_product(
                    product,selectedSource,selectedProjectRate,candidates,
                    *boundTicket.value(),liveSource->reference(),
                    *destination.value(),targetFormat,ditherSeed,0.0);
            QVERIFY(duplicate);
            QTRY_VERIFY_WITH_TIMEOUT(
                (*duplicate.value())->finished(),30000);
            const auto duplicateResult=
                (*duplicate.value())->result_if_finished();
            QVERIFY(duplicateResult);
            QVERIFY(!*duplicateResult);
            QCOMPARE(QFileInfo{path}.size(),before);
        }
        // The preproduction Product lifecycle admits exactly ONE async
        // export until its terminal result is acknowledged. No lost WAV
        // evidence, accidental parallel export or UI-thread blocking.
        app::internal::M18PrivateAsyncExportSession exportSession;
        auto managedPath=m18ProductSourceDir.filePath(
            QStringLiteral("managed-session-master.wav"));
        const auto managedDestination=
            platform::windows::WindowsResourceWriter::
                make_write_reference(
                    managedPath.toUtf8().toStdString(),
                    "managed-session-master.wav");
        QVERIFY(managedDestination);
        QVERIFY(exportSession.begin(
            product,selectedSource,selectedProjectRate,candidates,
            *boundTicket.value(),liveSource->reference(),
            *managedDestination.value(),audio::WavSampleFormat::IEEE_F32,
            std::nullopt,0.0));
        QVERIFY(!exportSession.begin(
            product,selectedSource,selectedProjectRate,candidates,
            *boundTicket.value(),liveSource->reference(),
            *managedDestination.value(),audio::WavSampleFormat::IEEE_F32,
            std::nullopt,0.0));
        QTRY_VERIFY_WITH_TIMEOUT(
            exportSession.snapshot().phase
                ==app::internal::M18PrivateAsyncExportSession::
                    Phase::VERIFIED_DELIVERED,
            30000);
        const auto finalSessionSnapshot=exportSession.snapshot();
        QCOMPARE(finalSessionSnapshot.encoded_frames,
                 finalSessionSnapshot.total_frames);
        QVERIFY(!finalSessionSnapshot.failure_code);
        QVERIFY(QFileInfo::exists(managedPath));
        // The session handle itself is not an atomic cross-thread pointer.
        // A rogue worker cannot cancel it, read it or acknowledge the
        // finished Product result while the UI owns its lifecycle.
        bool foreignSnapshotRejected=false;
        std::thread foreignSession([&] {
            const auto denied=exportSession.snapshot();
            foreignSnapshotRejected=
                denied.phase==app::internal::M18PrivateAsyncExportSession::
                    Phase::FAILED
                &&denied.failure_code==core::ErrorCode::InvalidState;
            exportSession.cancel();
        });
        foreignSession.join();
        QVERIFY(foreignSnapshotRejected);
        QCOMPARE(exportSession.snapshot().phase,
            app::internal::M18PrivateAsyncExportSession::
                Phase::VERIFIED_DELIVERED);
        QVERIFY(exportSession.acknowledge_finished());
        QCOMPARE(exportSession.snapshot().phase,
                 app::internal::M18PrivateAsyncExportSession::Phase::IDLE);
        QVERIFY(!exportSession.acknowledge_finished());

        // A dormant but genuine Qt ViewModel observes ONLY atomic worker
        // progress and a post-readback verified result. No Product/GUI
        // registration, and no worker callback may enter the QML thread.
        app::M18PrivateExportProgressViewModel exportView;
        QSignalSpy exportChanges{
            &exportView,&app::M18PrivateExportProgressViewModel::changed};
        QCOMPARE(exportView.status_text(),QStringLiteral("IDLE"));
        QVERIFY(!exportView.export_in_progress());
        QVERIFY(!exportView.can_cancel());
        QVERIFY(!exportView.can_acknowledge());
        QVERIFY(!exportView.verified_delivered());
        const auto viewOutputPath=m18ProductSourceDir.filePath(
            QStringLiteral("viewmodel-m18-verified.wav"));
        auto viewOutputRef=platform::windows::WindowsResourceWriter::
            make_write_reference(
                viewOutputPath.toUtf8().toStdString(),
                "viewmodel-m18-verified.wav");
        QVERIFY(viewOutputRef);
        QVERIFY(exportView.begin_private_export(
            product,selectedSource,selectedProjectRate,candidates,
            *boundTicket.value(),liveSource->reference(),
            *viewOutputRef.value(),audio::WavSampleFormat::IEEE_F32,
            std::nullopt,0.0));
        QVERIFY(!exportView.begin_private_export(
            product,selectedSource,selectedProjectRate,candidates,
            *boundTicket.value(),liveSource->reference(),
            *viewOutputRef.value(),audio::WavSampleFormat::IEEE_F32,
            std::nullopt,0.0));
        QTRY_VERIFY_WITH_TIMEOUT(exportView.verified_delivered(),30000);
        QCOMPARE(exportView.status_text(),
                 QStringLiteral("VERIFIED_DELIVERED"));
        QCOMPARE(exportView.encoded_progress(),1.0);
        QVERIFY(!exportView.export_in_progress());
        QVERIFY(!exportView.can_cancel());
        QVERIFY(exportView.can_acknowledge());
        QVERIFY(exportView.error_text().isEmpty());
        // A direct stale UI Cancel click after successful WAV readback
        // must not relabel or revoke the already verified delivery.
        exportView.cancelExport();
        QCOMPARE(exportView.status_text(),
                 QStringLiteral("VERIFIED_DELIVERED"));
        QVERIFY(exportView.verified_delivered());
        QVERIFY(exportView.can_acknowledge());
        QVERIFY(exportChanges.count()>=2);
        QVERIFY(QFileInfo::exists(viewOutputPath));
        QVERIFY(exportView.acknowledgeExport());
        QCOMPARE(exportView.status_text(),QStringLiteral("IDLE"));
        QVERIFY(!exportView.can_acknowledge());
        QVERIFY(!exportView.can_cancel());
        QVERIFY(!exportView.acknowledgeExport());

        // A second attempt to use the same published filename must
        // surface a FAILED state and retain the previous WAV untouched.
        // Pressing Cancel afterward must NOT relabel that verification/
        // collision failure as a successful user cancellation.
        QVERIFY(exportView.begin_private_export(
            product,selectedSource,selectedProjectRate,candidates,
            *boundTicket.value(),liveSource->reference(),
            *viewOutputRef.value(),audio::WavSampleFormat::IEEE_F32,
            std::nullopt,0.0));
        QTRY_VERIFY_WITH_TIMEOUT(
            exportView.status_text()==QStringLiteral("FAILED"),30000);
        QVERIFY(!exportView.error_text().isEmpty());
        QVERIFY(exportView.can_acknowledge());
        QVERIFY(!exportView.can_cancel());
        exportView.cancelExport();
        QCOMPARE(exportView.status_text(),QStringLiteral("FAILED"));
        QVERIFY(exportView.acknowledgeExport());
        QCOMPARE(exportView.status_text(),QStringLiteral("IDLE"));

        // A denied preflight does not start a worker, emit a phantom
        // export result or occupy the one-job lifecycle state.
        app::SourceSelectionViewModel emptyViewSource;
        const auto deniedWithoutSource=exportView.begin_private_export(
            product,emptyViewSource,selectedProjectRate,candidates,
            *boundTicket.value(),liveSource->reference(),
            *viewOutputRef.value(),audio::WavSampleFormat::IEEE_F32,
            std::nullopt,0.0);
        QVERIFY(!deniedWithoutSource);
        QCOMPARE(exportView.status_text(),QStringLiteral("IDLE"));

        // Worker threads may only see the immutable M18 request and
        // atomic progress, never ask the live Qt ViewModel to mint a
        // new Product export or acknowledge a prior terminal outcome.
        const auto ownerSourceRef=liveSource->reference();
        const auto ownerDestinationRef=*viewOutputRef.value();
        bool refusedCrossThreadStart=false;
        bool refusedCrossThreadAck=false;
        std::thread rogueExportCaller([&] {
            const auto denied=exportView.begin_private_export(
                product,selectedSource,selectedProjectRate,candidates,
                *boundTicket.value(),ownerSourceRef,ownerDestinationRef,
                audio::WavSampleFormat::IEEE_F32,std::nullopt,0.0);
            refusedCrossThreadStart=!denied
                &&denied.error()->code()==core::ErrorCode::InvalidState;
            refusedCrossThreadAck=!exportView.acknowledgeExport();
        });
        rogueExportCaller.join();
        QVERIFY(refusedCrossThreadStart);
        QVERIFY(refusedCrossThreadAck);
        QCOMPARE(exportView.status_text(),QStringLiteral("IDLE"));

        // A ticket and verified M17 MASTER do not authorize export while
        // the Product has NO real current SOURCE selected in its QObject.
        app::SourceSelectionViewModel noCurrentProductSource;
        const auto deniedMissingCurrentSource=
            app::internal::make_m18_checked_stage_worker_request_from_live_product(
                product,noCurrentProductSource,selectedProjectRate,candidates,
                *boundTicket.value(),liveSource->reference(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!deniedMissingCurrentSource);
        app::MasterOutputRateState noCurrentProductRate;
        const auto deniedMissingCurrentRate=
            app::internal::make_m18_checked_stage_worker_request_from_live_product(
                product,selectedSource,noCurrentProductRate,candidates,
                *boundTicket.value(),liveSource->reference(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!deniedMissingCurrentRate);
        QVERIFY(correctProductWorkerRequest.value()->payload()
                .channel(0U).value()->data()
            ==m17View.channel(0U).value()->data());
        // Float32 is a second valid delivery encoding from the SAME
        // immutable final M17 MASTER: never dither Float32 or re-render.
        const auto float32ProductWorkerRequest=
            app::internal::make_m18_checked_stage_worker_request_from_live_product(
                product,selectedSource,selectedProjectRate,candidates,
                *boundTicket.value(),liveSource->reference(),*dst.value(),
                audio::WavSampleFormat::IEEE_F32,std::nullopt,-1.0);
        QVERIFY(float32ProductWorkerRequest);
        QCOMPARE(float32ProductWorkerRequest.value()->sample_format(),
                 audio::WavSampleFormat::IEEE_F32);
        QVERIFY(!float32ProductWorkerRequest.value()->dither_seed());
        QVERIFY(float32ProductWorkerRequest.value()->payload()
                .channel(0U).value()->data()
            ==m17View.channel(0U).value()->data());
        // No implicit PCM24 dither seed and no Float32 dither:
        // invalid encoding/seed combinations fail before worker dispatch.
        const auto missingPcm24Seed=
            app::internal::make_m18_checked_stage_worker_request_from_live_product(
                product,selectedSource,selectedProjectRate,candidates,
                *boundTicket.value(),liveSource->reference(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,std::nullopt,-1.0);
        QVERIFY(!missingPcm24Seed);
        const auto forbiddenFloat32Seed=
            app::internal::make_m18_checked_stage_worker_request_from_live_product(
                product,selectedSource,selectedProjectRate,candidates,
                *boundTicket.value(),liveSource->reference(),*dst.value(),
                audio::WavSampleFormat::IEEE_F32,UINT64_C(42),-1.0);
        QVERIFY(!forbiddenFloat32Seed);
        const auto falseProductSource=core::ResourceReference::create(
            liveSource->reference().provider_id(),
            liveSource->reference().locator()+".stale",
            true,false,"stale-product-source");
        QVERIFY(falseProductSource);
        const auto deniedWrongProductSource=
            app::internal::make_m18_checked_stage_worker_request_from_live_product(
                product,selectedSource,selectedProjectRate,candidates,
                *boundTicket.value(),*falseProductSource.value(),
                *dst.value(),audio::WavSampleFormat::PCM_S24,
                UINT64_C(42),-1.0);
        QVERIFY(!deniedWrongProductSource);
        app::MasterOutputRateState differentProjectMaster;
        differentProjectMaster.source_committed(48000);
        const auto deniedWrongProjectMaster=
            app::internal::make_m18_checked_stage_worker_request_from_live_product(
                product,selectedSource,differentProjectMaster,candidates,
                *boundTicket.value(),liveSource->reference(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!deniedWrongProjectMaster);
        // UI-only Product stage/rate/SOURCE lookup must be denied before
        // any QObject access when invoked from an export worker.
        const auto verifiedCurrentSource=liveSource->reference();
        bool workerRejectedLiveProduct=false;
        std::thread illicitExportWorker([&] {
            const auto denied=
                app::internal::make_m18_checked_stage_worker_request_from_live_product(
                    product,selectedSource,selectedProjectRate,candidates,
                    *boundTicket.value(),verifiedCurrentSource,*dst.value(),
                    audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
            workerRejectedLiveProduct=!denied
                &&denied.error()->code()==core::ErrorCode::InvalidState;
        });
        illicitExportWorker.join();
        QVERIFY(workerRejectedLiveProduct);
        // Re-selecting SOURCE at the same sample rate is still a new
        // Product identity. The old ticket must not be able to publish
        // the retained old M17 buffer even if Fs and locator match.
        const auto previousRevision=selectedProjectRate.rate_revision();
        selectedProjectRate.source_committed(
            selectedSource.sample_rate_hz());
        QCOMPARE(selectedProjectRate.rate_revision(),previousRevision+1);
        const auto staleSameRateProduct=
            app::internal::make_m18_checked_stage_worker_request_from_live_product(
                product,selectedSource,selectedProjectRate,candidates,
                *boundTicket.value(),verifiedCurrentSource,*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!staleSameRateProduct);
        QCOMPARE(staleSameRateProduct.error()->code(),
                 core::ErrorCode::InvalidState);
        // The preproduction typed handoff consumes a REAL checked M17
        // Product candidate plus the Qt-free worker epoch Ticket. It may
        // never copy Master PCM or forge a new terminal realization.
        const auto ownerBoundRequest=
            app::internal::make_m18_checked_stage_worker_request(
                candidates,original,*masterTicket.value(),
                *src.value(),*src.value(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,
                UINT64_C(42),-1.0);
        QVERIFY(ownerBoundRequest);
        // Exact current-Source provenance is distinct from a worker's
        // asserted file path. Even a real M17 stage and valid Product epoch
        // cannot authorize writing PCM for a different current SOURCE.
        auto otherSource=core::ResourceReference::create(
            src.value()->provider_id(),src.value()->locator()+".other",
            true,false,"other-source");
        QVERIFY(otherSource);
        const auto wrongSourceRequest=
            app::internal::make_m18_checked_stage_worker_request(
                candidates,original,*masterTicket.value(),
                *otherSource.value(),*src.value(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!wrongSourceRequest);
        QCOMPARE(wrongSourceRequest.error()->code(),
                 core::ErrorCode::InvalidState);
        // Same opaque locator but a different provider is NOT identical
        // SOURCE provenance (nor a license to reinterpret local paths).
        auto foreignProvider=core::ResourceReference::create(
            "test.other-provider",src.value()->locator(),true,false,
            "foreign-provider-source");
        QVERIFY(foreignProvider);
        const auto foreignProviderRequest=
            app::internal::make_m18_checked_stage_worker_request(
                candidates,original,*masterTicket.value(),
                *foreignProvider.value(),*src.value(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!foreignProviderRequest);
        QCOMPARE(foreignProviderRequest.error()->code(),
                 core::ErrorCode::InvalidState);
        auto readDenied=core::ResourceReference::create(
            src.value()->provider_id(),src.value()->locator(),
            false,true,"unreadable-source");
        QVERIFY(readDenied);
        const auto unreadableRequest=
            app::internal::make_m18_checked_stage_worker_request(
                candidates,original,*masterTicket.value(),
                *readDenied.value(),*src.value(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!unreadableRequest);
        QCOMPARE(unreadableRequest.error()->code(),
                 core::ErrorCode::InvalidState);
        auto unreadableWorkerSource=core::ResourceReference::create(
            src.value()->provider_id(),src.value()->locator(),
            false,true,"unreadable-worker-source");
        QVERIFY(unreadableWorkerSource);
        const auto deniedWorkerSource=
            app::internal::make_m18_checked_stage_worker_request(
                candidates,original,*masterTicket.value(),
                *src.value(),*unreadableWorkerSource.value(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!deniedWorkerSource);
        QCOMPARE(deniedWorkerSource.error()->code(),
                 core::ErrorCode::InvalidState);
        // A write-authorized reference still may not alias the exact
        // SOURCE provider/locator. Physical aliases are checked by Windows
        // again at commit time.
        auto sameFileDest=core::ResourceReference::create(
            src.value()->provider_id(),src.value()->locator(),
            false,true,"forbidden-alias");
        QVERIFY(sameFileDest);
        const auto aliasDestination=
            app::internal::make_m18_checked_stage_worker_request(
                candidates,original,*masterTicket.value(),
                *src.value(),*src.value(),*sameFileDest.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!aliasDestination);
        QCOMPARE(aliasDestination.error()->code(),
                 core::ErrorCode::InvalidState);
        auto noWriteDest=core::ResourceReference::create(
            dst.value()->provider_id(),dst.value()->locator(),
            true,false,"forbidden-read-only-destination");
        QVERIFY(noWriteDest);
        const auto unwritableDestination=
            app::internal::make_m18_checked_stage_worker_request(
                candidates,original,*masterTicket.value(),
                *src.value(),*src.value(),*noWriteDest.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!unwritableDestination);
        QCOMPARE(unwritableDestination.error()->code(),
                 core::ErrorCode::InvalidState);
        // A retained M17 stage from another/retired Product candidate gate
        // can never use a current ticket to publish a different MASTER.
        app::TerminalCandidateGate detachedGate;
        const auto detachedRequest=
            app::internal::make_m18_checked_stage_worker_request(
                detachedGate,original,*masterTicket.value(),
                *src.value(),*src.value(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,
                UINT64_C(42),-1.0);
        QVERIFY(!detachedRequest);
        QCOMPARE(detachedRequest.error()->code(),
                 core::ErrorCode::InvalidState);
        const auto aliasView=ownerBoundRequest.value()->payload();
        const auto originalStageView=original->rendered.terminal.checked
            .terminal.compensated_pcm.view();
        QCOMPARE(aliasView.frame_count(),originalStageView.frame_count());
        const auto ownerBoundLeft=aliasView.channel(0U);
        const auto originalLeftBefore=originalStageView.channel(0U);
        QVERIFY(ownerBoundLeft&&originalLeftBefore);
        QVERIFY(ownerBoundLeft.value()->data()
                ==originalLeftBefore.value()->data());
        // A ticket for the DEVICE rate must NEVER borrow a separately
        // verified final MASTER at a different chosen output rate.
        auto wrongMasterIdentity=verifiedId;
        wrongMasterIdentity.master_rate_hz=48000U;
        M18ProductEpochTicketAuthority wrongRateOwner{
            888U,wrongMasterIdentity};
        QVERIFY(wrongRateOwner.try_transition(
            888U,889U,wrongMasterIdentity,true));
        auto wrongRateTicket=wrongRateOwner.capture(wrongMasterIdentity);
        QVERIFY(wrongRateTicket);
        const auto deniedWrongRate=
            app::internal::make_m18_checked_stage_worker_request(
                candidates,original,*wrongRateTicket.value(),
                *src.value(),*src.value(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!deniedWrongRate);
        QCOMPARE(deniedWrongRate.error()->code(),
                 core::ErrorCode::InvalidState);
        // Same output Fs, different chain revision: never alias PCM.
        auto wrongChainIdentity=verifiedId;
        ++wrongChainIdentity.chain_revision;
        M18ProductEpochTicketAuthority wrongChainOwner{
            890U,wrongChainIdentity};
        QVERIFY(wrongChainOwner.try_transition(
            890U,891U,wrongChainIdentity,true));
        auto wrongChainTicket=wrongChainOwner.capture(wrongChainIdentity);
        QVERIFY(wrongChainTicket);
        const auto deniedWrongChain=
            app::internal::make_m18_checked_stage_worker_request(
                candidates,original,*wrongChainTicket.value(),
                *src.value(),*src.value(),*dst.value(),
                audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0);
        QVERIFY(!deniedWrongChain);
        QCOMPARE(deniedWrongChain.error()->code(),
                 core::ErrorCode::InvalidState);
        const auto workerFactory=masterTicket.value()->commit_lease_factory();
        auto heldCommit=workerFactory();
        QVERIFY(heldCommit);
        std::size_t interlockedAttempts=0;
        const auto commonAdmission=[&](const char*)->core::Status {
            ++interlockedAttempts;
            return sharedEpoch.try_transition(
                778U,779U,verifiedId,false);
        };
        product.set_private_m18_mutation_admission(commonAdmission);
        candidates.set_private_m18_mutation_admission(commonAdmission);
        selector.set_private_m18_mutation_admission(commonAdmission);
        guardedRate.set_private_m18_rate_mutation_admission([&] {
            return static_cast<bool>(commonAdmission("master_rate_edit"));
        });
        app::TerminalLimiterViewModel guardedLimiter{limiter.value()};
        guardedLimiter.set_private_m18_processing_mutation_admission([&] {
            return static_cast<bool>(commonAdmission("m17_limiter_edit"));
        });
        const auto originalBypass=guardedLimiter.bypass();
        guardedLimiter.setBypass(!originalBypass);
        QCOMPARE(guardedLimiter.bypass(),originalBypass);
        const auto blockedCandidateInvalidation=
            candidates.try_invalidate_for_m18();
        const auto rendererStatusBefore=renderer.status();
        const auto renderCountBefore=renderer.render_invocation_count();
        const auto blockedRendererInvalidation=
            renderer.try_invalidate_for_m18();
        // A second M17 Prepare while a verified M18 worker owns the
        // file-commit epoch must ALSO refuse before clearing the MASTER.
        const auto blockedSecondPrepare=
            renderer.request_private(*masterRate.value());
        const auto blockedProductCancel=product.cancel();
        const auto blockedUpstreamSource=selector.switch_to(
            app::AuditionTarget::PREPARED);
        const auto selectedRateBefore=guardedRate.master_rate_hz();
        const auto blockedProjectRate=guardedRate.choose_master_rate(48000);
        QVERIFY(!blockedCandidateInvalidation);
        QVERIFY(!blockedRendererInvalidation);
        QVERIFY(!blockedSecondPrepare);
        QCOMPARE(blockedSecondPrepare.error()->code(),
                 core::ErrorCode::InvalidState);
        QCOMPARE(renderer.status(),rendererStatusBefore);
        QCOMPARE(renderer.render_invocation_count(),renderCountBefore);
        QVERIFY(!blockedProductCancel);
        QVERIFY(!blockedUpstreamSource);
        QVERIFY(!blockedProjectRate);
        QCOMPARE(interlockedAttempts,std::size_t{7});
        QVERIFY(candidates.staged()==original);
        QCOMPARE(guardedRate.master_rate_hz(),selectedRateBefore);
        QVERIFY((*heldCommit.value())->revalidate_current());
        heldCommit.value()->reset();
        QVERIFY(sharedEpoch.try_transition(778U,779U,verifiedId,false));
        QVERIFY(!workerFactory()); // no re-admission after revocation
        product.set_private_m18_mutation_admission({});
        candidates.set_private_m18_mutation_admission({});
        selector.set_private_m18_mutation_admission({});
        guardedRate.set_private_m18_rate_mutation_admission({});
        guardedLimiter.set_private_m18_processing_mutation_admission({});

        // A foreign export worker or a reentrant callback cannot disarm
        // the limiter Product guard and mutate the verified M17 stage.
        std::size_t limiterGuardCalls=0,limiterNestedDenied=0;
        guardedLimiter.set_private_m18_processing_mutation_admission([&] {
            ++limiterGuardCalls;
            guardedLimiter.set_private_m18_processing_mutation_admission({});
            const auto bypassBeforeNested=guardedLimiter.bypass();
            guardedLimiter.setBypass(!bypassBeforeNested);
            if(guardedLimiter.bypass()==bypassBeforeNested)
                ++limiterNestedDenied;
            return false;
        });
        std::thread foreignLimiterDisarm([&] {
            guardedLimiter.set_private_m18_processing_mutation_admission({});
        });
        foreignLimiterDisarm.join();
        guardedLimiter.setBypass(!originalBypass);
        guardedLimiter.setBypass(!originalBypass);
        QCOMPARE(guardedLimiter.bypass(),originalBypass);
        QCOMPARE(limiterGuardCalls,std::size_t{2});
        QCOMPARE(limiterNestedDenied,std::size_t{2});
        guardedLimiter.set_private_m18_processing_mutation_admission({});

        const auto originalPcm=original->rendered.terminal.checked.terminal
            .compensated_pcm.view();
        const auto requestedPcm=authorized.value()->payload();
        QCOMPARE(requestedPcm.frame_count(),originalPcm.frame_count());
        QVERIFY(requestedPcm.format()==originalPcm.format());
        const auto originalLeft=originalPcm.channel(0U);
        const auto requestedLeft=requestedPcm.channel(0U);
        QVERIFY(originalLeft && requestedLeft);
        // Alias retention of EXACT original M17 Master; never copy/render.
        QVERIFY(originalLeft.value()->data()==requestedLeft.value()->data());
        const auto commitLease=authorized.value()->commit_lease_factory()();
        QVERIFY(commitLease);
        QVERIFY((*commitLease.value())->revalidate_current());
        // A caller on a worker thread MUST NOT obtain a Product export
        // request or revalidate the existing lease: Qt owner affinity is
        // a hard contract, not a silent post-consent race.
        bool workerDenied=false;
        std::thread worker([&] {
            const auto offThread=
                product.make_private_m18_synchronous_export_request(
                    *masterRate.value(),*src.value(),*dst.value(),
                    audio::WavSampleFormat::PCM_S24,UINT64_C(42),-1.0,approve);
            const auto offThreadLease=
                (*commitLease.value())->revalidate_current();
            workerDenied=!offThread
                &&offThread.error()->code()==core::ErrorCode::InvalidState
                &&!offThreadLease
                &&offThreadLease.error()->code()==core::ErrorCode::InvalidState;
        });
        worker.join();
        QVERIFY(workerDenied);
        consentCurrent=false;
        // Product permission is checked again AT COMMIT, not merely at
        // request capture. The held lease may no longer authorize file IO.
        const auto revoked=(*commitLease.value())->revalidate_current();
        QVERIFY(!revoked);
        QCOMPARE(revoked.error()->code(),core::ErrorCode::AccessDenied);
        QVERIFY(!authorized.value()->commit_lease_factory()());
        QVERIFY(consentChecks>=5U);
    }
    QCOMPARE(monitorUi.master_rate_hz(),int{192000});
    QVERIFY(monitorUi.can_install());
    QSignalSpy installEligibility{&monitorUi,&app::TerminalMonitorViewModel::changed};
    QVERIFY(installEligibility.isValid());
    transport.playOrResume();
    QVERIFY(monitorUi.can_install()); // PLAYING is a valid mapped handoff.
    transport.stop();
    QVERIFY(monitorUi.can_install());
    const auto reusedNormal=product.materialize_private_final_master_monitoring(
        [&](audio::AudioBufferView master)
            ->core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>> {
            using Owned=std::shared_ptr<const audio::MonitoringPcmCandidate>;
            using Result=core::Result<Owned>;
            auto made=audio::materialize_monitoring_pcm_candidate(
                master,*deviceRate.value(),*block.value());
            if(!made)return Result::failure(*made.error());
            return Result::success(std::make_shared<
                const audio::MonitoringPcmCandidate>(
                    std::move(*made.value())));
        });
    QVERIFY2(reusedNormal,reusedNormal?"":reusedNormal.error()->message().c_str());
    QCOMPARE(renderer.render_invocation_count(),beforeNormalM17+1);
    // Device-only SRC did not retrigger MASTER preparation or replace the
    // accepted actual per-stage timing evidence.
    auto stillObserved=renderer.last_approved_prepare_timing();
    QVERIFY(stillObserved.has_value());
    QCOMPARE(stillObserved->render_wall_us,
             observedPrepare->render_wall_us);
    QCOMPARE((*reusedNormal.value())->immutable_master_rate,
        *masterRate.value());
    QCOMPARE((*reusedNormal.value())->device_rate,*deviceRate.value());
    // Two-phase UI-thread authority for a future nonblocking converter.
    // A fabricated absent MASTER token cannot publish even a valid PCM.
    const auto capturedForAsync=product.capture_monitor_master_for_conversion();
    QVERIFY(capturedForAsync);
    QVERIFY(capturedForAsync==candidates.staged());
    auto rejectedCapture=product.admit_converted_monitor_owner(
        {},*reusedNormal.value());
    QVERIFY(!rejectedCapture);
    QCOMPARE(rejectedCapture.error()->code(),core::ErrorCode::InvalidState);
    QVERIFY(product.admit_converted_monitor_owner(
        capturedForAsync,*reusedNormal.value()));
    // A DSP conversion worker may hold/copy the immutable MASTER ticket,
    // but MUST NOT call the QObject-owned source/DEVICE admission there.
    // Verify refusal before any access to transport/session mutable state.
    bool foreignCaptureDenied=false;
    bool foreignAdmissionDenied=false;
    std::jthread foreignThread([&] {
        foreignCaptureDenied=!product.capture_monitor_master_for_conversion();
        const auto refused=product.admit_converted_monitor_owner(
            capturedForAsync,*reusedNormal.value());
        foreignAdmissionDenied=!refused &&refused.error()
            &&refused.error()->code()==core::ErrorCode::InvalidState;
    });
    foreignThread.join();
    QVERIFY(foreignCaptureDenied);
    QVERIFY(foreignAdmissionDenied);
    // The owning thread can still admit the exact unchanged ticket.
    QVERIFY(product.admit_converted_monitor_owner(
        capturedForAsync,*reusedNormal.value()));
    // Admission does NOT call the DEVICE sink or create a RealizationId.
    QCOMPARE(sinkInstalls,std::size_t{0});
    // Install exactly this borrowed candidate into the fake anonymous
    // DEVICE sink, then return to exact MANUAL without destroying M17's
    // immutable checked PCM. No new M17 Prepare and no terminal identity.
    std::shared_ptr<const audio::MonitoringPcmCandidate> monitorSinkOwner;
    bool injectedBorrowedInstallFailure=false;
    std::optional<platform::windows::MonitoringTransportAction> injectedSeedFailure;
    product.set_private_monitor_transport(
        [device,&monitorSinkOwner,&injectedBorrowedInstallFailure]
            (auto candidate)->core::Status {
            if(!candidate||monitorSinkOwner)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M17_BORROWED_MONITOR_SINK_ALREADY_OWNED"});
            monitorSinkOwner=candidate;
            device->state=core::PlaybackState::STOPPED;
            device->position=core::FrameIndex{0};
            device->duration=candidate->device_pcm.view().frame_count();
            device->loop.reset();
            device->audibleRealization={
                core::AudibleHandoffPhase::UNAVAILABLE,std::nullopt};
            if(injectedBorrowedInstallFailure) {
                injectedBorrowedInstallFailure=false;
                return core::Status::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "TEST_INSTALL_FAILED_AFTER_PLAYING_MANUAL_REPLACED"});
            }
            return core::Status::success();
        },
        [&monitorSinkOwner](auto candidate)->core::Result<bool> {
            return core::Result<bool>::success(
                monitorSinkOwner && monitorSinkOwner==candidate);
        },
        [device,&monitorSinkOwner](auto candidate)->core::Status {
            if(!candidate||candidate!=monitorSinkOwner)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "M17_BORROWED_MONITOR_SINK_OWNER_MISMATCH"});
            monitorSinkOwner.reset();
            device->state=core::PlaybackState::NO_SOURCE;
            device->position=core::FrameIndex{0};
            device->duration.reset();
            device->loop.reset();
            device->audibleRealization={
                core::AudibleHandoffPhase::UNAVAILABLE,std::nullopt};
            return core::Status::success();
        },
        [&selector](auto manual,core::RealizationId id,
                    core::FrameIndex cue)->core::Status {
            return selector.restore_manual_processed_after_terminal(
                std::move(manual),id,cue);
        });
    const auto normalM17Stage=candidates.staged();
    QVERIFY(normalM17Stage);
    QVERIFY(product.install_private_monitor_stopped(*reusedNormal.value()));
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED"));
    QVERIFY(monitorUi.monitor_installed());
    QVERIFY(monitorUi.monitor_exclusive());
    QCOMPARE(monitorSinkOwner,*reusedNormal.value());
    QCOMPARE(candidates.staged(),normalM17Stage);
    QCOMPARE(renderer.render_invocation_count(),beforeNormalM17+1);
    QCOMPARE(sinkInstalls,std::size_t{0});
    // End-to-end guard: the authoritative Project rate stays at the
    // immutable DEVICE-owned MASTER until the exact Return completes.
    app::MasterOutputRateState guardedProject;
    guardedProject.source_committed(192000);
    guardedProject.set_target_selection_guard([&product]() noexcept {
        return !product.monitoring_transaction_active();
    });
    const auto guardedRevision=guardedProject.rate_revision();
    QVERIFY(!guardedProject.can_edit_target());
    QVERIFY(!guardedProject.choose_master_rate(96000));
    QCOMPARE(guardedProject.master_rate_hz(),qint64{192000});
    QCOMPARE(guardedProject.rate_revision(),guardedRevision);
    QCOMPARE(renderer.render_invocation_count(),beforeNormalM17+1);
    QVERIFY(monitorUi.returnManual());
    QVERIFY(guardedProject.can_edit_target());
    QVERIFY(guardedProject.choose_master_rate(96000));
    QCOMPARE(guardedProject.rate_revision(),guardedRevision+1);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(candidates.staged(),normalM17Stage);
    QCOMPARE(renderer.render_invocation_count(),beforeNormalM17+1);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QVERIFY(!monitorSinkOwner);
    QVERIFY(monitorUi.can_install());

    // M17 asynchronous Master audition PRODUCT path: a true off-thread
    // full-duration FIR (not a fake ready PCM) with Qt-owner-only preflight
    // and fresh post-conversion recheck. No second M17 Prepare and no UI
    // thread allocation of the DEVICE PCM.
    std::size_t asyncPreflights=0;
    std::size_t asyncRechecks=0;
    monitorUi.set_async_monitoring_device(
        [&](audio::AudioBufferView master,bool allowCaution)
            ->core::Result<platform::windows::MonitoringAsyncDevicePlan> {
            ++asyncPreflights;
            using PlanResult=core::Result<
                platform::windows::MonitoringAsyncDevicePlan>;
            if(allowCaution
               ||master.format().sample_rate()!=*masterRate.value())
                return PlanResult::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "ASYNC_TEST_INVALID_MASTER_OR_CAUTION"});
            return PlanResult::success({
                *deviceRate.value(),"fixed-fake-physical-endpoint"
            });
        },
        [&](const platform::windows::MonitoringAsyncDevicePlan& plan,
            audio::AudioBufferView master,bool allowCaution)->core::Status {
            ++asyncRechecks;
            if(allowCaution ||plan.device_rate!=*deviceRate.value()
               ||plan.opaque_endpoint_id!="fixed-fake-physical-endpoint"
               ||master.format().sample_rate()!=*masterRate.value())
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,"ASYNC_TEST_ENDPOINT_CHANGED"});
            return core::Status::success();
        });
    const auto beforeAsyncRender=renderer.render_invocation_count();
    QVERIFY(monitorUi.installMonitor(false));
    QVERIFY(monitorUi.monitor_preparing());
    QVERIFY(monitorUi.property("monitorPreparing").toBool());
    QCOMPARE(monitorUi.device_preparation_fraction(),0.0);
    QVERIFY(!monitorUi.device_preparation_eta().isEmpty());
    // The original SOURCE transport remains interactive during a whole-song
    // DEVICE FIR worker: PLAY and STOP only change transport, not M17
    // immutable PCM or the device-owner ticket. These are synchronous UI
    // commands while the worker is still PREPARING (before queued completion).
    transport.playOrResume();
    QVERIFY(monitorUi.monitor_preparing());
    transport.stop();
    QVERIFY(monitorUi.monitor_preparing());
    QVERIFY(monitorUi.can_dismiss());
    QVERIFY(!monitorUi.can_install());
    QVERIFY(!monitorUi.monitor_installed());
    QTRY_VERIFY_WITH_TIMEOUT(monitorUi.monitor_installed(),12000);
    QVERIFY(!monitorUi.monitor_preparing());
    QCOMPARE(monitorUi.device_preparation_fraction(),0.0);
    QVERIFY(!monitorUi.device_preparation_determinate());
    QCOMPARE(asyncPreflights,std::size_t{1});
    QCOMPARE(asyncRechecks,std::size_t{1});
    QVERIFY(monitorSinkOwner);
    QCOMPARE(monitorSinkOwner->windows_prepared_endpoint_id,
        std::string{"fixed-fake-physical-endpoint"});
    QCOMPARE(renderer.render_invocation_count(),beforeAsyncRender);
    QVERIFY(monitorUi.returnManual());
    QVERIFY(!monitorUi.master_loop_enabled());
    QCOMPARE(monitorUi.master_region_end_frames(),qint64{0});
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QVERIFY(!monitorSinkOwner);
    // Same immutable MASTER and same physical endpoint: reuse the already
    // checked DEVICE PCM. It must install synchronously, without another
    // full-track device FIR or a second M17 whole-Prepare.
    QVERIFY(monitorUi.installMonitor(false));
    QVERIFY(!monitorUi.monitor_preparing());
    QVERIFY(monitorUi.monitor_installed());
    QCOMPARE(asyncPreflights,std::size_t{2}); // fresh admission, not stale
    QCOMPARE(asyncRechecks,std::size_t{2});
    QCOMPARE(renderer.render_invocation_count(),beforeAsyncRender);
    QVERIFY(monitorUi.returnManual());
    QVERIFY(!monitorSinkOwner);
    // UNKNOWN or DISCOURAGED memory advice must be refused BEFORE the
    // jthread even begins; the checked M17 MASTER stays ready for retry.
    monitorUi.set_async_monitoring_device(
        [](audio::AudioBufferView,bool)->core::Result<
            platform::windows::MonitoringAsyncDevicePlan> {
            return core::Result<
                platform::windows::MonitoringAsyncDevicePlan>::failure(
                    core::Error{core::ErrorCode::InvalidState,
                        "MONITOR_MEMORY_NOT_APPROVED_OR_UNAVAILABLE"});
        },
        [](const platform::windows::MonitoringAsyncDevicePlan&,
            audio::AudioBufferView,bool)->core::Status {
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "MONITOR_ASYNC_RECHECK_MUST_NEVER_RUN"});
        });
    QVERIFY(!monitorUi.installMonitor(false));
    QVERIFY(!monitorUi.monitor_preparing());
    QVERIFY(!monitorSinkOwner);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(monitorUi.error_text(),
        QStringLiteral("MONITOR_MEMORY_NOT_APPROVED_OR_UNAVAILABLE"));
    QCOMPARE(renderer.render_invocation_count(),beforeAsyncRender);
    QCOMPARE(asyncRechecks,std::size_t{2});
    // A hardware endpoint can change to a different physical ID with the
    // SAME rate while the FIR runs. The second owner-thread probe MUST
    // refuse installation even though the audio bits are already computed.
    monitorUi.set_async_monitoring_device(
        [&](audio::AudioBufferView master,bool allowCaution)
            ->core::Result<platform::windows::MonitoringAsyncDevicePlan> {
            ++asyncPreflights;
            if(allowCaution
               ||master.format().sample_rate()!=*masterRate.value())
                return core::Result<
                    platform::windows::MonitoringAsyncDevicePlan>::failure(
                        core::Error{core::ErrorCode::InvalidState,
                            "ASYNC_TEST_PREPARE_OWNER_CHANGED"});
            return core::Result<
                platform::windows::MonitoringAsyncDevicePlan>::success({
                    *deviceRate.value(),"original-endpoint-id"
                });
        },
        [&](const platform::windows::MonitoringAsyncDevicePlan&,
            audio::AudioBufferView,bool)->core::Status {
            ++asyncRechecks;
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "MONITOR_ASYNC_DEVICE_ENDPOINT_OR_RATE_CHANGED_DURING_CONVERSION"});
        });
    QVERIFY(monitorUi.installMonitor(false));
    QVERIFY(monitorUi.monitor_preparing());
    QTRY_VERIFY_WITH_TIMEOUT(!monitorUi.monitor_preparing(),12000);
    QVERIFY(!monitorUi.monitor_installed());
    QVERIFY(!monitorSinkOwner);
    QCOMPARE(monitorUi.error_text(),QStringLiteral(
        "MONITOR_ASYNC_DEVICE_ENDPOINT_OR_RATE_CHANGED_DURING_CONVERSION"));
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(asyncRechecks,std::size_t{3});
    QVERIFY(monitorUi.can_install());
    // The cancel test below must still use a current, approvable endpoint;
    // a freshly captured identity is independent from the rejected one.
    monitorUi.set_async_monitoring_device(
        [&](audio::AudioBufferView master,bool)->core::Result<
            platform::windows::MonitoringAsyncDevicePlan> {
            ++asyncPreflights;
            if(master.format().sample_rate()!=*masterRate.value())
                return core::Result<
                    platform::windows::MonitoringAsyncDevicePlan>::failure(
                        core::Error{core::ErrorCode::InvalidState,
                            "ASYNC_TEST_MASTER_NOT_CURRENT"});
            return core::Result<
                platform::windows::MonitoringAsyncDevicePlan>::success({
                    *deviceRate.value(),"fixed-fake-physical-endpoint"
                });
        },
        [&](const platform::windows::MonitoringAsyncDevicePlan&,
            audio::AudioBufferView,bool)->core::Status {
            ++asyncRechecks;
            return core::Status::success();
        });
    // Accepted async preflight can be withdrawn before the Qt completion
    // posts. No eventual stale DEVICE PCM may be installed. A second click
    // must not launch a competing worker or re-probe a potentially changed
    // device while the first conversion still owns its immutable ticket.
    QVERIFY(monitorUi.installMonitor(false));
    QVERIFY(monitorUi.monitor_preparing());
    const auto beforeDuplicateProbe=asyncPreflights;
    QVERIFY(!monitorUi.installMonitor(false));
    QCOMPARE(asyncPreflights,beforeDuplicateProbe);
    QVERIFY(monitorUi.monitor_preparing());
    QVERIFY(monitorUi.cancel());
    QVERIFY(!monitorUi.monitor_preparing());
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    // Immediately after Cancel, the FIR worker can still be draining.
    // The command must remain disabled until it has actually exited,
    // without treating an old queued completion as DEVICE authorization.
    if(!monitorUi.can_install()) {
        QVERIFY(monitorUi.monitor_cancelling());
        QCOMPARE(monitorUi.property("monitorCancelling").toBool(),true);
        QVERIFY(monitorUi.status_text().contains(
            QStringLiteral("CANCELLING MASTER DEVICE PREPARE")));
    }
    QTRY_VERIFY_WITH_TIMEOUT(monitorUi.can_install(),12000);
    QVERIFY(!monitorUi.monitor_cancelling());
    QCOMPARE(monitorUi.property("monitorCancelling").toBool(),false);
    QVERIFY(!monitorSinkOwner);
    QCOMPARE(asyncRechecks,std::size_t{3});
    monitorUi.set_async_monitoring_device({}, {});

    // The asynchronous converter correctly superseded the old synchronous
    // DEVICE ticket. Even with bit-identical/master-matched PCM, the stale
    // owner is NOT legal for a subsequent manual transport handoff. An
    // explicit new owner admission from the SAME live immutable M17 stage
    // must precede the later stopped/playing/paused transport cases below.
    QVERIFY(!product.install_private_monitor_stopped(*reusedNormal.value()));
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    const auto afterAsyncCapture=
        product.capture_monitor_master_for_conversion();
    QVERIFY(afterAsyncCapture);
    QCOMPARE(afterAsyncCapture,normalM17Stage);
    const auto readmitted=
        product.admit_converted_monitor_owner(
            afterAsyncCapture,*reusedNormal.value());
    QVERIFY2(readmitted,readmitted?"":readmitted.error()->message().c_str());
    QVERIFY(!monitorSinkOwner);
    QCOMPARE(renderer.render_invocation_count(),beforeNormalM17+1);

    // Product P0 dynamic handoff. With an unchanged M17 READY stage we can
    // enter DEVICE audition at a nonzero MANUAL playhead while PLAYING or
    // PAUSED, and return to exactly the same SOURCE cue and original ID.
    // All monitor commands are checked against the exact DEVICE owner and
    // operate in MASTER coordinates, never retarget M17.
    std::optional<core::FrameRange> requestedMasterLoop;
    product.set_private_monitor_controls(
        [&](auto owner,auto request)->core::Status {
            if(owner!=monitorSinkOwner)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "TEST_DYNAMIC_MONITOR_WRONG_OWNER"});
            auto mapper=audio::MonitoringFrameMap::create(*owner);
            if(!mapper)return core::Status::failure(*mapper.error());
            using Action=platform::windows::MonitoringTransportAction;
            if(injectedSeedFailure && request.action==*injectedSeedFailure) {
                injectedSeedFailure.reset();
                return core::Status::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "TEST_MAPPED_COMMAND_FAILED_AFTER_DEVICE_REPLACEMENT"});
            }
            switch(request.action) {
            case Action::PLAY:return device->play();
            case Action::PAUSE:return device->pause();
            case Action::STOP:return device->stop();
            case Action::SEEK_MASTER: {
                auto pos=mapper.value()->master_to_device(request.master_frame);
                if(!pos)return core::Status::failure(*pos.error());
                return device->seek(*pos.value());
            }
            case Action::SET_MASTER_LOOP: {
                std::optional<core::FrameRange> region;
                if(request.master_loop) {
                    const auto converted=mapper.value()->master_loop_to_device(
                        *request.master_loop);
                    if(!converted)return core::Status::failure(*converted.error());
                    region=*converted.value();
                }
                const auto applied=device->set_loop(region);
                if(applied)requestedMasterLoop=request.master_loop;
                return applied;
            }
            }
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidArgument,"TEST_DYNAMIC_MONITOR_COMMAND"});
        },
        [&](auto owner)
            ->core::Result<audio::MonitoringMasterDisplaySnapshot> {
            using Display=audio::MonitoringMasterDisplaySnapshot;
            if(owner!=monitorSinkOwner)
                return core::Result<Display>::failure(core::Error{
                    core::ErrorCode::InvalidState,"TEST_DYNAMIC_DISPLAY_OWNER"});
            auto mapper=audio::MonitoringFrameMap::create(*owner);
            if(!mapper)return core::Result<Display>::failure(*mapper.error());
            const auto snap=device->snapshot();
            if(!snap)return core::Result<Display>::failure(*snap.error());
            return mapper.value()->project_display_snapshot(
                *snap.value(),requestedMasterLoop);
        });
    QVERIFY(transport.seek_source_frame(core::FrameIndex{800}));
    transport.playOrResume();
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    const auto renderCountBeforeDevice=renderer.render_invocation_count();
    QVERIFY(monitorUi.can_install());
    QVERIFY(product.install_private_monitor_stopped(*reusedNormal.value()));
    QCOMPARE(product.status(),QStringLiteral("MONITOR_PRIVATE_PLAYING"));
    QCOMPARE(device->position,core::FrameIndex{200});
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(renderer.render_invocation_count(),renderCountBeforeDevice);
    // This fixture installs its fake DEVICE-display snapshot only in the
    // P0 dynamic handoff section below, not during the earlier worker
    // admission smoke. Verify Region UI after the real test display seam
    // exists; checking masterDurationFrames before this point falsely
    // asserts zero even for a valid 1920-frame MASTER owner.
    monitorUi.refresh();
    // One permanently visible Region strip edits independent MASTER frames.
    // A draft interval does not affect SOURCE or DEVICE loop until enabled.
    const qint64 measuredMasterDuration=monitorUi.master_duration_frames();
    // The independent fake M17 MASTER can be a SHORT real frame sequence.
    // Always select legal half-open regions from measured master geometry,
    // never assume a large hardcoded count such as 960 frames.
    QVERIFY(measuredMasterDuration>=16);
    const qint64 firstStart=measuredMasterDuration/8;
    const qint64 firstEnd=measuredMasterDuration*7/8;
    const qint64 secondStart=measuredMasterDuration/4;
    const qint64 secondEnd=measuredMasterDuration*3/4;
    QVERIFY(firstStart<secondStart);
    QVERIFY(secondStart<secondEnd);
    QVERIFY(secondEnd<firstEnd);
    QCOMPARE(monitorUi.master_region_start_frames(),qint64{0});
    QCOMPARE(monitorUi.master_region_end_frames(),measuredMasterDuration);
    QVERIFY(!monitorUi.master_loop_enabled());
    QVERIFY(monitorUi.setMasterRegion(firstStart,firstEnd));
    QCOMPARE(monitorUi.master_region_start_frames(),firstStart);
    QCOMPARE(monitorUi.master_region_end_frames(),firstEnd);
    QVERIFY(!monitorUi.master_loop_enabled());
    QVERIFY(monitorUi.setMasterLoop(firstStart,firstEnd,true));
    QVERIFY(monitorUi.master_loop_enabled());
    QCOMPARE(monitorUi.master_region_start_frames(),firstStart);
    QCOMPARE(monitorUi.master_region_end_frames(),firstEnd);
    QVERIFY(monitorUi.setMasterRegion(secondStart,secondEnd));
    QVERIFY(monitorUi.master_loop_enabled());
    QCOMPARE(monitorUi.master_region_start_frames(),secondStart);
    QCOMPARE(monitorUi.master_region_end_frames(),secondEnd);
    QVERIFY(monitorUi.clearMasterRegion());
    QVERIFY(!monitorUi.master_loop_enabled());
    QCOMPARE(monitorUi.master_region_start_frames(),qint64{0});
    QCOMPARE(monitorUi.master_region_end_frames(),
        monitorUi.master_duration_frames());
    QVERIFY(monitorUi.returnManual());
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(device->position,core::FrameIndex{800});
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(renderer.render_invocation_count(),renderCountBeforeDevice);

    transport.pause();
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QVERIFY(monitorUi.can_install());
    QVERIFY(product.install_private_monitor_stopped(*reusedNormal.value()));
    QCOMPARE(product.status(),QStringLiteral("MONITOR_PRIVATE_PAUSED"));
    QCOMPARE(device->position,core::FrameIndex{200});
    QVERIFY(monitorUi.returnManual());
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(device->position,core::FrameIndex{800});
    QCOMPARE(renderer.render_invocation_count(),renderCountBeforeDevice);

    // A loop belongs to the MANUAL transport epoch, not to the DSP render
    // window. A new explicit Prepare after changing the loop produces one
    // M17 checked owner. DEVICE audition is always nonlooped; the original
    // SOURCE loop is preserved for the exact return to MANUAL.
    transport.stop();
    QVERIFY(product.cancel());
    // An arbitrarily late worker result must not revive a canceled M17
    // epoch, even if it carries a structurally valid DEVICE PCM owner.
    auto staleWorker=product.admit_converted_monitor_owner(
        capturedForAsync,*reusedNormal.value());
    QVERIFY(!staleWorker);
    QCOMPARE(staleWorker.error()->code(),core::ErrorCode::InvalidState);
    QVERIFY(!product.capture_monitor_master_for_conversion());
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    const auto sourceRegion=core::FrameRange::create(
        core::FrameIndex{120},core::FrameIndex{1200});
    QVERIFY(sourceRegion);
    selectedSourceLoop=*sourceRegion.value();
    QVERIFY(transport.set_loop_source_range(selectedSourceLoop));
    QVERIFY(transport.seek_source_frame(core::FrameIndex{500}));
    transport.playOrResume();
    QVERIFY(product.request_private(*masterRate.value(),true));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    const auto renderedWithLoop=renderer.render_invocation_count();
    const auto loopMonitor=product.materialize_private_final_master_monitoring(
        [&](audio::AudioBufferView master)
            ->core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>> {
            using Owner=std::shared_ptr<const audio::MonitoringPcmCandidate>;
            using Result=core::Result<Owner>;
            auto built=audio::materialize_monitoring_pcm_candidate(
                master,*deviceRate.value(),*block.value());
            if(!built)return Result::failure(*built.error());
            return Result::success(std::make_shared<
                const audio::MonitoringPcmCandidate>(std::move(*built.value())));
        });
    QVERIFY(loopMonitor);
    QVERIFY(monitorUi.can_install());
    QVERIFY(product.install_private_monitor_stopped(*loopMonitor.value()));
    QCOMPARE(product.status(),QStringLiteral("MONITOR_PRIVATE_PLAYING"));
    QCOMPARE(device->position,core::FrameIndex{125});
    QVERIFY(!requestedMasterLoop);
    QVERIFY(!device->loop); // Loop is SOURCE-only, never inherited by Master.
    QCOMPARE(renderer.render_invocation_count(),renderedWithLoop);
    QVERIFY(monitorUi.returnManual());
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->position,core::FrameIndex{500});
    QCOMPARE(device->loop,selectedSourceLoop);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(renderer.render_invocation_count(),renderedWithLoop);
    // P0 adversarial: failure AFTER replacing PLAYING + looped MANUAL
    // restores precisely the original SOURCE transport, not STOPPED/zero.
    const auto loopStage=candidates.staged();
    const auto originalRenderCount=renderer.render_invocation_count();
    injectedBorrowedInstallFailure=true;
    QVERIFY(!product.install_private_monitor_stopped(*loopMonitor.value()));
    QVERIFY(!monitorSinkOwner);
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->position,core::FrameIndex{500});
    QCOMPARE(device->loop,selectedSourceLoop);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(candidates.staged(),loopStage);
    QCOMPARE(renderer.render_invocation_count(),originalRenderCount);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));

    // Each mapped seeding step is its own DEVICE owner-thread action.
    // Master no longer inherits a SOURCE Loop, so only SEEK/PLAY seed
    // failures enter the partial-owner recovery path.
    using SeedAction=platform::windows::MonitoringTransportAction;
    for(const auto failingAction:{
            SeedAction::SEEK_MASTER,SeedAction::PLAY}) {
        requestedMasterLoop.reset();
        injectedSeedFailure=failingAction;
        QVERIFY(!product.install_private_monitor_stopped(*loopMonitor.value()));
        QVERIFY(!injectedSeedFailure);
        QCOMPARE(product.status(),
            QStringLiteral("MONITOR_PRIVATE_RECOVERY_REQUIRED"));
        QCOMPARE(monitorSinkOwner,*loopMonitor.value());
        QVERIFY(monitorUi.monitor_installed()); // Return must remain available.
        QVERIFY(!monitorUi.monitor_controllable()); // Partial sink is unsafe.
        QVERIFY(!monitorUi.play());
        QVERIFY(!monitorUi.seekMaster(250));
        QVERIFY(!monitorUi.setMasterLoop(200,800,true));
        QCOMPARE(monitorSinkOwner,*loopMonitor.value());
        QVERIFY(monitorUi.returnManual());
        QVERIFY(!monitorSinkOwner);
        QCOMPARE(device->state,core::PlaybackState::PLAYING);
        QCOMPARE(device->position,core::FrameIndex{500});
        QCOMPARE(device->loop,selectedSourceLoop);
        QCOMPARE(device->audibleRealization.realizationId,manualId);
        QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
        QVERIFY(!monitorUi.monitor_controllable());
        QCOMPARE(candidates.staged(),loopStage);
        QCOMPARE(renderer.render_invocation_count(),originalRenderCount);
    }
    // PAUSE failure is reached only after successful SEEK/PLAY seeds.
    // Revert to the original PAUSED state rather than falsely PLAYING.
    transport.pause();
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    requestedMasterLoop.reset();
    injectedSeedFailure=SeedAction::PAUSE;
    QVERIFY(!product.install_private_monitor_stopped(*loopMonitor.value()));
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_RECOVERY_REQUIRED"));
    QVERIFY(!monitorUi.monitor_controllable());
    QVERIFY(monitorUi.returnManual());
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(device->position,core::FrameIndex{500});
    QCOMPARE(device->loop,selectedSourceLoop);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QVERIFY(!monitorSinkOwner);
    QCOMPARE(candidates.staged(),loopStage);
    QCOMPARE(renderer.render_invocation_count(),originalRenderCount);
    QVERIFY(product.cancel());
    transport.stop();
    selectedSourceLoop.reset();
    QVERIFY(transport.set_loop_source_range(std::nullopt));
    product.set_private_monitor_controls({}, {});
    // The first, independently proven borrowed-monitor fixture installed
    // real fake DEVICE callbacks. Clear them explicitly before the second
    // fixture tests fail-closed behavior with NO transport callbacks.
    // Otherwise the old callbacks remain valid after cancel(), and the
    // later "must refuse missing transport" assertion is meaningless.
    product.set_private_monitor_transport({}, {}, {}, {});
    QVERIFY(monitorUi.can_prepare());
    QVERIFY(!monitorUi.can_install());
    QVERIFY(!monitorUi.play()); // source MANUAL is never anonymous MONITOR
    QVERIFY(monitorUi.can_dismiss());
    QVERIFY(!monitorUi.error_text().isEmpty());
    QVERIFY(monitorUi.cancel());
    QVERIFY(monitorUi.error_text().isEmpty());
    QVERIFY(!monitorUi.can_dismiss());
    QVERIFY(!monitorUi.requestMaster(12345)); // not a frozen MASTER rate
    QVERIFY(monitorUi.can_dismiss());
    QVERIFY(monitorUi.can_prepare());
    QVERIFY(monitorUi.cancel());
    QVERIFY(monitorUi.error_text().isEmpty());
    QVERIFY(!monitorUi.can_dismiss());
    // OFFLINE Prepare never requires STOPPED or disabled SOURCE loop:
    // a real 192-kHz checked MASTER may complete while MANUAL is playing
    // inside an unchanged Loop Region. Only the unfinished handoff to a
    // distinct DEVICE sink remains guarded by its own ownership contract.
    const auto listeningLoop=core::FrameRange::create(
        core::FrameIndex{120},core::FrameIndex{1300});
    QVERIFY(listeningLoop);
    selectedSourceLoop=*listeningLoop.value();
    QVERIFY(transport.set_loop_source_range(selectedSourceLoop));
    transport.playOrResume();
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    const auto preLoopPrepareCount=renderer.render_invocation_count();
    QVERIFY(product.request_private(*masterRate.value(),true));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
    QCOMPARE(renderer.render_invocation_count(),preLoopPrepareCount+1);
    // PO 31 Oct: change the LIVE Loop while the verified entire-track
    // MASTER is READY. This changes no SOURCE PCM, no M17 invocation and
    // no true-peak certificate. The later device handoff must map the
    // *new* SOURCE Loop, not the old captured one.
    const auto changedLoop=core::FrameRange::create(
        core::FrameIndex{160},core::FrameIndex{1100});
    QVERIFY(changedLoop);
    selectedSourceLoop=*changedLoop.value();
    QVERIFY(transport.set_loop_source_range(selectedSourceLoop));
    QVERIFY(product.adopt_pending_source_loop_without_rerender());
    QCOMPARE(renderer.render_invocation_count(),preLoopPrepareCount+1);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    selectedSourceLoop=*listeningLoop.value();
    QVERIFY(transport.set_loop_source_range(selectedSourceLoop));
    QVERIFY(product.adopt_pending_source_loop_without_rerender());
    QCOMPARE(renderer.render_invocation_count(),preLoopPrepareCount+1);
    // Two accepted identical-key REAL Prepare wall times are sufficient for
    // an empirical observed range. Unrelated rates/ceilings must not borrow it.
    const auto sameShape=renderer.exact_historical_prepare_range(
        observedPrepare->source_rate,observedPrepare->master_rate,
        observedPrepare->source_frames,observedPrepare->layout,
        observedPrepare->ceiling_db_tp,
        observedPrepare->manual_chain_revision,
        observedPrepare->terminal_chain_revision,
        observedPrepare->terminal_parameter_revision);
    QVERIFY(sameShape.has_value());
    QVERIFY(sameShape->compatible_samples>=2);
    QVERIFY(sameShape->minimum_wall_us>0);
    QVERIFY(sameShape->maximum_wall_us>=sameShape->minimum_wall_us);
    const auto differentFrames=core::FrameCount::create(
        observedPrepare->source_frames.value()+1);
    QVERIFY(differentFrames);
    QVERIFY(!renderer.exact_historical_prepare_range(
        observedPrepare->source_rate,observedPrepare->master_rate,
        *differentFrames.value(),observedPrepare->layout,
        observedPrepare->ceiling_db_tp,
        observedPrepare->manual_chain_revision,
        observedPrepare->terminal_chain_revision,
        observedPrepare->terminal_parameter_revision).has_value());
    QVERIFY(!renderer.exact_historical_prepare_range(
        observedPrepare->source_rate,observedPrepare->master_rate,
        observedPrepare->source_frames,observedPrepare->layout,
        observedPrepare->ceiling_db_tp,
        observedPrepare->manual_chain_revision+1,
        observedPrepare->terminal_chain_revision,
        observedPrepare->terminal_parameter_revision).has_value());
    QVERIFY(!renderer.exact_historical_prepare_range(
        observedPrepare->source_rate,observedPrepare->master_rate,
        observedPrepare->source_frames,observedPrepare->layout,
        observedPrepare->ceiling_db_tp,
        observedPrepare->manual_chain_revision,
        observedPrepare->terminal_chain_revision,
        observedPrepare->terminal_parameter_revision+1).has_value());
    QVERIFY(!renderer.exact_historical_prepare_range(
        *deviceRate.value(),observedPrepare->master_rate,
        observedPrepare->source_frames,observedPrepare->layout,
        observedPrepare->ceiling_db_tp,
        observedPrepare->manual_chain_revision,
        observedPrepare->terminal_chain_revision,
        observedPrepare->terminal_parameter_revision).has_value());
    QVERIFY(!renderer.exact_historical_prepare_range(
        observedPrepare->source_rate,observedPrepare->master_rate,
        observedPrepare->source_frames,observedPrepare->layout,-12.0,
        observedPrepare->manual_chain_revision,
        observedPrepare->terminal_chain_revision,
        observedPrepare->terminal_parameter_revision).has_value());
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QVERIFY(product.verified_master_rate_for_monitoring());
    QVERIFY(monitorUi.can_install()); // PLAY/Loop are not Prepare barriers.
    QVERIFY(renderer.last_approved_prepare_timing().has_value());
    QVERIFY(product.cancel());
    // Cancel revokes MASTER and must revoke its profile-eligible timing.
    QVERIFY(!renderer.last_approved_prepare_timing().has_value());
    transport.stop();
    selectedSourceLoop.reset();
    QVERIFY(transport.set_loop_source_range(std::nullopt));
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    // The legacy explicit MASTER-only branch is also an OFFLINE worker;
    // there is no need for STOPPED, zero position or a disabled Loop just
    // to calculate the immutable MASTER. Installation has its own gate.
    const auto masterOnlyPlayingLoop=core::FrameRange::create(
        core::FrameIndex{200},core::FrameIndex{1200});
    QVERIFY(masterOnlyPlayingLoop);
    selectedSourceLoop=*masterOnlyPlayingLoop.value();
    QVERIFY(transport.set_loop_source_range(selectedSourceLoop));
    transport.playOrResume();
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    const auto beforeMasterOnlyPlayingLoop=renderer.render_invocation_count();
    QVERIFY(monitorUi.requestMaster(192000));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"),12000);
    QCOMPARE(renderer.render_invocation_count(),beforeMasterOnlyPlayingLoop+1);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    // MASTER-only offline Prepare MUST NOT reintroduce the old
    // stopped/zero/no-loop installation restriction. Exact MANUAL
    // playback with an unchanged loop may proceed to the independently
    // owner-checked DEVICE handoff; no PCM/sink is installed by canInstall.
    QVERIFY(monitorUi.can_install());
    QCOMPARE(device->loop,selectedSourceLoop);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(renderer.render_invocation_count(),beforeMasterOnlyPlayingLoop+1);
    QVERIFY(monitorUi.cancel());
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->loop,selectedSourceLoop);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    transport.stop();
    selectedSourceLoop.reset();
    QVERIFY(transport.set_loop_source_range(std::nullopt));
    QVERIFY(monitorUi.can_prepare());
    std::size_t deviceProbes=0;
    product.set_preferred_output_rate_probe(
        [&deviceProbes,deviceRate](audio::ChannelLayout)
            ->core::Result<core::SampleRate> {
            ++deviceProbes;
            return core::Result<core::SampleRate>::success(
                *deviceRate.value());
        });
    QVERIFY(monitorUi.requestMaster(192000));
    QVERIFY(monitorUi.monitor_exclusive());
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"),12000);
    const auto wholeMasterRenderCalls=renderer.render_invocation_count();
    // Two earlier explicit full-master jobs (normal READY + active Loop
    // READY) were each counted; this distinct standalone request is third.
    // The one-Prepare invariant is per unchanged owner/parameters/epoch,
    // not a lifetime count across deliberately canceled preparations.
    QCOMPARE(wholeMasterRenderCalls,beforeMasterOnlyPlayingLoop+2);
    QVERIFY(monitorUi.can_dismiss());
    QVERIFY(product.monitoring_transaction_active());
    const auto owned=candidates.staged();
    QVERIFY(owned);
    QCOMPARE(owned->rendered.output_rate,*masterRate.value());
    QVERIFY(!owned->rendered.used_candidate_src);
    QCOMPARE(owned->rendered.terminal.checked.terminal
        .compensated_pcm.view().format().sample_rate(),*masterRate.value());
    QCOMPARE(deviceProbes,std::size_t{0});
    QCOMPARE(sinkInstalls,std::size_t{0});
    QVERIFY(!audioGate.has_session());
    QVERIFY(!product.private_id());
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(device->duration,std::optional{*frames.value()});
    // Private M17-verified MASTER -> separately owned 48-kHz monitor PCM.
    // Mock callback stands in for the Windows fresh-RAM-preflight seam;
    // NO Windows sink is installed in this portable integration fixture.
    const auto masterView=owned->rendered.terminal.checked.terminal
        .compensated_pcm.view();
    std::vector<std::uint64_t> originalBits;
    for(std::size_t ch=0;ch<2;++ch) {
        const auto lane=masterView.channel(ch);
        QVERIFY(lane);
        for(const double sample:*lane.value())
            originalBits.push_back(std::bit_cast<std::uint64_t>(sample));
    }
    std::size_t preflightCalls=0;
    const app::TerminalPrivateProductSession::MonitoringMaterializer
    makeMonitor=[&](audio::AudioBufferView immutable)
        ->core::Result<std::shared_ptr<const audio::MonitoringPcmCandidate>> {
        using Result=core::Result<
            std::shared_ptr<const audio::MonitoringPcmCandidate>>;
        auto built=audio::materialize_monitoring_pcm_candidate(
            immutable,*deviceRate.value(),*block.value(),
            [&](const audio::MonitoringPcmResourceProjection& cost)
                ->core::Status {
                ++preflightCalls;
                if(cost.device_frames.value()!=480
                   ||cost.experimental_fir_scratch_bytes==0)
                    return core::Status::failure(core::Error{
                        core::ErrorCode::InvalidState,
                        "M17_MONITOR_PROJECTION_CHANGED"});
                return core::Status::success();
            });
        if(!built)return Result::failure(*built.error());
        return Result::success(
            std::make_shared<const audio::MonitoringPcmCandidate>(
                std::move(*built.value())));
    };
    const app::TerminalPrivateProductSession::MonitoringMaterializer
        emptyMaterializer{};
    auto missing=product.materialize_private_final_master_monitoring(
        emptyMaterializer);
    QVERIFY(!missing);
    QCOMPARE(missing.error()->code(),core::ErrorCode::InvalidArgument);
    auto monitored=product.materialize_private_final_master_monitoring(
        makeMonitor);
    QVERIFY2(monitored,monitored?"":monitored.error()->message().c_str());
    QCOMPARE(preflightCalls,std::size_t{1});
    // The MASTER->DEVICE materialization reuses the exact immutable M17
    // result; no hidden second PREPARE is legal on monitor entry.
    QCOMPARE(renderer.render_invocation_count(),wholeMasterRenderCalls);
    QCOMPARE((*monitored.value())->immutable_master_rate,*masterRate.value());
    QCOMPARE((*monitored.value())->device_rate,*deviceRate.value());
    QCOMPARE((*monitored.value())->immutable_master_frames,*frames.value());
    QCOMPARE((*monitored.value())->device_pcm.view().frame_count().value(),
        std::int64_t{480});
    std::vector<std::uint64_t> afterBits;
    for(std::size_t ch=0;ch<2;++ch) {
        const auto lane=masterView.channel(ch);
        QVERIFY(lane);
        for(const double sample:*lane.value())
            afterBits.push_back(std::bit_cast<std::uint64_t>(sample));
    }
    QCOMPARE(afterBits,originalBits);
    QCOMPARE(sinkInstalls,std::size_t{0});
    QCOMPARE(deviceProbes,std::size_t{0});
    QVERIFY(!audioGate.has_session());

    // The app now has the same explicit Windows preallocation registration
    // seam as main.cpp. No registration/policy means no device PCM creation;
    // UNKNOWN OS memory cannot be silently interpreted as permission.
    constexpr std::uint64_t kOtherReserve=32ULL*1024*1024;
    const auto explicitAllow=[](
        const audio::MonitoringPcmPreallocationAdvice& observation)
            ->core::Status {
        if(observation.kind!=audio::MonitoringMemoryAdviceKind::UNKNOWN)
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "TEST_EXPECTED_UNKNOWN_MEMORY_OBSERVATION"});
        return core::Status::success();
    };
    auto noRegistration=product
        .materialize_private_final_master_monitoring_with_preflight(
            *block.value(),kOtherReserve,explicitAllow);
    QVERIFY(!noRegistration);
    QCOMPARE(noRegistration.error()->code(),core::ErrorCode::InvalidState);
    std::size_t windowsSeamCalls=0;
    std::size_t explicitPolicyCalls=0;
    // The UI uses the strict Playback SRC limit, not the M17 renderer's
    // independent 128-frame chunks or an invalid 4096-frame request.
    const auto uiMonitoringBlock=core::FrameCount::create(
        audio::PlaybackSampleRateAdapter::kMaximumOutputBlockFrames);
    QVERIFY(uiMonitoringBlock);
    product.set_monitoring_preflight_materializer(
        [&](audio::AudioBufferView view,core::FrameCount requestedBlock,
            std::uint64_t reserve,
            const app::TerminalPrivateProductSession::
                MonitoringPreallocationDecision& decision)
            ->core::Result<
                std::shared_ptr<const audio::MonitoringPcmCandidate>> {
            using Result=core::Result<std::shared_ptr<
                const audio::MonitoringPcmCandidate>>;
            ++windowsSeamCalls;
            // Two exact supported callers, never arbitrary block/reserve
            // combinations: private 128-frame fixture / bounded 1024-frame UI.
            const bool fixturePolicy=requestedBlock==*block.value()
                &&reserve==kOtherReserve;
            const bool uiPolicy=requestedBlock==*uiMonitoringBlock.value()
                &&reserve==0;
            if(!fixturePolicy&&!uiPolicy)
                return Result::failure(core::Error{
                    core::ErrorCode::InvalidArgument,
                    "TEST_MONITOR_WINDOWS_PREFLIGHT_PARAMETERS_CHANGED"});
            auto advice=audio::advise_imminent_monitoring_pcm_allocation(
                view.format().sample_rate(),view.frame_count(),
                *deviceRate.value(),view.format().channel_layout(),
                audio::MonitoringMemoryObservation{0,0,reserve});
            if(!advice)return Result::failure(*advice.error());
            const auto decided=decision(*advice.value());
            if(!decided)return Result::failure(*decided.error());
            return makeMonitor(view);
        });
    const app::TerminalPrivateProductSession::
        MonitoringPreallocationDecision emptyDecision{};
    auto withoutPolicy=product
        .materialize_private_final_master_monitoring_with_preflight(
            *block.value(),kOtherReserve,emptyDecision);
    QVERIFY(!withoutPolicy);
    QCOMPARE(withoutPolicy.error()->code(),core::ErrorCode::InvalidArgument);
    QCOMPARE(windowsSeamCalls,std::size_t{0});
    auto denied=product
        .materialize_private_final_master_monitoring_with_preflight(
            *block.value(),kOtherReserve,
            [&](const audio::MonitoringPcmPreallocationAdvice& observation)
                ->core::Status {
                ++explicitPolicyCalls;
                if(observation.kind!=audio::MonitoringMemoryAdviceKind::UNKNOWN)
                    return core::Status::failure(core::Error{
                        core::ErrorCode::InvalidState,
                        "TEST_EXPECTED_UNKNOWN_MEMORY_OBSERVATION"});
                return core::Status::failure(core::Error{
                    core::ErrorCode::UnsupportedOperation,
                    "TEST_USER_EXPLICIT_MONITOR_MEMORY_VETO"});
            });
    QVERIFY(!denied);
    QCOMPARE(denied.error()->code(),core::ErrorCode::UnsupportedOperation);
    QCOMPARE(explicitPolicyCalls,std::size_t{1});
    QCOMPARE(windowsSeamCalls,std::size_t{1});
    const auto preflightApproved=product
        .materialize_private_final_master_monitoring_with_preflight(
            *block.value(),kOtherReserve,
            [&](const audio::MonitoringPcmPreallocationAdvice& advice)
                ->core::Status {
                ++explicitPolicyCalls;
                return explicitAllow(advice);
            });
    QVERIFY(preflightApproved);
    QCOMPARE(explicitPolicyCalls,std::size_t{2});
    QCOMPARE(windowsSeamCalls,std::size_t{2});
    QCOMPARE((*preflightApproved.value())->immutable_master_rate,
             *masterRate.value());
    QCOMPARE((*preflightApproved.value())->device_rate,*deviceRate.value());
    // UNKNOWN Windows memory cannot be overridden even when the user
    // explicitly permits CAUTION. Denial MUST NOT install a device sink.
    QVERIFY(!monitorUi.installMonitor(true));
    QCOMPARE(monitorUi.error_text(),
             QStringLiteral("MONITOR_MEMORY_NOT_APPROVED_OR_UNAVAILABLE"));
    QVERIFY(monitorUi.monitor_exclusive());
    QCOMPARE(sinkInstalls,std::size_t{0});
    QCOMPARE(deviceProbes,std::size_t{0});
    QVERIFY(!audioGate.has_session());
    QCOMPARE(device->audibleRealization.realizationId,manualId);

    // Private STOPPED-only M17 -> MONITOR device handoff and exact MANUAL
    // return. The real app registers the Windows guarded owner-thread ports;
    // here each operation is exercised against the same fake device.
    const auto privateMonitor=*preflightApproved.value();
    QVERIFY(!product.install_private_monitor_stopped(privateMonitor));
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    std::shared_ptr<const audio::MonitoringPcmCandidate> deviceMonitor;
    std::size_t monitorInstalls=0;
    std::size_t monitorRetires=0;
    std::size_t manualRestores=0;
    int injectedManualRestoreFailure=0;
    int injectedManualRestoreException=0;
    int injectedManualRestoreUnknownException=0;
    int injectedManualRestoreWrongOwner=0;
    std::optional<core::FrameRange> monitorMasterLoop;
    int injectedMonitorInstallFailure=0;
    int injectedMonitorRetireFailure=0;
    bool sourceEpochCurrent=true;
    bool revokeOfflineEpochOnInstall=false;
    product.set_private_monitor_transport(
        [&](auto candidate)->core::Status {
            ++monitorInstalls;
            deviceMonitor=injectedMonitorInstallFailure==2
                ?*monitored.value():candidate;
            device->state=core::PlaybackState::STOPPED;
            device->position=core::FrameIndex{0};
            device->duration=candidate->device_pcm.view().frame_count();
            device->loop.reset();
            device->audibleRealization={};
            if(revokeOfflineEpochOnInstall) {
                // Source epoch changes exactly after the genuine owner
                // was installed, but before the session post-check.
                sourceEpochCurrent=false;
                revokeOfflineEpochOnInstall=false;
            }
            if(injectedMonitorInstallFailure)
                return core::Status::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "TEST_WINDOWS_INSTALL_FAILED_AFTER_DEVICE_MUTATION"});
            return core::Status::success();
        },
        [&](auto candidate)->core::Result<bool> {
            return core::Result<bool>::success(
                candidate&&candidate==deviceMonitor
                &&device->state!=core::PlaybackState::NO_SOURCE
                &&device->duration==std::optional{
                    candidate->device_pcm.view().frame_count()}
                &&device->audibleRealization.phase
                    ==core::AudibleHandoffPhase::UNAVAILABLE
                &&!device->audibleRealization.realizationId);
        },
        [&](auto candidate)->core::Status {
            if(candidate!=deviceMonitor
               ||device->state==core::PlaybackState::NO_SOURCE)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "TEST_EXACT_MONITOR_OWNER_REQUIRED"});
            if(injectedMonitorRetireFailure>0) {
                --injectedMonitorRetireFailure;
                return core::Status::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "TEST_EXACT_MONITOR_RETIRE_FAILURE"});
            }
            ++monitorRetires;
            deviceMonitor.reset();
            return device->clear();
        },
        [&](auto manual,core::RealizationId id,core::FrameIndex cue)
            ->core::Status {
            ++manualRestores;
            if(injectedManualRestoreException>0) {
                --injectedManualRestoreException;
                throw std::bad_alloc{};
            }
            if(injectedManualRestoreUnknownException>0) {
                --injectedManualRestoreUnknownException;
                throw 73; // Non-std Windows/Qt boundary exception, not success.
            }
            if(injectedManualRestoreFailure>0) {
                --injectedManualRestoreFailure;
                return core::Status::failure(core::Error{
                    core::ErrorCode::IoFailure,
                    "TEST_WINDOWS_SOURCE_INSTALL_DENIED_AFTER_RETIRE"});
            }
            if(injectedManualRestoreWrongOwner>0) {
                --injectedManualRestoreWrongOwner;
                // Simulated buggy Windows completion reports success but
                // the sink contains a different owner. M17 must fail the
                // snapshot witness, keep MASTER, and never clear this sink.
                device->state=core::PlaybackState::STOPPED;
                device->duration=*frames.value();
                device->position=cue;
                device->audibleRealization={
                    core::AudibleHandoffPhase::NEW,
                    core::RealizationId{0xDEAD}};
                return core::Status::success();
            }
            return selector.restore_manual_processed_after_terminal(
                std::move(manual),id,cue);
        });
    // The first conversion was superseded by the latest preflight
    // candidate. Equal rates/PCM geometry cannot reuse that older pointer.
    QVERIFY(!product.install_private_monitor_stopped(*monitored.value()));
    QCOMPARE(monitorInstalls,std::size_t{0});
    device->position=core::FrameIndex{1};
    QVERIFY(!product.install_private_monitor_stopped(privateMonitor));
    device->position=core::FrameIndex{0};
    QCOMPARE(monitorInstalls,std::size_t{0});
    QVERIFY(product.install_private_monitor_stopped(privateMonitor));
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED"));
    QCOMPARE(monitorInstalls,std::size_t{1});
    QCOMPARE(deviceMonitor,privateMonitor);
    QVERIFY(monitorUi.monitor_installed());
    QVERIFY(!monitorUi.requestMaster(96000));
    QCOMPARE(device->duration,std::optional{
        privateMonitor->device_pcm.view().frame_count()});
    QVERIFY(!device->audibleRealization.realizationId);
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QCOMPARE(sinkInstalls,std::size_t{0});
    QVERIFY(!product.install_private_monitor_stopped(privateMonitor));

    // Real MASTER-frame command contract, simulated with the exact same
    // monitoring coordinate map and fake playback engine (NO Windows DAC).
    product.set_private_monitor_controls(
        [&](auto owner,auto command)->core::Status {
            if(owner!=deviceMonitor)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "TEST_MONITOR_COMMAND_WRONG_OWNER"});
            auto map=audio::MonitoringFrameMap::create(*owner);
            if(!map)return core::Status::failure(*map.error());
            using Action=platform::windows::MonitoringTransportAction;
            switch(command.action) {
            case Action::PLAY:return device->play();
            case Action::PAUSE:return device->pause();
            case Action::STOP:return device->stop();
            case Action::SEEK_MASTER: {
                auto frame=map.value()->master_to_device(command.master_frame);
                if(!frame)return core::Status::failure(*frame.error());
                return device->seek(*frame.value());
            }
            case Action::SET_MASTER_LOOP: {
                std::optional<core::FrameRange> mapped;
                if(command.master_loop) {
                    auto region=map.value()->master_loop_to_device(
                        *command.master_loop);
                    if(!region)return core::Status::failure(*region.error());
                    mapped=*region.value();
                }
                auto status=device->set_loop(mapped);
                if(status)monitorMasterLoop=command.master_loop;
                return status;
            }
            }
            return core::Status::failure(core::Error{
                core::ErrorCode::InvalidArgument,
                "TEST_MONITOR_UNKNOWN_COMMAND"});
        },
        [&](auto owner)
            ->core::Result<audio::MonitoringMasterDisplaySnapshot> {
            using Display=audio::MonitoringMasterDisplaySnapshot;
            if(owner!=deviceMonitor)
                return core::Result<Display>::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "TEST_MONITOR_DISPLAY_WRONG_OWNER"});
            auto map=audio::MonitoringFrameMap::create(*owner);
            if(!map)return core::Result<Display>::failure(*map.error());
            auto playback=device->snapshot();
            if(!playback)return core::Result<Display>::failure(*playback.error());
            return map.value()->project_display_snapshot(
                *playback.value(),monitorMasterLoop);
        });
    using Action=platform::windows::MonitoringTransportAction;
    using Request=platform::windows::MonitoringTransportRequest;
    auto originalDisplay=product.private_monitor_master_snapshot();
    QVERIFY(originalDisplay);
    QCOMPARE(originalDisplay.value()->master_rate,*masterRate.value());
    QCOMPARE(originalDisplay.value()->device_rate,*deviceRate.value());
    QCOMPARE(originalDisplay.value()->master_display_position.value(),
        std::int64_t{0});
    // Pause from STOPPED fails; the Windows worker will additionally guard
    // the state before dispatching to its actual sink.
    QVERIFY(!monitorUi.pause());
    const auto rejectedPause=monitorUi.error_text();
    QVERIFY(!rejectedPause.isEmpty());
    monitorUi.refresh();
    // A polling tick cannot erase a rejected user action.
    QCOMPARE(monitorUi.error_text(),rejectedPause);
    QVERIFY(monitorUi.seekMaster(800));
    QCOMPARE(monitorUi.master_position_frames(),qint64{800});
    QCOMPARE(monitorUi.master_rate_hz(),192000);
    QCOMPARE(monitorUi.device_rate_hz(),48000);
    QVERIFY(monitorUi.error_text().isEmpty());
    QCOMPARE(device->position.value(),std::int64_t{200});
    auto region=core::FrameRange::create(
        core::FrameIndex{256},core::FrameIndex{1536});
    QVERIFY(region);
    QVERIFY(monitorUi.setMasterLoop(256,1536,true));
    QVERIFY(device->loop);
    QCOMPARE(device->loop->begin().value(),std::int64_t{64});
    QCOMPARE(device->loop->end().value(),std::int64_t{384});
    // Pure SOURCE-loop projection. Same-rate is exact; cross-rate uses
    // ceil half-open boundaries. No source region or owner is mutated.
    auto coordinateMap=audio::MonitoringFrameMap::create(*privateMonitor);
    QVERIFY(coordinateMap);
    auto source192=coordinateMap.value()->master_loop_to_source(
        *region.value(),*masterRate.value(),*frames.value());
    QVERIFY(source192);
    QCOMPARE(source192.value()->begin().value(),std::int64_t{256});
    QCOMPARE(source192.value()->end().value(),std::int64_t{1536});
    auto source441Rate=core::SampleRate::create(44100);
    auto source441Frames=core::FrameCount::create(441);
    QVERIFY(source441Rate&&source441Frames);
    auto source441=coordinateMap.value()->master_loop_to_source(
        *region.value(),*source441Rate.value(),*source441Frames.value());
    QVERIFY(source441);
    QCOMPARE(source441.value()->begin().value(),std::int64_t{59});
    QCOMPARE(source441.value()->end().value(),std::int64_t{353});
    // Inverse Source->Master/Device cue + Loop mapping for a future
    // seamless playing/paused handoff; no new M17 render or PCM mutation.
    const auto sourceCue=coordinateMap.value()->source_to_master(
        core::FrameIndex{59},*source441Rate.value(),
        *source441Frames.value());
    QVERIFY(sourceCue);
    QCOMPARE(sourceCue.value()->value(),std::int64_t{257});
    const auto targetDevice=coordinateMap.value()->master_to_device(
        *sourceCue.value());
    QVERIFY(targetDevice);
    QCOMPARE(targetDevice.value()->value(),std::int64_t{65});
    const auto sourceLoop=core::FrameRange::create(
        core::FrameIndex{59},core::FrameIndex{353});
    QVERIFY(sourceLoop);
    const auto mappedMaster=coordinateMap.value()->source_loop_to_master(
        *sourceLoop.value(),*source441Rate.value(),
        *source441Frames.value());
    QVERIFY(mappedMaster);
    QCOMPARE(mappedMaster.value()->begin().value(),std::int64_t{257});
    QCOMPARE(mappedMaster.value()->end().value(),std::int64_t{1537});
    const auto mappedDevice=coordinateMap.value()->master_loop_to_device(
        *mappedMaster.value());
    QVERIFY(mappedDevice);
    QCOMPARE(mappedDevice.value()->begin().value(),std::int64_t{65});
    QCOMPARE(mappedDevice.value()->end().value(),std::int64_t{385});
    const auto sourceEnd=coordinateMap.value()->source_to_master(
        core::FrameIndex{441},*source441Rate.value(),
        *source441Frames.value());
    QVERIFY(sourceEnd);
    QCOMPARE(sourceEnd.value()->value(),std::int64_t{1920});
    QVERIFY(!coordinateMap.value()->source_to_master(
        core::FrameIndex{442},*source441Rate.value(),
        *source441Frames.value()));
    auto oneMasterFrame=core::FrameRange::create(
        core::FrameIndex{1},core::FrameIndex{2});
    QVERIFY(oneMasterFrame);
    QVERIFY(!coordinateMap.value()->master_loop_to_source(
        *oneMasterFrame.value(),*source441Rate.value(),
        *source441Frames.value()));
    auto wrongSourceDuration=core::FrameCount::create(900);
    QVERIFY(wrongSourceDuration);
    QVERIFY(!coordinateMap.value()->master_loop_to_source(
        *region.value(),*source441Rate.value(),
        *wrongSourceDuration.value()));
    QCOMPARE(device->loop->begin().value(),std::int64_t{64});
    QCOMPARE(device->loop->end().value(),std::int64_t{384});
    QVERIFY(monitorUi.play());
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QVERIFY(monitorUi.pause());
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(monitorUi.transport_state(),QStringLiteral("PAUSED"));
    auto paused=product.private_monitor_master_snapshot();
    QVERIFY(paused);
    QCOMPARE(paused.value()->master_display_position.value(),
        std::int64_t{800});
    QCOMPARE(paused.value()->requested_master_loop,*region.value());
    QVERIFY(monitorUi.play());
    QVERIFY(monitorUi.stop());
    QCOMPARE(device->position.value(),std::int64_t{0});
    QVERIFY(monitorUi.setMasterLoop(0,0,false));
    QVERIFY(monitorUi.seekMaster(800));
    QCOMPARE(device->position.value(),std::int64_t{200});
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED"));
    // A differently owned, same-rate PCM may never consume our commands.
    deviceMonitor=*monitored.value();
    QVERIFY(!product.private_monitor_master_snapshot());
    QVERIFY(!monitorUi.play());
    QVERIFY(!monitorUi.cancel()); // cannot clear or adopt alien DEVICE owner
    monitorUi.refresh();
    QCOMPARE(monitorUi.transport_state(),QStringLiteral("UNAVAILABLE"));
    QCOMPARE(monitorUi.device_rate_hz(),0);
    QCOMPARE(monitorUi.master_position_frames(),qint64{0});
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    QCOMPARE(device->position.value(),std::int64_t{200});
    deviceMonitor=privateMonitor;
    // A second anonymous monitor at the same DEVICE rate supersedes ours.
    // The product must neither clear it nor restore a false MANUAL owner.
    deviceMonitor=*monitored.value();
    auto alienReturn=product.return_private_monitor_to_manual();
    QVERIFY(!alienReturn);
    QCOMPARE(monitorRetires,std::size_t{0});
    QCOMPARE(manualRestores,std::size_t{0});
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED"));
    deviceMonitor=privateMonitor;
    // FQ98: hardware SOURCE preflight fails while M17 monitor remains
    // exactly owned. Return must NOT retire the current device, must NOT
    // install a partial MANUAL, and must retain the master and cue for retry.
    std::size_t sourceRestoreProbes=0;
    bool sourceRestoreSupported=false;
    bool sourceRestoreThrows=false;
    bool reenterReturnOnPreflight=false;
    bool nestedReturnCorrectlyBlocked=false;
    bool invalidateSelectionOnPreflight=false;
    bool occupyDeviceDuringRecoveryPreflight=false;
    product.set_private_monitor_manual_restore_preflight(
        [&](auto original)->core::Status {
            ++sourceRestoreProbes;
            if(reenterReturnOnPreflight) {
                reenterReturnOnPreflight=false;
                const auto nested=product.return_private_monitor_to_manual();
                nestedReturnCorrectlyBlocked=!nested
                    &&nested.error()->code()==core::ErrorCode::InvalidState;
            }
            if(invalidateSelectionOnPreflight) {
                invalidateSelectionOnPreflight=false;
                productAudibleSelectionAccepted=false;
            }
            if(sourceRestoreThrows)throw std::bad_alloc{};
            if(occupyDeviceDuringRecoveryPreflight) {
                occupyDeviceDuringRecoveryPreflight=false;
                device->state=core::PlaybackState::STOPPED;
                device->duration=*frames.value();
                device->audibleRealization={
                    core::AudibleHandoffPhase::NEW,
                    core::RealizationId{0xBAAD}};
            }
            if(original!=selector.processed_realization_snapshot())
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "TEST_PREFLIGHT_MANUAL_EPOCH_CHANGED"});
            if(!sourceRestoreSupported)
                return core::Status::failure(core::Error{
                    core::ErrorCode::UnsupportedOperation,
                    "TEST_SOURCE_DEVICE_FORMAT_NOT_SUPPORTED"});
            return core::Status::success();
        });
    reenterReturnOnPreflight=true;
    const auto retryable=product.return_private_monitor_to_manual();
    QVERIFY(!retryable);
    QCOMPARE(retryable.error()->code(),core::ErrorCode::UnsupportedOperation);
    QVERIFY(nestedReturnCorrectlyBlocked);
    QCOMPARE(sourceRestoreProbes,std::size_t{1});
    QCOMPARE(monitorRetires,std::size_t{0});
    QCOMPARE(manualRestores,std::size_t{0});
    QCOMPARE(deviceMonitor,privateMonitor);
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED"));
    QCOMPARE(device->position,core::FrameIndex{200});
    sourceRestoreThrows=true;
    const auto exceptionRefused=product.return_private_monitor_to_manual();
    QVERIFY(!exceptionRefused);
    QCOMPARE(exceptionRefused.error()->code(),core::ErrorCode::IoFailure);
    QCOMPARE(sourceRestoreProbes,std::size_t{2});
    QCOMPARE(monitorRetires,std::size_t{0});
    QCOMPARE(manualRestores,std::size_t{0});
    QCOMPARE(deviceMonitor,privateMonitor);
    sourceRestoreThrows=false;
    sourceRestoreSupported=true;
    injectedManualRestoreFailure=1;
    // FC100 / blocker #73: the device passed *preflight*, but the real
    // Windows SOURCE install refused only AFTER exact monitor retirement.
    // Neither a second lost Prepare nor a false verified audible sink
    // may follow. A bounded retry must re-probe the physical endpoint.
    const auto retiredButSourceRefused=product.return_private_monitor_to_manual();
    QVERIFY(!retiredButSourceRefused);
    QCOMPARE(retiredButSourceRefused.error()->code(),core::ErrorCode::IoFailure);
    QCOMPARE(sourceRestoreProbes,std::size_t{3});
    QCOMPARE(monitorRetires,std::size_t{1});
    QCOMPARE(manualRestores,std::size_t{1});
    QVERIFY(!deviceMonitor);
    QCOMPARE(device->state,core::PlaybackState::NO_SOURCE);
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED"));
    QVERIFY(product.monitoring_transaction_active());
    QVERIFY(candidates.staged());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QVERIFY(!product.verified_master_rate_for_monitoring());
    QVERIFY(!monitorUi.monitor_installed());
    QVERIFY(monitorUi.monitor_exclusive());
    QCOMPARE(monitorUi.return_target_text(),
        QStringLiteral("Retry Return to Source"));
    QVERIFY(monitorUi.status_text().contains(
        QStringLiteral("MASTER PCM RETAINED")));
    // A Qt SOURCE-loop changed signal is NOT a user request to retry
    // a just-failed physical Windows sink. No hidden device mutation.
    product.cancel_pending_on_region_change();
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED"));
    QCOMPARE(sourceRestoreProbes,std::size_t{3});
    QCOMPARE(manualRestores,std::size_t{1});
    QVERIFY(candidates.staged());
    const auto unexpectedCancel=product.cancel();
    QVERIFY(!unexpectedCancel);
    QCOMPARE(unexpectedCancel.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED"));
    QCOMPARE(sourceRestoreProbes,std::size_t{3});
    QCOMPARE(manualRestores,std::size_t{1});
    QVERIFY(candidates.staged());
    // If Windows/another application installs any other sink between the
    // failed Return and our explicit retry, don't clear it or borrow it.
    device->state=core::PlaybackState::STOPPED;
    device->duration=*frames.value();
    device->audibleRealization={
        core::AudibleHandoffPhase::NEW,core::RealizationId{0xABCD}};
    const auto foreignRefused=product.return_private_monitor_to_manual();
    QVERIFY(!foreignRefused);
    QCOMPARE(foreignRefused.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(sourceRestoreProbes,std::size_t{3});
    QCOMPARE(manualRestores,std::size_t{1});
    QCOMPARE(monitorRetires,std::size_t{1});
    QCOMPARE(device->audibleRealization.realizationId,
        std::optional{core::RealizationId{0xABCD}});
    // Only the fake physical system (not M17) removes that foreign owner.
    QVERIFY(device->clear());
    sourceRestoreSupported=false;
    const auto refusedRetry=product.return_private_monitor_to_manual();
    QVERIFY(!refusedRetry);
    QCOMPARE(refusedRetry.error()->code(),
        core::ErrorCode::UnsupportedOperation);
    QCOMPARE(sourceRestoreProbes,std::size_t{4});
    QCOMPARE(manualRestores,std::size_t{1});
    QCOMPARE(monitorRetires,std::size_t{1});
    QVERIFY(candidates.staged());
    sourceRestoreSupported=true;
    injectedManualRestoreException=1;
    nestedReturnCorrectlyBlocked=false;
    reenterReturnOnPreflight=true;
    const auto throwingRetry=product.return_private_monitor_to_manual();
    QVERIFY(!throwingRetry);
    QCOMPARE(throwingRetry.error()->code(),core::ErrorCode::IoFailure);
    QVERIFY(nestedReturnCorrectlyBlocked);
    QCOMPARE(sourceRestoreProbes,std::size_t{5});
    QCOMPARE(manualRestores,std::size_t{2});
    QCOMPARE(monitorRetires,std::size_t{1});
    QVERIFY(candidates.staged());
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED"));
    // #73: a nested Qt event changed SOURCE during the Windows retry probe.
    // No second manual install may consume the now-stale captured owner.
    invalidateSelectionOnPreflight=true;
    const auto changedDuringRetry=product.return_private_monitor_to_manual();
    QVERIFY(!changedDuringRetry);
    QCOMPARE(changedDuringRetry.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(sourceRestoreProbes,std::size_t{6});
    QCOMPARE(manualRestores,std::size_t{2});
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED"));
    QVERIFY(candidates.staged());
    productAudibleSelectionAccepted=true;
    // A foreign DEVICE sink installed *during* preflight is distinct from
    // initial NO_SOURCE. The explicit retry must not silently overwrite it.
    occupyDeviceDuringRecoveryPreflight=true;
    const auto occupiedDuringRetry=product.return_private_monitor_to_manual();
    QVERIFY(!occupiedDuringRetry);
    QCOMPARE(occupiedDuringRetry.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(sourceRestoreProbes,std::size_t{7});
    QCOMPARE(manualRestores,std::size_t{2});
    QCOMPARE(device->audibleRealization.realizationId,
        std::optional{core::RealizationId{0xBAAD}});
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED"));
    QVERIFY(device->clear()); // Simulate separate OS cleanup of the alien.
    // Catch even a non-standard exception AFTER retiring the monitored
    // sink: retain immutable MASTER for a later explicit retry.
    injectedManualRestoreUnknownException=1;
    const auto unknownExceptionRetry=product.return_private_monitor_to_manual();
    QVERIFY(!unknownExceptionRetry);
    QCOMPARE(unknownExceptionRetry.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(sourceRestoreProbes,std::size_t{8});
    QCOMPARE(manualRestores,std::size_t{3});
    QVERIFY(candidates.staged());
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED"));
    injectedManualRestoreWrongOwner=1;
    const auto wrongOwner=product.return_private_monitor_to_manual();
    QVERIFY(!wrongOwner);
    QCOMPARE(wrongOwner.error()->code(),core::ErrorCode::InvalidState);
    QVERIFY(QString::fromStdString(wrongOwner.error()->message())
        .contains(QStringLiteral("MONITOR_RETURN_REALIZATION_MISMATCH")));
    QCOMPARE(sourceRestoreProbes,std::size_t{9});
    QCOMPARE(monitorRetires,std::size_t{1});
    QCOMPARE(manualRestores,std::size_t{4});
    QCOMPARE(device->audibleRealization.realizationId,
        std::optional{core::RealizationId{0xDEAD}});
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_SOURCE_RECOVERY_REQUIRED"));
    QVERIFY(candidates.staged());
    // Never auto-clear the foreign sink. The test environment alone
    // emulates its removal before the next explicitly requested Return.
    QVERIFY(device->clear());
    // The ACTUAL UI button/menu calls cancel(), not returnManual()!
    // Verify the user-visible path performs a genuine explicit retry.
    QVERIFY(monitorUi.cancel());
    QCOMPARE(sourceRestoreProbes,std::size_t{10});
    QCOMPARE(monitorRetires,std::size_t{1});
    QCOMPARE(manualRestores,std::size_t{5});
    QCOMPARE(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"));
    QVERIFY(!deviceMonitor);
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    QCOMPARE(device->duration,std::optional{*frames.value()});
    QCOMPARE(device->position,core::FrameIndex{800});
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QCOMPARE(sinkInstalls,std::size_t{0});
    std::vector<std::uint64_t> bitsAfterRoundTrip;
    for(std::size_t ch=0;ch<2;++ch) {
        const auto lane=masterView.channel(ch);
        QVERIFY(lane);
        for(const double sample:*lane.value())
            bitsAfterRoundTrip.push_back(std::bit_cast<std::uint64_t>(sample));
    }
    QCOMPARE(bitsAfterRoundTrip,originalBits);

    // Deliberately fail closed if MONITOR's loop cannot be reconciled with
    // the currently selected SOURCE loop; clearing it permits safe return.
    QVERIFY(transport.seek_source_frame(core::FrameIndex{0}));
    QVERIFY(product.install_private_monitor_stopped(privateMonitor));
    QCOMPARE(monitorInstalls,std::size_t{2});
    QVERIFY(product.control_private_monitor(
        Request{Action::SET_MASTER_LOOP,core::FrameIndex{0},
                *region.value()}));
    auto loopRefused=product.return_private_monitor_to_manual();
    QVERIFY(!loopRefused);
    QCOMPARE(deviceMonitor,privateMonitor);
    QCOMPARE(monitorRetires,std::size_t{1});
    QCOMPARE(manualRestores,std::size_t{5});
    QVERIFY(product.control_private_monitor(
        Request{Action::SET_MASTER_LOOP,core::FrameIndex{0},std::nullopt}));
    QVERIFY(product.control_private_monitor(
        Request{Action::SEEK_MASTER,core::FrameIndex{800}}));
    QVERIFY(product.control_private_monitor(Request{Action::PLAY}));
    QVERIFY(product.control_private_monitor(Request{Action::PAUSE}));
    const auto pausedSourcePosition=device->position;
    QCOMPARE(pausedSourcePosition.value(),std::int64_t{200});
    QVERIFY(product.return_private_monitor_to_manual());
    QCOMPARE(device->state,core::PlaybackState::PAUSED);
    QCOMPARE(device->position,core::FrameIndex{800});
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(monitorRetires,std::size_t{2});
    QCOMPARE(manualRestores,std::size_t{6});
    QCOMPARE(product.status(),QStringLiteral("MASTER_READY_MONITOR_PENDING"));
    // A PLAYING monitor must yield a PLAYING original MANUAL (never a
    // freshly issued M17 identity). It may advance after resume, so
    // equality with the pre-retire cue is not a DAC-clock certificate.
    QVERIFY(device->stop());
    QVERIFY(product.install_private_monitor_stopped(privateMonitor));
    QCOMPARE(monitorInstalls,std::size_t{3});
    QVERIFY(product.control_private_monitor(
        Request{Action::SEEK_MASTER,core::FrameIndex{1024}}));
    QVERIFY(product.control_private_monitor(Request{Action::PLAY}));
    QVERIFY(product.return_private_monitor_to_manual());
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(device->position,core::FrameIndex{1024});
    QCOMPARE(monitorRetires,std::size_t{3});
    QCOMPARE(manualRestores,std::size_t{7});
    QCOMPARE(product.status(),QStringLiteral("MASTER_READY_MONITOR_PENDING"));
    QCOMPARE(sinkInstalls,std::size_t{0});
    QVERIFY(!product.private_id());
    // #73: natural end-of-programme is legal for the anonymous monitor.
    // The real MANUAL restore canonicalizes SOURCE EOF to frame zero.
    // A strict postcondition must compare against ZERO, not the old EOF.
    QVERIFY(device->stop());
    QVERIFY(product.install_private_monitor_stopped(privateMonitor));
    // Windows/Qt preflight can allow nested SOURCE revision changes. Stop
    // BEFORE monitor retirement, never after losing the device.
    invalidateSelectionOnPreflight=true;
    const auto stalePreflightReturn=product.return_private_monitor_to_manual();
    QVERIFY(!stalePreflightReturn);
    QCOMPARE(stalePreflightReturn.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(monitorRetires,std::size_t{3});
    QCOMPARE(manualRestores,std::size_t{7});
    QCOMPARE(deviceMonitor,privateMonitor);
    QCOMPARE(product.status(),
             QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED"));
    productAudibleSelectionAccepted=true;
    QVERIFY(device->duration.has_value());
    device->position=core::FrameIndex{device->duration->value()};
    const auto eofMonitor=product.private_monitor_master_snapshot();
    QVERIFY(eofMonitor);
    QCOMPARE(eofMonitor.value()->master_display_position,
             core::FrameIndex{
                 privateMonitor->immutable_master_frames.value()});
    QVERIFY(product.return_private_monitor_to_manual());
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    QCOMPARE(device->position,core::FrameIndex{0});
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(product.status(),QStringLiteral("MASTER_READY_MONITOR_PENDING"));
    QCOMPARE(monitorRetires,std::size_t{4});
    QCOMPARE(manualRestores,std::size_t{8});
    QVERIFY(candidates.staged());
    QCOMPARE(sinkInstalls,std::size_t{0});
    // Leave the original stopped MANUAL state for the existing
    // cancellation-inside-conversion oracle following this test.
    QVERIFY(device->stop());

    // A cancellation INSIDE conversion revokes the master transaction.
    // A finished device PCM from that obsolete epoch must be discarded.
    auto revoked=product.materialize_private_final_master_monitoring(
        [&](audio::AudioBufferView immutable)
            ->core::Result<std::shared_ptr<
                const audio::MonitoringPcmCandidate>> {
            const auto retired=product.cancel();
            if(!retired)return core::Result<std::shared_ptr<
                const audio::MonitoringPcmCandidate>>::failure(
                    *retired.error());
            return makeMonitor(immutable);
        });
    QVERIFY(!revoked);
    QCOMPARE(revoked.error()->code(),core::ErrorCode::InvalidState);
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());
    QVERIFY(!product.private_id());
    QCOMPARE(sinkInstalls,std::size_t{0});
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    auto unavailable=product.materialize_private_final_master_monitoring(
        makeMonitor);
    QVERIFY(!unavailable);
    QCOMPARE(unavailable.error()->code(),core::ErrorCode::InvalidState);
    QVERIFY(!product.install_ready_private());
    QVERIFY(!product.install_measured_private());
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());
    QCOMPARE(sinkInstalls,std::size_t{0});
    QCOMPARE(device->audibleRealization.realizationId,manualId);

    // New verified MASTER epoch after cancellation: a MASTER loop cannot
    // silently mutate SOURCE before the exact monitor sink is retired.
    QVERIFY(product.request_private_for_final_master(*masterRate.value()));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"),12000);
    auto freshMonitor=product.materialize_private_final_master_monitoring(
        makeMonitor);
    QVERIFY(freshMonitor);
    QVERIFY(product.install_private_monitor_stopped(*freshMonitor.value()));
    QVERIFY(product.monitoring_transaction_active());
    QVERIFY(product.control_private_monitor(
        Request{Action::SET_MASTER_LOOP,core::FrameIndex{0},
                *region.value()}));
    QVERIFY(product.control_private_monitor(
        Request{Action::SEEK_MASTER,core::FrameIndex{800}}));
    std::size_t sourceLoopCommits=0;
    QVERIFY(!product.return_private_monitor_to_manual());
    QCOMPARE(sourceLoopCommits,std::size_t{0});
    QCOMPARE(deviceMonitor,*freshMonitor.value());
    // Explicit callback sees only STOPPED original MANUAL after retirement.
    product.set_private_monitor_source_loop_reconciliation(
        [&](core::FrameRange projected)->core::Status {
            ++sourceLoopCommits;
            if(deviceMonitor
               ||device->state!=core::PlaybackState::STOPPED
               ||device->audibleRealization.realizationId!=manualId)
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "TEST_SOURCE_LOOP_BEFORE_EXACT_RETIREMENT"});
            selectedSourceLoop=projected;
            // Synchronous Qt AuditionRegion changed listener while
            // monitorTransitioning_ is still active (never recurse cancel).
            product.cancel_pending_on_region_change();
            if(!product.monitoring_transaction_active()
               ||product.status()!=QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED"))
                return core::Status::failure(core::Error{
                    core::ErrorCode::InvalidState,
                    "TEST_REENTRANT_REGION_EVENT_CANCELLED_MONITOR"});
            return transport.set_loop_source_range(projected);
        });
    QVERIFY(monitorUi.returnManual());
    QCOMPARE(sourceLoopCommits,std::size_t{1});
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    QCOMPARE(device->position,core::FrameIndex{800});
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(device->loop,std::optional{*region.value()});
    QCOMPARE(selectedSourceLoop,std::optional{*region.value()});
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!product.monitoring_transaction_active());
    QVERIFY(!monitorUi.can_dismiss());
    QVERIFY(!candidates.staged());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QCOMPARE(sinkInstalls,std::size_t{0});

    // Fault injection: Windows install callback reports failure AFTER
    // replacing MANUAL with the exact candidate. Recover only that owner,
    // verify an empty device, then recover the original stopped MANUAL.
    selectedSourceLoop.reset();
    QVERIFY(transport.set_loop_source_range(std::nullopt));
    QVERIFY(device->stop());
    QVERIFY(transport.seek_source_frame(core::FrameIndex{0}));
    QVERIFY(product.request_private_for_final_master(*masterRate.value()));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"),12000);
    auto recoveryMonitor=product.materialize_private_final_master_monitoring(
        makeMonitor);
    QVERIFY(recoveryMonitor);
    const auto retireCountBefore=monitorRetires;
    const auto restoresBefore=manualRestores;
    injectedMonitorInstallFailure=1;
    QVERIFY(!product.install_private_monitor_stopped(*recoveryMonitor.value()));
    QCOMPARE(monitorRetires,retireCountBefore+std::size_t{1});
    QCOMPARE(manualRestores,restoresBefore+std::size_t{1});
    QVERIFY(!deviceMonitor);
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    QCOMPARE(device->position,core::FrameIndex{0});
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QCOMPARE(device->duration,std::optional{*frames.value()});
    QCOMPARE(product.status(),QStringLiteral("MASTER_READY_MONITOR_PENDING"));
    QCOMPARE(sinkInstalls,std::size_t{0});
    // An anonymous same-rate successor is NOT the failed candidate:
    // never retire it and never pretend to restore the old MANUAL sink.
    injectedMonitorInstallFailure=2;
    QVERIFY(!product.install_private_monitor_stopped(*recoveryMonitor.value()));
    QCOMPARE(deviceMonitor,*monitored.value());
    QCOMPARE(monitorRetires,retireCountBefore+std::size_t{1});
    QCOMPARE(manualRestores,restoresBefore+std::size_t{1});
    QCOMPARE(device->state,core::PlaybackState::STOPPED);
    QVERIFY(!device->audibleRealization.realizationId);
    QCOMPARE(sinkInstalls,std::size_t{0});

    // DEVICE vanishes AFTER the SOURCE/MANUAL provenance was captured.
    // The explicit offline FINAL MASTER may render, but audible M17 still
    // must fail and no terminal or anonymous sink may be installed.
    QVERIFY(product.cancel());
    QVERIFY(device->clear());
    QCOMPARE(device->state,core::PlaybackState::NO_SOURCE);
    QCOMPARE(device->audibleRealization.phase,
             core::AudibleHandoffPhase::UNAVAILABLE);
    QVERIFY(!device->audibleRealization.realizationId);
    QVERIFY(monitorUi.can_prepare_offline()); // device-refused fallback only
    // A non-QML caller cannot invoke the NO_SOURCE offline path without
    // the same explicit authorization used by the real UI.
    app::TerminalMonitorViewModel noOfflineAuthorizer{product,transport};
    QVERIFY(noOfflineAuthorizer.can_prepare());
    QVERIFY(!noOfflineAuthorizer.can_prepare_offline());
    const auto renderCountBeforeUnauthorized=renderer.render_invocation_count();
    QVERIFY(!noOfflineAuthorizer.requestMaster(192000));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QCOMPARE(renderer.render_invocation_count(),renderCountBeforeUnauthorized);
    app::TerminalMonitorViewModel deniedOfflineAuthorizer{product,transport};
    deniedOfflineAuthorizer.set_offline_master_authorizer([]{
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "TEST_OFFLINE_SOURCE_EPOCH_NOT_AUTHORIZED"});
    });
    QVERIFY(deniedOfflineAuthorizer.can_prepare_offline());
    QVERIFY(!deniedOfflineAuthorizer.requestMaster(192000));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QCOMPARE(renderer.render_invocation_count(),renderCountBeforeUnauthorized);
    QCOMPARE(deniedOfflineAuthorizer.error_text(),
        QStringLiteral("TEST_OFFLINE_SOURCE_EPOCH_NOT_AUTHORIZED"));
    QVERIFY(!product.request_private(*masterRate.value(),true));
    QVERIFY(monitorUi.can_prepare());
    QVERIFY(monitorUi.requestMaster(192000));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"),12000);
    const auto offlineMaster=candidates.staged();
    QVERIFY(offlineMaster);
    QCOMPARE(offlineMaster->rendered.output_rate,*masterRate.value());
    QVERIFY(offlineMaster->rendered.manual_owner
        == selector.processed_realization_snapshot());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QCOMPARE(sinkInstalls,std::size_t{0});
    QCOMPARE(device->state,core::PlaybackState::NO_SOURCE);
    // Full immutable MASTER materialization may produce private device PCM
    // but physical DEVICE INSTALL must still reject a missing MANUAL sink.
    auto offlineMonitor=product.materialize_private_final_master_monitoring(
        makeMonitor);
    QVERIFY(offlineMonitor);
    QVERIFY(!product.install_private_monitor_stopped(*offlineMonitor.value()));
    QVERIFY(!monitorUi.monitor_installed());
    QVERIFY(!audioGate.has_session());
    QCOMPARE(device->state,core::PlaybackState::NO_SOURCE);

    // A later foreign device owner revokes the offline no-sink authority.
    // Neither master materialization nor install may borrow its transport.
    device->state=core::PlaybackState::STOPPED;
    device->audibleRealization={
        core::AudibleHandoffPhase::NEW,core::RealizationId{0xABCD}};
    const auto hijacked=product.materialize_private_final_master_monitoring(
        makeMonitor);
    QVERIFY(!hijacked);
    QCOMPARE(hijacked.error()->code(),core::ErrorCode::InvalidState);
    QVERIFY(!product.install_private_monitor_stopped(*offlineMonitor.value()));
    QVERIFY(!audioGate.has_session());
    QVERIFY(product.cancel());
    QCOMPARE(product.status(),QStringLiteral("IDLE"));

    // Session-level analogue of INITIAL DEVICE rejection. This fixture
    // revokes the product's Selected callback WITHOUT mutating the
    // authoritative accepted SOURCE/MANUAL owner. The selector's actual
    // no-active-target rejection is covered by its own WAV-rate tests.
    // Clear the foreign test owner externally, never through the selector.
    QVERIFY(device->clear());
    productAudibleSelectionAccepted=false;
    bool explicitOfflineProvenance=false;
    product.set_offline_final_master_selection_checker(
        [&explicitOfflineProvenance] {return explicitOfflineProvenance;});
    QVERIFY(!product.request_private_for_final_master(*masterRate.value()));
    explicitOfflineProvenance=true;
    QVERIFY(!product.request_private(*masterRate.value(),true));
    QVERIFY(product.request_private_for_final_master(*masterRate.value()));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"),12000);
    QVERIFY(candidates.staged());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QCOMPARE(device->state,core::PlaybackState::NO_SOURCE);
    QVERIFY(!product.install_ready_private());
    explicitOfflineProvenance=false;
    QVERIFY(!product.materialize_private_final_master_monitoring(makeMonitor));
    QCOMPARE(device->state,core::PlaybackState::NO_SOURCE);
    QVERIFY(product.cancel());
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!candidates.staged());

    // Initial SOURCE/PROCESSED device-selection failure now has one
    // independent, product-explicit DEVICE path. No synthetic MANUAL
    // realization may appear, before OR after physical audition.
    product.set_offline_source_provenance_checker(
        [&sourceEpochCurrent]{return sourceEpochCurrent;});
    explicitOfflineProvenance=true;
    injectedMonitorInstallFailure=0;
    monitorMasterLoop.reset();
    QVERIFY(monitorUi.requestMaster(192000));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"),12000);
    auto preparedOfflineDevice=
        product.materialize_private_final_master_monitoring(makeMonitor);
    QVERIFY(preparedOfflineDevice);
    const auto restoreCount=manualRestores;
    QVERIFY(!product.install_ready_private()); // never ordinary M17
    QVERIFY(product.install_private_monitor_stopped(
        *preparedOfflineDevice.value()));
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_INSTALLED_STOPPED"));
    QCOMPARE(deviceMonitor,*preparedOfflineDevice.value());
    QCOMPARE(monitorUi.return_target_text(),
        QStringLiteral("Close DEVICE · No MANUAL sink"));
    QVERIFY(!device->audibleRealization.realizationId);
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QVERIFY(product.private_monitor_master_snapshot());
    QVERIFY(product.control_private_monitor(Request{Action::PLAY}));
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QVERIFY(product.control_private_monitor(Request{Action::PAUSE}));
    QVERIFY(!product.control_private_monitor(
        Request{Action::SET_MASTER_LOOP,core::FrameIndex{0},*region.value()}));
    QVERIFY(product.control_private_monitor(Request{Action::STOP}));
    // SOURCE provenance can expire while independently-owned PCM is active.
    // Subsequent control fails closed; exact-owner retirement MUST STILL
    // work and MUST NOT try to reinstall unsupported original SOURCE.
    sourceEpochCurrent=false;
    QVERIFY(!product.control_private_monitor(Request{Action::PLAY}));
    QVERIFY(monitorUi.returnManual());
    QCOMPARE(device->state,core::PlaybackState::NO_SOURCE);
    QCOMPARE(device->audibleRealization.phase,
             core::AudibleHandoffPhase::UNAVAILABLE);
    QVERIFY(!device->audibleRealization.realizationId);
    QVERIFY(!device->duration);
    QCOMPARE(manualRestores,restoreCount);
    QVERIFY(!deviceMonitor);
    QVERIFY(!product.monitoring_transaction_active());
    QVERIFY(!audioGate.has_session());
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QCOMPARE(monitorUi.status_text(),
        QStringLiteral("MASTER LISTENING IDLE · NO DEVICE PCM"));
    // SOURCE was refused by the physical device: returning from the
    // exclusive monitor MUST clear its readouts immediately. The UI must
    // never retain an apparently PLAYING ghost owner or borrowed rate.
    QCOMPARE(monitorUi.transport_state(),QStringLiteral("UNAVAILABLE"));
    QCOMPARE(monitorUi.device_rate_hz(),0);
    QCOMPARE(monitorUi.master_rate_hz(),0);
    QCOMPARE(monitorUi.master_position_frames(),qint64{0});
    QCOMPARE(monitorUi.master_duration_frames(),qint64{0});
    QVERIFY(!monitorUi.monitor_installed());
    QVERIFY(!monitorUi.play());
    QVERIFY(!monitorUi.pause());
    QVERIFY(!monitorUi.stop());
    QVERIFY(!device->audibleRealization.realizationId);

    // Fault injection: SOURCE changes in the narrow post-install
    // window while the worker DOES own the new monitor. A failure of
    // exact-device retirement must retain a recovery ticket. Retrying
    // Return must retire ONLY that verified owner, not invent MANUAL.
    sourceEpochCurrent=true;
    explicitOfflineProvenance=true;
    QVERIFY(product.request_private_for_final_master(*masterRate.value()));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("MASTER_READY_MONITOR_PENDING"),12000);
    auto faultMonitor=product.materialize_private_final_master_monitoring(
        makeMonitor);
    QVERIFY(faultMonitor);
    revokeOfflineEpochOnInstall=true;
    injectedMonitorRetireFailure=1;
    QVERIFY(!product.install_private_monitor_stopped(*faultMonitor.value()));
    QCOMPARE(product.status(),
        QStringLiteral("MONITOR_PRIVATE_RECOVERY_REQUIRED"));
    QCOMPARE(deviceMonitor,*faultMonitor.value());
    QVERIFY(monitorUi.monitor_installed());
    QCOMPARE(monitorUi.return_target_text(),
        QStringLiteral("Close DEVICE · No MANUAL sink"));
    QVERIFY(monitorUi.returnManual());
    QCOMPARE(device->state,core::PlaybackState::NO_SOURCE);
    QVERIFY(!deviceMonitor);
    QCOMPARE(manualRestores,restoreCount);
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!product.monitoring_transaction_active());

    // A foreign successor still must not be borrowed for another offline
    // job, even when a SOURCE lineage callback asserts 'current'.
    sourceEpochCurrent=true;
    device->state=core::PlaybackState::STOPPED;
    device->audibleRealization={
        core::AudibleHandoffPhase::NEW,core::RealizationId{0xBEEF}};
    QVERIFY(!product.request_private_for_final_master(*masterRate.value()));
    QCOMPARE(device->audibleRealization.realizationId,
             std::optional{core::RealizationId{0xBEEF}});
    QVERIFY(device->clear());
}

void TerminalActivationIntegrationTest::staleM17TerminalJobCancelsWithoutPublishingPcm()
{
    // This is a product-render boundary test: the protected per-block M17
    // process and its strict FP recurrence are untouched.
    using namespace rgsml;
    const auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    const auto params=dsp::TruePeakLimiterParameters::create(-12.0);
    const auto rate=core::SampleRate::create(44100);
    const auto block=core::FrameCount::create(64);
    const auto frames=core::FrameCount::create(320);
    QVERIFY(registry && params && rate && block && frames);
    const auto fmt=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(fmt);
    auto input=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::OUTPUT_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(input);
    std::size_t polls=0;
    const render::TerminalRenderCancel cancelled=[&] {
        return ++polls>=3;
    };
    const auto aborted=render::render_true_peak_limiter_terminal_candidate(
        input.value()->view(),*params.value(),*registry.value(),
        *block.value(),false,cancelled);
    QVERIFY(!aborted);
    QVERIFY(aborted.error());
    // An abandoned candidate cannot return even a partial owned PCM/GR.
    QVERIFY(std::string_view{aborted.error()->message()} ==
        std::string_view{
            "Stale terminal render was cancelled before complete publication."});
    QCOMPARE(polls,std::size_t{3});

    // A live, never-cancelled job remains binary64 bit-identical with the
    // previous no-callback render across both real stereo output lanes.
    const auto baseline=render::render_true_peak_limiter_terminal_candidate(
        input.value()->view(),*params.value(),*registry.value(),
        *block.value(),false);
    const render::TerminalRenderCancel never=[] {return false;};
    const auto live=render::render_true_peak_limiter_terminal_candidate(
        input.value()->view(),*params.value(),*registry.value(),
        *block.value(),false,never);
    QVERIFY(baseline && live);
    // Passive per-block counter is exact and MUST NOT perturb frozen M17 PCM.
    std::vector<render::TerminalRenderProgress> observed;
    const auto recorded=render::render_true_peak_limiter_terminal_candidate(
        input.value()->view(),*params.value(),*registry.value(),
        *block.value(),false,never,[&](auto p) {observed.push_back(p);});
    QVERIFY(recorded);
    QCOMPARE(observed.size(),std::size_t{5});
    for(std::size_t i=0;i<observed.size();++i) {
        QCOMPARE(observed[i].phase,render::TerminalRenderProgressPhase::M17_CORE);
        QCOMPARE(observed[i].processed_frames,static_cast<std::int64_t>((i+1)*64));
        QCOMPARE(observed[i].total_frames,std::int64_t{320});
    }
    QCOMPARE(baseline.value()->applied_gain_frames.size(),std::size_t{320});
    QCOMPARE(live.value()->applied_gain_frames.size(),std::size_t{320});
    const auto a=baseline.value()->compensated_pcm.view();
    const auto b=live.value()->compensated_pcm.view();
    const auto observedPcm=recorded.value()->compensated_pcm.view();
    for (std::size_t ch=0;ch<2;++ch) {
        const auto ca=a.channel(ch);
        const auto cb=b.channel(ch);
        const auto cc=observedPcm.channel(ch);
        QVERIFY(ca && cb && cc);
        QCOMPARE(ca.value()->size(),cb.value()->size());
        QCOMPARE(ca.value()->size(),cc.value()->size());
        for (std::size_t i=0;i<ca.value()->size();++i) {
            QCOMPARE(std::bit_cast<std::uint64_t>((*ca.value())[i]),
                     std::bit_cast<std::uint64_t>((*cb.value())[i]));
            QCOMPARE(std::bit_cast<std::uint64_t>((*ca.value())[i]),
                     std::bit_cast<std::uint64_t>((*cc.value())[i]));
        }
    }
}

} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::TerminalActivationIntegrationTest)
#include "test_terminal_product_activation.moc"
