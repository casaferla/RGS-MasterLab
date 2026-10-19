#include "terminal_consumed_output_binding.hpp"
#include "terminal_limiter_state.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <QtTest/QTest>
#include <cmath>
#include <memory>

namespace rgsml::tests {
class TerminalPrivateGrProjectionTest final : public QObject {
    Q_OBJECT
private slots:
    void gainWitnessRateMatrix_data();
    void gainWitnessRateMatrix();
};

void TerminalPrivateGrProjectionTest::gainWitnessRateMatrix_data()
{
    QTest::addColumn<int>("sourceHz");
    QTest::addColumn<int>("outputHz");
    QTest::newRow("source-44100-master-44100") << 44100 << 44100;
    QTest::newRow("source-44100-device-48000") << 44100 << 48000;
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
    auto output=render::render_true_peak_limiter_owned_output_rate_candidate(
        owner,*manual.value(),*frames.value(),manual.value()->revision(),
        *outputRate.value(),terminal.value()->chain(),
        terminal.value()->execution_bindings(),*registry.value(),*block.value());
    QVERIFY(output);
    QCOMPARE(output.value()->output_rate,*outputRate.value());
    QCOMPARE(output.value()->used_candidate_src,sourceHz!=outputHz);
    QCOMPARE(output.value()->terminal.checked.terminal.compensated_pcm
        .view().frame_count().value(),std::int64_t{outputHz/100});
    QVERIFY(output.value()->terminal.checked.final_check);
    auto stage=std::make_shared<const app::TerminalCandidateGate::StagedCandidate>(
        app::TerminalCandidateGate::StagedCandidate{
            std::move(*output.value()),core::RealizationId{33},owner});
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
} // namespace rgsml::tests

QTEST_GUILESS_MAIN(rgsml::tests::TerminalPrivateGrProjectionTest)
#include "test_terminal_private_gr_projection.moc"
