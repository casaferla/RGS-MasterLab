#include "audition_source_selector.hpp"
#include "playback_transport_view_model.hpp"
#include "terminal_candidate_gate.hpp"
#include "terminal_audio_session_gate.hpp"
#include "terminal_private_publication_coordinator.hpp"
#include "terminal_private_render_controller.hpp"
#include "terminal_private_product_session.hpp"
#include "terminal_monitor_view_model.hpp"
#include "terminal_product_activation_coordinator.hpp"
#include "master_output_rate_state.hpp"
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
#include <array>
#include <bit>
#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>

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
    void audibleOnBypassRestoresExactManualSourceAndLoop();
    void finalMasterAt192kNeverBecomesDeviceRate48k();
    void sixSourceBySixTargetOfflineMasterAndDeviceRateMatrix();
    void privateMaster192kOn48kDeviceRemainsUninstalled();
    void staleM17TerminalJobCancelsWithoutPublishingPcm();
    void nonconformingPreviewRequiresSeparateExplicitConsent();
    void rejectedCeilingCannotAlterAuthority_data();
    void rejectedCeilingCannotAlterAuthority();
    void unchangedCeilingLeavesRevisionAndConsentUntouched();
};


// Adversarial parameter-input oracle: independent data rows run even if a
// different failure later aborts the full A/B integration scenario. No PCM,
// filename, or hardware certificate is created by these input tests.
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
    vm.previewNonconforming();
    QCOMPARE(unsafe.count(),0);
    QVERIFY(state.value()->set_user_bypass(false));
    vm.setAudioStatus(QStringLiteral(
        "NON-CONFORMING · -11.76 dBTP (target -12.00)"));
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
    vm.previewPrepared();
    QCOMPARE(verified.count(),0); // Not a verified READY candidate.
    vm.previewNonconforming();
    QCOMPARE(unsafe.count(),1); // Explicit action only.
    vm.setAudioStatus(QStringLiteral("READY · PREVIEW AVAILABLE"));
    vm.cancelPreparation();
    QCOMPARE(cancelled.count(),0); // READY is not cancellable.
    vm.setAudioStatus(QStringLiteral("Preparing audio"));
    vm.cancelPreparation();
    QCOMPARE(cancelled.count(),1); // Only active preparation can cancel.
    QVERIFY(state.value()->set_user_bypass(true));
    vm.previewNonconforming();
    QCOMPARE(unsafe.count(),1); // No audition from Bypass.
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
            if(finalHz!=sourceHz)continue;
            // 36 more genuine PCM conversions: six independently
            // prepared source-rate M17 masters versus six DEVICE rates.
            // DEVICE allocation never mutates the MASTER output clock.
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
                    sourceHz!=deviceHz);
                QCOMPARE(monitor.value()->device_pcm.view().format().sample_rate(),
                         *deviceRate.value());
                QCOMPARE(master.format().sample_rate(),*sourceRate.value());
                ++devicePairs;
            }
        }
    }
    QCOMPARE(masterPairs,std::size_t{36});
    QCOMPARE(wavPairs,std::size_t{36});
    QCOMPARE(devicePairs,std::size_t{36});
    qInfo().noquote()<<"M17_RATE_MATRIX=36_MASTER_PLUS_36_IEEE_F64_WAV_PLUS_36_DEVICE_PCM";
    qInfo().noquote()<<"M17_EDGE_CASE=RATE_GRID_36X36_MASTER_WAV_DEVICE";
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
    // Product cannot silently borrow an already installed terminal sink;
    // a future explicit ownership-retirement transaction is mandatory.
    QVERIFY(!product.borrow_verified_m17_master_for_device(
        *deviceRate.value()));
    auto rejectedInstalledReuse=
        product.materialize_borrowed_verified_m17_monitoring(
            *deviceRate.value(),convertedFromExisting);
    QVERIFY(!rejectedInstalledReuse);
    QCOMPARE(borrowedSrcCalls,std::size_t{2});
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

    // SOURCE Loop edit cancels only the pending/READY private job. New
    // request is explicit, source ownership is never silently substituted.
    const auto shiftedRegion=core::FrameRange::create(
        core::FrameIndex{700},core::FrameIndex{3300});
    QVERIFY(shiftedRegion);
    desiredLoop=*shiftedRegion.value();
    QVERIFY(transport.set_loop_source_range(desiredLoop));
    coordinator.loop_context_changed();
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    QCOMPARE(product.status(),QStringLiteral("IDLE"));
    QVERIFY(!coordinator.device_active());
    QVERIFY(!product.private_id());
    vm.requestPrepare();
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),
        QStringLiteral("READY_TO_INSTALL"),12000);
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

    // A wrong target is rejected at explicit Prepare, not when arming ON.
    QVERIFY(selector.switch_to(app::AuditionTarget::PREPARED));
    vm.setBypass(false);
    QCOMPARE(vm.audio_status(),QStringLiteral("NEEDS PREPARATION"));
    vm.requestPrepare();
    QVERIFY(limiter.value()->bypassed());
    QCOMPARE(vm.audio_status(),QStringLiteral("ERROR · BYPASS"));
    QVERIFY(vm.validation_message().contains(
        QStringLiteral("requires PROCESSED audition target")));
    QVERIFY(!coordinator.device_active());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());

    // The source and selected master here are both 44.1 kHz. The
    // deliberately failing DEVICE probe must not affect the independent
    // master-only request, and must not publish into any audio sink.
    // This test does NOT qualify the separate 44.1 -> 192 FIR.
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

    // The additional 44.1 -> 192 FINAL conversion still uses the
    // experimental rational FIR, which has NO certified SHA-256. The
    // authoritative M17 candidate gate must reject this unqualified
    // provenance rather than treating the 192-kHz PCM as VERIFIED.
    QVERIFY(product.request_private_for_final_master(*highMasterRate.value()));
    QTRY_COMPARE_WITH_TIMEOUT(product.status(),QStringLiteral("STALE"),12000);
    QVERIFY(!candidates.staged());
    QVERIFY(!product.private_id());
    QVERIFY(!audioGate.has_session());
    QCOMPARE(device->audibleRealization.realizationId,currentManualId);
    QVERIFY(product.cancel());

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
        QStringLiteral("MASTER VERIFIED · USE MASTER AUDITION"));
    QVERIFY(!product.current_master_matches_device_for_preview());
    const auto callsBeforeIgnoredPreview=renderer.render_invocation_count();
    vm.previewPrepared(); // disallowed: project MASTER != physical DEVICE
    QCOMPARE(renderer.render_invocation_count(),callsBeforeIgnoredPreview);
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(device->audibleRealization.realizationId,currentManualId);
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
    // Separate fresh 192-kHz SOURCE fixture: the certified identity-rate
    // master route can succeed without relying on the still-unqualified
    // 44.1 -> 192 experimental Final Output FIR, while the independently
    // probed 48-kHz sink must remain completely untouched.
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
    QVERIFY(product.verified_master_rate_for_monitoring());
    QCOMPARE(*product.verified_master_rate_for_monitoring(),
        *masterRate.value());
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
    QCOMPARE((*reusedNormal.value())->immutable_master_rate,
        *masterRate.value());
    QCOMPARE((*reusedNormal.value())->device_rate,*deviceRate.value());
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
    QVERIFY(monitorUi.returnManual());
    QCOMPARE(product.status(),QStringLiteral("READY_TO_INSTALL"));
    QCOMPARE(candidates.staged(),normalM17Stage);
    QCOMPARE(renderer.render_invocation_count(),beforeNormalM17+1);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QVERIFY(!monitorSinkOwner);
    QVERIFY(monitorUi.can_install());

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
    // M17 checked owner; its DEVICE audition inherits and preserves the
    // loop including on the return to SOURCE with exact half-open bounds.
    transport.stop();
    QVERIFY(product.cancel());
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
    QCOMPARE(requestedMasterLoop,std::optional{*sourceRegion.value()});
    const auto expectedDeviceLoop=core::FrameRange::create(
        core::FrameIndex{30},core::FrameIndex{300});
    QVERIFY(expectedDeviceLoop);
    QCOMPARE(device->loop,std::optional{*expectedDeviceLoop.value()});
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
    // A failure at SET_LOOP, SEEK or PLAY may leave partially updated
    // anonymous PCM; only recovery Return may touch it. No manual STOP.
    using SeedAction=platform::windows::MonitoringTransportAction;
    for(const auto failingAction:{
            SeedAction::SET_MASTER_LOOP,SeedAction::SEEK_MASTER,
            SeedAction::PLAY}) {
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
    // PAUSE failure is reached only after successful LOOP/SEEK/PLAY seeds.
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
    QCOMPARE(device->state,core::PlaybackState::PLAYING);
    QCOMPARE(device->audibleRealization.realizationId,manualId);
    QVERIFY(product.verified_master_rate_for_monitoring());
    QVERIFY(monitorUi.can_install()); // PLAY/Loop are not Prepare barriers.
    QVERIFY(product.cancel());
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
    QVERIFY(product.return_private_monitor_to_manual());
    QCOMPARE(monitorRetires,std::size_t{1});
    QCOMPARE(manualRestores,std::size_t{1});
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
    QCOMPARE(manualRestores,std::size_t{1});
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
    QCOMPARE(manualRestores,std::size_t{2});
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
    QCOMPARE(manualRestores,std::size_t{3});
    QCOMPARE(product.status(),QStringLiteral("MASTER_READY_MONITOR_PENDING"));
    QCOMPARE(sinkInstalls,std::size_t{0});
    QVERIFY(!product.private_id());
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
        QStringLiteral("MASTER AUDITION IDLE · NO PRIVATE DEVICE PCM"));
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
