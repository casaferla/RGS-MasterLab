#include "true_peak_static_binding.hpp"
#include "render_test_support.hpp"

#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QTest>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>
#include <utility>

namespace rgsml::tests {
namespace {

using namespace render_support;
using app::BoundStaticTruePeak;
using app::TruePeakExpectedSource;

struct RenderFixture final {
    std::shared_ptr<const render::RenderResult> result;
    TruePeakExpectedSource expected;
};

[[nodiscard]] core::Result<RenderFixture> make_render(
    std::int64_t count, core::FrameRange window,
    std::int64_t rate = 48000,
    audio::ChannelLayout layout = audio::ChannelLayout::STEREO_LR)
{
    const auto fmt = format(layout, rate);
    const auto frames = frame_count(count);
    auto source = audio::AudioBuffer::create(
        fmt, audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, frames);
    if (!source) return core::Result<RenderFixture>::failure(*source.error());
    {
        auto left = source.value()->mutable_view().channel(0);
        if (!left) return core::Result<RenderFixture>::failure(*left.error());
        for (std::int64_t i = 0; i < count; ++i) {
            (*left.value())[static_cast<std::size_t>(i)] =
                i == 1 ? 0.75 : 0.0;
        }
        if (layout == audio::ChannelLayout::STEREO_LR) {
            auto right = source.value()->mutable_view().channel(1);
            if (!right) return core::Result<RenderFixture>::failure(*right.error());
            for (std::int64_t i = 0; i < count; ++i) {
                (*right.value())[static_cast<std::size_t>(i)] =
                    i == 2 ? -0.5 : 0.0;
            }
        }
    }
    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    if (!registry) return core::Result<RenderFixture>::failure(*registry.error());
    auto chain = dsp::ProcessingChain::create(
        *registry.value(),
        dsp::ProcessingChainContext{dsp::ProcessingStage::MASTER,
                                    dsp::ChainSegment::MANUAL});
    if (!chain) return core::Result<RenderFixture>::failure(*chain.error());
    auto request = render::RenderRequest::create(
        source.value()->view(), window, *chain.value(),
        std::vector<dsp::ModuleExecutionBinding>{},
        frame_count(16));
    if (!request) return core::Result<RenderFixture>::failure(*request.error());
    auto rendered = render::render_preview(*request.value(), *registry.value());
    if (!rendered) return core::Result<RenderFixture>::failure(*rendered.error());
    return core::Result<RenderFixture>::success(RenderFixture{
        std::make_shared<const render::RenderResult>(std::move(*rendered.value())),
        TruePeakExpectedSource{frames, *core::SampleRate::create(rate).value(), layout}
    });
}

class TruePeakStaticBindingTest final : public QObject {
    Q_OBJECT
private slots:
    void fullSourceProvenanceAndIdentity();
    void partialRenderFailsClosed();
    void sourceMismatchAndMissingIdentityFailClosed();
    void monoSilenceAndUnqualifiedStatus();
};

void TruePeakStaticBindingTest::fullSourceProvenanceAndIdentity()
{
    auto fixture = make_render(4, frame_range(0,4));
    QVERIFY(fixture);
    const auto source = fixture.value()->result;
    auto got = app::measure_accepted_processed_true_peak(
        source, core::RealizationId{27}, fixture.value()->expected);
    QVERIFY(got);
    const auto& b = *got.value();
    QVERIFY(b.owner == source);
    QCOMPARE(b.realizationId.value, std::uint64_t{27});
    QVERIFY(b.coveredSourceFrames == frame_range(0,4));
    QCOMPARE(b.chainRevision, source->chain_revision());
    QCOMPARE(b.measurement.qualification_status(),
             analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
    auto numeric = analysis::measure_true_peak_full_resource(source->buffer());
    QVERIFY(numeric);
    QCOMPARE(b.measurement.global_linear_peak(), numeric.value()->global_linear_peak());
    QVERIFY(b.measurement.global_db_tp() == numeric.value()->global_db_tp());
    auto playback = core::PlaybackSnapshot{};
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{2};
    playback.audibleRealization = {
        core::AudibleHandoffPhase::NEW, core::RealizationId{27}, std::nullopt};
    QVERIFY(b.matches_audible(playback));
    playback.audibleRealization.realizationId = core::RealizationId{26};
    QVERIFY(!b.matches_audible(playback));
    playback.audibleRealization.realizationId = core::RealizationId{27};
    playback.audibleRealization.phase = core::AudibleHandoffPhase::OLD;
    QVERIFY(!b.matches_audible(playback));
    playback.audibleRealization.phase = core::AudibleHandoffPhase::TRANSITION;
    QVERIFY(!b.matches_audible(playback));
    playback.audibleRealization.phase = core::AudibleHandoffPhase::NEW;
    playback.state = core::PlaybackState::PAUSED;
    QVERIFY(!b.matches_audible(playback));
    playback.state = core::PlaybackState::PLAYING;
    playback.position = core::FrameIndex{4};
    QVERIFY(!b.matches_audible(playback));
}

void TruePeakStaticBindingTest::partialRenderFailsClosed()
{
    auto partial = make_render(4, frame_range(1,3));
    QVERIFY(partial);
    auto res = app::measure_accepted_processed_true_peak(
        partial.value()->result, core::RealizationId{28}, partial.value()->expected);
    QVERIFY(!res);
    QCOMPARE(res.error()->code(), core::ErrorCode::InvalidState);
}

void TruePeakStaticBindingTest::sourceMismatchAndMissingIdentityFailClosed()
{
    auto full = make_render(4, frame_range(0,4));
    QVERIFY(full);
    auto expected = full.value()->expected;
    auto missing = app::measure_accepted_processed_true_peak(
        full.value()->result, core::RealizationId{0}, expected);
    QVERIFY(!missing);
    auto nullRender = app::measure_accepted_processed_true_peak(
        nullptr, core::RealizationId{7}, expected);
    QVERIFY(!nullRender);
    expected.frameCount = frame_count(5);
    auto badCount = app::measure_accepted_processed_true_peak(
        full.value()->result, core::RealizationId{7}, expected);
    QVERIFY(!badCount);
    expected = full.value()->expected;
    expected.sampleRate = *core::SampleRate::create(44100).value();
    auto badRate = app::measure_accepted_processed_true_peak(
        full.value()->result, core::RealizationId{7}, expected);
    QVERIFY(!badRate);
    expected = full.value()->expected;
    expected.layout = audio::ChannelLayout::MONO_C;
    auto badLayout = app::measure_accepted_processed_true_peak(
        full.value()->result, core::RealizationId{7}, expected);
    QVERIFY(!badLayout);
}

void TruePeakStaticBindingTest::monoSilenceAndUnqualifiedStatus()
{
    auto fixture = make_render(4, frame_range(0,4), 96000,
                               audio::ChannelLayout::MONO_C);
    QVERIFY(fixture);
    auto result = app::measure_accepted_processed_true_peak(
        fixture.value()->result, core::RealizationId{29}, fixture.value()->expected);
    QVERIFY(result);
    QCOMPARE(result.value()->measurement.channel_count(), std::size_t{1});
    QCOMPARE(result.value()->measurement.qualification_status(),
             analysis::TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
}

} // namespace
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::TruePeakStaticBindingTest)
#include "test_true_peak_static_binding.moc"
