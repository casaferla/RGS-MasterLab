#include "terminal_consumed_output_binding.hpp"
#include "terminal_limiter_state.hpp"
#include "terminal_private_gr_view_model.hpp"
#include "terminal_post_limiter_true_peak_coordinator.hpp"
#include <rgsml/core/error.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <QtTest/QTest>
#include <QSignalSpy>
#include <cmath>
#include <memory>
#include <string>

namespace rgsml::tests {
class TerminalPrivateGrProjectionTest final : public QObject {
    Q_OBJECT
private slots:
    void gainWitnessRateMatrix_data();
    void gainWitnessRateMatrix();
    void gainWitnessProjectionBoundedTail();
    void qmlPresentationFailClosed();
    void postLimiterMeterRejectsUnverifiedAndUsesActualOutputPcm();
};

void TerminalPrivateGrProjectionTest::gainWitnessRateMatrix_data()
{
    QTest::addColumn<int>("sourceHz");
    QTest::addColumn<int>("outputHz");
    QTest::newRow("source-44100-master-44100") << 44100 << 44100;
    QTest::newRow("source-44100-explicit-master-48000") << 44100 << 48000;
    QTest::newRow("native-48000-master-device") << 48000 << 48000;
}

void TerminalPrivateGrProjectionTest::gainWitnessRateMatrix()
{
    QFETCH(int,sourceHz);
    QFETCH(int,outputHz);
    using namespace rgsml;
    const auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto manual=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    const auto uuid=core::Uuid::parse(
        "7f000000-0000-4000-8000-000000000017");
    QVERIFY(uuid);
    const auto instanceId=dsp::ModuleInstanceId::from_uuid(*uuid.value());
    QVERIFY(instanceId);
    auto terminal=app::TerminalLimiterState::create_default(
        *registry.value(),*instanceId.value());
    QVERIFY(terminal);
    const auto sourceRate=core::SampleRate::create(sourceHz);
    const auto outputRate=core::SampleRate::create(outputHz);
    QVERIFY(sourceRate);
    QVERIFY(outputRate);
    const auto format=audio::AudioFormat::create(
        *sourceRate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(format);
    const auto frames=core::FrameCount::create(sourceHz/100);
    const auto block=core::FrameCount::create(17);
    QVERIFY(frames);
    QVERIFY(block);
    auto buffer=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(buffer);
    for(std::size_t ch=0;ch<2;++ch) {
        auto lane=buffer.value()->mutable_view().channel(ch);
        QVERIFY(lane);
        for(std::size_t i=0;i<static_cast<std::size_t>(sourceHz/100);++i)
            (*lane.value())[i]=(i==0)
                ?(ch==0 ? 4.0 : -3.0) : (ch==0 ? 0.25 : -0.125);
    }
    const auto request=render::RenderRequest::create(
        buffer.value()->view(),buffer.value()->view().absolute_range(),
        *manual.value(),{},*block.value());
    QVERIFY(request);
    auto rendered=render::render_preview(*request.value(),*registry.value());
    QVERIFY(rendered);
    const auto owner=std::make_shared<const render::RenderResult>(
        std::move(*rendered.value()));
    QVERIFY(terminal.value()->set_user_bypass(false));
    // Future WAV master target is INDEPENDENT of the device. Missing
    // requested rate preserves the source exactly, 44.1->48 is only an
    // EXPLICIT final-master selection; all SRC precedes M17/M16.
    const std::optional<core::SampleRate> requestedMasterRate =
        sourceHz==outputHz ? std::nullopt
        : std::optional<core::SampleRate>{*outputRate.value()};
    auto master=render::render_true_peak_limiter_owned_final_master_candidate(
        owner,*manual.value(),*frames.value(),manual.value()->revision(),
        requestedMasterRate,terminal.value()->chain(),
        terminal.value()->execution_bindings(),*registry.value(),*block.value());
    QVERIFY(master);
    QCOMPARE(master.value()->master_sample_rate,*outputRate.value());
    QVERIFY(render::validate_final_master_encoding_rate(
        *master.value(),*outputRate.value()));
    auto wrongRate=core::SampleRate::create(outputHz==44100 ? 48000 : 44100);
    QVERIFY(wrongRate);
    QVERIFY(!render::validate_final_master_encoding_rate(
        *master.value(),*wrongRate.value()));
    auto output=std::move(master.value()->output);
    QCOMPARE(output.output_rate,*outputRate.value());
    QCOMPARE(output.used_candidate_src,sourceHz!=outputHz);
    QCOMPARE(output.terminal.checked.terminal.compensated_pcm
        .view().frame_count().value(),std::int64_t{outputHz/100});
    QVERIFY(output.terminal.checked.final_check);
    auto stage=std::make_shared<const app::TerminalCandidateGate::StagedCandidate>(
        app::TerminalCandidateGate::StagedCandidate{
            std::move(output),core::RealizationId{33},owner});
    // Synthetic consumed interval in this unit test, NOT physical
    // sink chronology. The real observer must validate it separately.
    const app::TerminalConsumedOutputEvidence proof{
        stage,core::RealizationId{44},9,outputHz/100,0,outputHz/100,0};
    const auto history=app::project_private_terminal_gr(proof);
    QVERIFY(history);
    QCOMPARE(history.value()->realizationId,core::RealizationId{44});
    QCOMPARE(history.value()->consumedFrameBegin,std::int64_t{0});
    QCOMPARE(history.value()->consumedFrameEndExclusive,
        std::int64_t{outputHz/100});
    QVERIFY(!history.value()->buckets.empty());
    QVERIFY(history.value()->buckets.size()<=256);
    bool reduction=false;
    std::int64_t cursor=0;
    for(const auto& bucket:history.value()->buckets) {
        QCOMPARE(bucket.frameBegin,cursor);
        QVERIFY(bucket.frameEndExclusive>cursor);
        QVERIFY(std::isfinite(bucket.peakReductionDb));
        QVERIFY(bucket.peakReductionDb>=0);
        reduction |= bucket.peakReductionDb>0;
        cursor=bucket.frameEndExclusive;
    }
    QCOMPARE(cursor,std::int64_t{outputHz/100});
    QVERIFY(reduction);
    auto narrow=proof;
    narrow.firstTerminalFrame=111;
    narrow.lastTerminalFrameExclusive=117;
    const auto narrowed=app::project_private_terminal_gr(narrow);
    QVERIFY(narrowed);
    QCOMPARE(narrowed.value()->buckets.size(),std::size_t{6});
    narrow.firstTerminalFrame=-1;
    QVERIFY(!app::project_private_terminal_gr(narrow));
    narrow=proof;
    narrow.lastTerminalFrameExclusive=outputHz/100+1;
    QVERIFY(!app::project_private_terminal_gr(narrow));
    narrow=proof;
    narrow.playbackGeneration=0;
    QVERIFY(!app::project_private_terminal_gr(narrow));
    narrow=proof;
    narrow.realizationId=core::RealizationId{33};
    QVERIFY(!app::project_private_terminal_gr(narrow));
    narrow=proof;
    narrow.owner.reset();
    QVERIFY(!app::project_private_terminal_gr(narrow));
    QVERIFY(terminal.value()->set_user_bypass(true));
    auto byp=render::render_true_peak_limiter_owned_output_rate_candidate(
        owner,*manual.value(),*frames.value(),manual.value()->revision(),
        *outputRate.value(),terminal.value()->chain(),
        terminal.value()->execution_bindings(),*registry.value(),*block.value());
    QVERIFY(byp);
    auto bypStage=std::make_shared<
        const app::TerminalCandidateGate::StagedCandidate>(
        app::TerminalCandidateGate::StagedCandidate{
            std::move(*byp.value()),core::RealizationId{33},owner});
    auto bypProof=proof;
    bypProof.owner=bypStage;
    QVERIFY(!app::project_private_terminal_gr(bypProof));
}


void TerminalPrivateGrProjectionTest::gainWitnessProjectionBoundedTail()
{
    using namespace rgsml;
    const auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto chain=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto instance=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("7f000000-0000-4000-8000-000000000127").value());
    QVERIFY(instance);
    auto limiter=app::TerminalLimiterState::create_default(
        *registry.value(),*instance.value());
    QVERIFY(limiter);
    QVERIFY(limiter.value()->set_user_bypass(false));
    const auto rate=core::SampleRate::create(48000);
    const auto count=core::FrameCount::create(48017); // > one-second GUI tail
    const auto block=core::FrameCount::create(1024);
    QVERIFY(rate&&count&&block);
    const auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(format);
    auto source=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*count.value());
    QVERIFY(source);
    for(std::size_t channel=0;channel<2;++channel) {
        auto lane=source.value()->mutable_view().channel(channel);
        QVERIFY(lane);
        for(std::size_t i=0;i<48017;++i)
            (*lane.value())[i]=(i==0 ? 3.0 : 0.125)
                *(channel==0 ? 1.0 : -1.0);
    }
    const auto request=render::RenderRequest::create(
        source.value()->view(),source.value()->view().absolute_range(),
        *chain.value(),{},*block.value());
    QVERIFY(request);
    auto manual=render::render_preview(*request.value(),*registry.value());
    QVERIFY(manual);
    auto owner=std::make_shared<const render::RenderResult>(
        std::move(*manual.value()));
    auto rendered=render::render_true_peak_limiter_owned_output_rate_candidate(
        owner,*chain.value(),*count.value(),chain.value()->revision(),
        *rate.value(),limiter.value()->chain(),
        limiter.value()->execution_bindings(),*registry.value(),*block.value());
    QVERIFY(rendered);
    const auto stage=std::make_shared<const app::TerminalCandidateGate::StagedCandidate>(
        app::TerminalCandidateGate::StagedCandidate{
            std::move(*rendered.value()),core::RealizationId{101},owner});
    const app::TerminalConsumedOutputEvidence proof{
        stage,core::RealizationId{102},17,48017,0,48017,0};
    const auto history=app::project_private_terminal_gr(proof);
    QVERIFY(history);
    // Bounded GUI work and real terminal-frame indices, not a remapped
    // fabricated zero-origin tail. The sound/render still covers 48017 frames.
    QCOMPARE(history.value()->consumedFrameBegin,std::int64_t{17});
    QCOMPARE(history.value()->consumedFrameEndExclusive,std::int64_t{48017});
    QVERIFY(history.value()->buckets.size()<=256);
    QCOMPARE(history.value()->buckets.front().frameBegin,std::int64_t{17});
    QCOMPARE(history.value()->buckets.back().frameEndExclusive,
             std::int64_t{48017});
    auto narrow=proof;
    narrow.firstTerminalFrame=47900;
    const auto narrowHistory=app::project_private_terminal_gr(narrow);
    QVERIFY(narrowHistory);
    QCOMPARE(narrowHistory.value()->consumedFrameBegin,std::int64_t{47900});
    QCOMPARE(narrowHistory.value()->consumedFrameEndExclusive,
             std::int64_t{48017});
    QCOMPARE(narrowHistory.value()->buckets.size(),std::size_t{117});
}

void TerminalPrivateGrProjectionTest::qmlPresentationFailClosed()
{
    using namespace rgsml;
    using Series=app::TerminalPrivateGrSeries;
    std::optional<Series> source;
    bool workerRejects=false;
    app::TerminalPrivateGrViewModel provider([&]()->core::Result<Series> {
        if(workerRejects || !source)
            return core::Result<Series>::failure(core::Error{
                core::ErrorCode::InvalidState,"No consumed OUTPUT_RATE witness."});
        return core::Result<Series>::success(*source);
    });
    QSignalSpy changed(&provider,&app::TerminalPrivateGrViewModel::changed);
    QVERIFY(changed.isValid());
    QVERIFY(!provider.verified());
    QVERIFY(provider.verified_series().isEmpty());
    provider.refresh();
    QVERIFY(!provider.verified());
    QCOMPARE(changed.size(),0);

    // Production can only supply this source through the authentic
    // observe_private_terminal_gr(product,WindowsPrefixReader) callback.
    // This standalone presentation fixture deliberately synthesizes a
    // positive *result* to test fail-closed UI conversion, not the sink.
    source=Series{core::RealizationId{8201},42,100,104,
        {{100,102,3.0},{102,104,1.5}}};
    provider.refresh();
    QVERIFY(provider.verified());
    QCOMPARE(changed.size(),1);
    QCOMPARE(provider.verified_series().size(),qsizetype{2});
    QCOMPARE(provider.verified_series().at(0).toMap()
        .value(QStringLiteral("peakReductionDb")).toDouble(),3.0);
    QCOMPARE(provider.verified_series().at(1).toMap()
        .value(QStringLiteral("frameEndExclusive")).toLongLong(),
        qint64{104});
    provider.refresh();
    QCOMPARE(changed.size(),1); // no repaint of identical chronology
    source->buckets[0].peakReductionDb=-1.0;
    provider.refresh();
    QVERIFY(!provider.verified());
    QVERIFY(provider.verified_series().isEmpty());
    QCOMPARE(changed.size(),2);
    source->buckets[0].peakReductionDb=3.0;
    source->buckets[1].frameBegin=103; // consumed chronology gap
    provider.refresh();
    QVERIFY(!provider.verified());
    source->buckets[1].frameBegin=102;
    source->playbackGeneration=0;
    provider.refresh();
    QVERIFY(!provider.verified());
    source->playbackGeneration=43;
    provider.refresh();
    QVERIFY(provider.verified());
    workerRejects=true; // Paused/Seek/Loop/Handoff/worker reset
    provider.refresh();
    QVERIFY(!provider.verified());
    QVERIFY(provider.verified_series().isEmpty());
    workerRejects=false;
    provider.refresh();
    QVERIFY(provider.verified());
    // A verified source/identity and a new SOURCE traversal may coexist
    // in the Compressor-style GR strip only with an explicit split.
    // The missing worker proof NEVER permits the old trace to be shown.
    workerRejects=true;
    provider.refresh();
    QVERIFY(!provider.verified());
    QVERIFY(provider.verified_series().isEmpty());
    workerRejects=false;
    source->seekSerial=1;
    source->consumedFrameBegin=200;
    source->consumedFrameEndExclusive=204;
    source->buckets={{200,202,2.0},{202,204,1.0}};
    provider.refresh();
    QVERIFY(provider.verified());
    auto bridged=provider.verified_series();
    QCOMPARE(bridged.size(),qsizetype{4});
    QVERIFY(!bridged.at(1).toMap()
        .value(QStringLiteral("breakBefore")).toBool());
    QVERIFY(bridged.at(2).toMap()
        .value(QStringLiteral("breakBefore")).toBool());
    QVERIFY(!bridged.at(2).toMap()
        .value(QStringLiteral("loopBefore")).toBool());
    QCOMPARE(bridged.at(1).toMap()
        .value(QStringLiteral("frameEndExclusive")).toLongLong(),qint64{104});
    QCOMPARE(bridged.at(2).toMap()
        .value(QStringLiteral("frameBegin")).toLongLong(),qint64{200});
    // Continuation of the SAME traversal preserves the dashed break.
    source->consumedFrameEndExclusive=206;
    source->buckets.push_back({204,206,0.5});
    provider.refresh();
    QVERIFY(provider.verified());
    bridged=provider.verified_series();
    QCOMPARE(bridged.size(),qsizetype{5});
    QVERIFY(bridged.at(2).toMap()
        .value(QStringLiteral("breakBefore")).toBool());

    // Independently proved Loop wrap gets the same two-slot break,
    // but grey instead of copper, as for the Compressor grammar.
    source->loopWrapCount=1;
    source->traversalSerial=2;
    source->consumedFrameBegin=100;
    source->consumedFrameEndExclusive=102;
    source->buckets={{100,102,4.0}};
    provider.refresh();
    QVERIFY(provider.verified());
    bridged=provider.verified_series();
    // The second discontinuity keeps only the latest verified traversal,
    // not the previous already-combined display history (no duplication).
    QCOMPARE(bridged.size(),qsizetype{4});
    QCOMPARE(bridged.front().toMap()
        .value(QStringLiteral("frameBegin")).toLongLong(),qint64{200});
    QVERIFY(bridged.back().toMap()
        .value(QStringLiteral("breakBefore")).toBool());
    QVERIFY(bridged.back().toMap()
        .value(QStringLiteral("loopBefore")).toBool());

    // A different realization is NEVER drawn alongside old GR history.
    source->realizationId=core::RealizationId{8202};
    provider.refresh();
    QVERIFY(provider.verified());
    QCOMPARE(provider.verified_series().size(),qsizetype{1});
    QVERIFY(!provider.verified_series().front().toMap()
        .value(QStringLiteral("breakBefore")).toBool());

    provider.invalidate();
    QVERIFY(!provider.verified());
    QVERIFY(provider.verified_series().isEmpty());
    provider.refresh();
    QVERIFY(provider.verified());
    QCOMPARE(provider.verified_series().size(),qsizetype{1});
    QVERIFY(!provider.verified_series().front().toMap()
        .value(QStringLiteral("breakBefore")).toBool());
}

void TerminalPrivateGrProjectionTest::postLimiterMeterRejectsUnverifiedAndUsesActualOutputPcm()
{
    using namespace rgsml;
    using Evidence=app::TerminalConsumedOutputEvidence;
    std::optional<Evidence> proof;
    app::TerminalPostLimiterTruePeakCoordinator meter([&]()
        ->core::Result<Evidence> {
        if(!proof)
            return core::Result<Evidence>::failure(core::Error{
                core::ErrorCode::InvalidState,"No verified Windows sink prefix."});
        return core::Result<Evidence>::success(*proof);
    });
    QVERIFY(!meter.recent_available());
    QVERIFY(!meter.hold_available());
    QCOMPARE(meter.left_text(),QStringLiteral("—"));
    QTRY_VERIFY_WITH_TIMEOUT(meter.monitoring_attempted(),1000);
    QVERIFY(!meter.recent_available());
    QVERIFY(meter.status_text().contains(
        QStringLiteral("No verified Windows sink prefix.")));
    QSignalSpy diagnosticSignals(
        &meter,&app::TerminalPostLimiterTruePeakCoordinator::changed);
    QVERIFY(diagnosticSignals.isValid());
    QTest::qWait(160);
    // Identical 50-ms failure polls must not repeatedly reset FIR, history
    // or redraw the complete editor; the original diagnostic stays visible.
    QCOMPARE(diagnosticSignals.size(),0);
    QVERIFY(meter.status_text().contains(
        QStringLiteral("No verified Windows sink prefix.")));

    const auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto manual=dsp::ProcessingChain::create(*registry.value(),
        {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(manual);
    const auto instance=dsp::ModuleInstanceId::from_uuid(
        *core::Uuid::parse("7f000000-0000-4000-8000-000000000118").value());
    QVERIFY(instance);
    auto limiter=app::TerminalLimiterState::create_default(
        *registry.value(),*instance.value());
    QVERIFY(limiter);
    QVERIFY(limiter.value()->set_user_bypass(false));
    const auto rate=core::SampleRate::create(48000);
    const auto frames=core::FrameCount::create(5760); // 120 ms
    const auto block=core::FrameCount::create(257);
    QVERIFY(rate && frames && block);
    const auto format=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::STEREO_LR);
    QVERIFY(format);
    auto pcm=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(pcm);
    for(std::size_t channel=0;channel<2;++channel) {
        const auto lane=pcm.value()->mutable_view().channel(channel);
        QVERIFY(lane);
        for(std::size_t i=0;i<5760;++i)
            (*lane.value())[i]=channel==0 ? 0.72 : -0.57;
    }
    const auto requested=render::RenderRequest::create(
        pcm.value()->view(),pcm.value()->view().absolute_range(),
        *manual.value(),{},*block.value());
    QVERIFY(requested);
    auto rendered=render::render_preview(
        *requested.value(),*registry.value());
    QVERIFY(rendered);
    auto owner=std::make_shared<const render::RenderResult>(
        std::move(*rendered.value()));
    auto candidate=render::render_true_peak_limiter_owned_final_master_candidate(
        owner,*manual.value(),*frames.value(),manual.value()->revision(),
        std::nullopt,limiter.value()->chain(),
        limiter.value()->execution_bindings(),*registry.value(),*block.value());
    QVERIFY(candidate);
    auto stage=std::make_shared<const app::TerminalCandidateGate::StagedCandidate>(
        app::TerminalCandidateGate::StagedCandidate{
            std::move(candidate.value()->output),core::RealizationId{91},owner});
    // Only this TEST injects an evidence RESULT; in production the observer
    // is exclusively observe_private_terminal_consumed_output() with the
    // independently consumed, generation-fenced Windows prefix.
    proof=Evidence{stage,core::RealizationId{92},17,5760,0,5760,0};
    QTRY_VERIFY_WITH_TIMEOUT(meter.recent_available(),10000);
    QVERIFY(meter.monitoring_attempted());
    QCOMPARE(meter.channel_count(),2);
    QVERIFY(meter.status_text().contains(QStringLiteral("POST-LIMITER")));
    QVERIFY(meter.left_text()!=QStringLiteral("—"));
    QVERIFY(meter.right_text()!=QStringLiteral("—"));
    QVERIFY(!meter.hold_available()); // 120 ms does not prove 1s hold.
    proof.reset();
    QTRY_VERIFY_WITH_TIMEOUT(!meter.recent_available(),3000);
    QVERIFY(!meter.hold_available());
    QCOMPARE(meter.peak_text(),QStringLiteral("UNAVAILABLE"));
    QCOMPARE(meter.left_level(),0.0);
    QTest::qWait(100);
    QVERIFY(!meter.recent_available()); // never reuse an old owner on GAP

    // Independent Windows sink-prefix binding regression: a real prefix may
    // retain an older loop traversal and a GAP before the most recent,
    // independently proven one-to-one OUTPUT_RATE suffix. A fresh meter may
    // use ONLY that terminal suffix, never bridge FIR/HOLD over the GAP.
    const auto loop=core::FrameRange::create(
        core::FrameIndex{150},core::FrameIndex{5650});
    QVERIFY(loop);
    core::PlaybackSnapshot playback{};
    playback.state=core::PlaybackState::PLAYING;
    playback.position=core::FrameIndex{5200};
    playback.duration=*frames.value();
    playback.loop=*loop.value();
    playback.audibleRealization={
        core::AudibleHandoffPhase::NEW,core::RealizationId{92}};
    playback.loopWrapCount=1;
    app::TerminalPrivateProductSession::InstalledOutputIdentity identity{
        stage,core::RealizationId{92},playback,
        app::TerminalSourceTransportPort::DisplaySnapshot{
            core::PlaybackState::PLAYING,core::FrameIndex{5200},
            *frames.value(),*loop.value()}};
    namespace win=rgsml::platform::windows;
    win::WindowsConsumedOutputPrefix prefix{};
    prefix.generation=17;
    prefix.retainedOutputBegin=0;
    prefix.processedWatermarkExclusive=5600;
    prefix.partitionRepresented=true;
    prefix.reason=win::WindowsConsumedGapReason::UNPROVEN_LOOP;
    prefix.count=3;
    auto& old=prefix.slices[0];
    old.generation=17;
    old.outputBegin=0;
    old.outputEnd=100;
    old.kind=win::WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    old.realizationId=core::RealizationId{92};
    old.sourceBegin=300;
    old.sourceEnd=400;
    old.loopTraversalProven=true;
    old.loopWrapCount=0; // older traversal, not valid for current FIR.
    old.handoffPureNewProven=true;
    auto& gap=prefix.slices[1];
    gap.generation=17;
    gap.outputBegin=100;
    gap.outputEnd=200;
    gap.kind=win::WindowsConsumedSliceKind::GAP_UNPROVEN;
    gap.reason=win::WindowsConsumedGapReason::UNPROVEN_LOOP;
    auto& tail=prefix.slices[2];
    tail.generation=17;
    tail.outputBegin=200;
    tail.outputEnd=5600;
    tail.kind=win::WindowsConsumedSliceKind::SOURCE_CANDIDATE_NOT_QUALIFIED;
    tail.realizationId=core::RealizationId{92};
    tail.sourceBegin=200;
    tail.sourceEnd=5600;
    tail.loopTraversalProven=true;
    tail.loopWrapCount=1;
    tail.handoffPureNewProven=true;
    auto verified=app::bind_private_terminal_consumed_output(
        identity,prefix,identity);
    QVERIFY(verified);
    QCOMPARE(verified.value()->firstTerminalFrame,std::int64_t{200});
    QCOMPARE(verified.value()->lastTerminalFrameExclusive,std::int64_t{5600});
    QCOMPARE(verified.value()->loopWrapCount,std::uint64_t{1});
    QCOMPARE(verified.value()->realizationId,core::RealizationId{92});
    // The post-limiter FIR receives 5400 current proven frames, not the
    // previous traversal or 100 GAP frames; enough for a genuine 100ms peak.
    // Distinct diagnostic classes must remain equally fail-closed. A
    // reported SRC mismatch is not interchangeable with a missing device
    // chronology (particularly for a 44.1k source and 48k audition device).
    auto missing=prefix;
    missing.partitionRepresented=false;
    const auto missingResult=app::bind_private_terminal_consumed_output(
        identity,missing,identity);
    QVERIFY(!missingResult);
    QVERIFY(missingResult.error()->message().find(
        "no complete consumed OUTPUT partition")!=std::string::npos);
    auto srcDerived=prefix;
    srcDerived.srcInputRateHz=44100;
    srcDerived.srcOutputRateHz=48000;
    const auto srcResult=app::bind_private_terminal_consumed_output(
        identity,srcDerived,identity);
    QVERIFY(!srcResult);
    QVERIFY(srcResult.error()->message().find("SRC-derived")
        !=std::string::npos);
    auto staleClock=prefix;
    staleClock.reason=win::WindowsConsumedGapReason::STALE_GENERATION;
    const auto staleClockResult=app::bind_private_terminal_consumed_output(
        identity,staleClock,identity);
    QVERIFY(!staleClockResult);
    QVERIFY(staleClockResult.error()->message().find("worker clock")
        !=std::string::npos);
    auto invalid=prefix;
    invalid.slices[2].realizationId=core::RealizationId{93};
    QVERIFY(!app::bind_private_terminal_consumed_output(
        identity,invalid,identity));
    invalid=prefix;
    invalid.slices[2].handoffPureNewProven=false;
    QVERIFY(!app::bind_private_terminal_consumed_output(
        identity,invalid,identity));
    invalid=prefix;
    invalid.slices[2].loopWrapCount=0;
    QVERIFY(!app::bind_private_terminal_consumed_output(
        identity,invalid,identity));
    invalid=prefix;
    invalid.slices[2].kind=win::WindowsConsumedSliceKind::GAP_UNPROVEN;
    QVERIFY(!app::bind_private_terminal_consumed_output(
        identity,invalid,identity));
    invalid=prefix;
    invalid.slices[1].outputEnd=199; // torn partition, never trust suffix.
    QVERIFY(!app::bind_private_terminal_consumed_output(
        identity,invalid,identity));
    auto stale=identity;
    stale.playback.seekSerial=1;
    QVERIFY(!app::bind_private_terminal_consumed_output(
        identity,prefix,stale));
}

} // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::TerminalPrivateGrProjectionTest)
#include "test_terminal_private_gr_projection.moc"
