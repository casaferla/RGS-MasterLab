#include "internal/l3_m06_d03d04_to_m07_m08.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <QtTest/QTest>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace rgsml::tests {
namespace {
namespace real=rgsml::app::l3_automatch::m05m06_real;
[[nodiscard]] core::Result<audio::AudioBuffer> source(double amplitude) {
    auto fs=core::SampleRate::create(48000);
    auto fmt=audio::AudioFormat::create(
        *fs.value(),audio::ChannelLayout::STEREO_LR);
    auto frames=core::FrameCount::create(480000);
    auto audio=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    if(audio) {
        auto l=audio.value()->mutable_view().channel(0);
        auto r=audio.value()->mutable_view().channel(1);
        for(std::size_t i=0;i<l.value()->size();++i) {
            (*l.value())[i]=amplitude;
            (*r.value())[i]=-amplitude;
        }
    }
    return audio;
}
[[nodiscard]] std::optional<real::D03D04FullEvidence> evidence() {
    auto prep=source(0.25),ref=source(0.5),now=source(0.375);
    if(!prep||!ref||!now)return std::nullopt;
    return real::D03D04FullEvidence{
        real::observe_d03_d04_real_pcm(prep.value()->view()),
        real::observe_d03_d04_real_pcm(ref.value()->view()),
        real::observe_d03_d04_real_pcm(now.value()->view()),
        {"PREP",1,true},{"REFERENCE",2,true},{"CANDIDATE",3,true},
        true,true,"authenticated-phase6-feature-revision-sha"
    };
}
class M06NativeBindingTest final:public QObject {
    Q_OBJECT
private slots:
    void exactCanonicalBindingsAndNoExtraMass();
    void missingFeatureAuthorityNeverBecomesSolverInput();
    void absentCurrentCandidateValueRemainsNull();
};
void M06NativeBindingTest::exactCanonicalBindingsAndNoExtraMass() {
    const auto input=evidence();
    QVERIFY(input);
    const auto mapped=real::make_real_d03_d04_m07_m08_slots(*input);
    QCOMPARE(mapped.reason,real::D03D04SlotFailure::none);
    QVERIFY(mapped.feature_slots);
    QCOMPARE((*mapped.feature_slots)[0].binding_id,
             std::string{"rgsml.reference-match.binding.dynamics.d03"});
    QCOMPARE((*mapped.feature_slots)[1].source_feature_id,
             std::string{"rgsml.dynamics.crest.local.series"});
    QCOMPARE((*mapped.feature_slots)[0].configured.size(),std::size_t{1});
    QCOMPARE((*mapped.feature_slots)[1].configured.size(),std::size_t{3});
    QVERIFY((*mapped.feature_slots)[0].configured[0].current_valid);
    QCOMPARE((*mapped.feature_slots)[0].configured[0].paired_native_support,1.0);
    for(const auto& c:(*mapped.feature_slots)[1].configured) {
        QVERIFY(c.current_valid);
        QVERIFY(c.current_candidate_z);
        QCOMPARE(c.paired_native_support,1.0);
    }
    QVERIFY(!mapped.authorizes_product_publication());
}
void M06NativeBindingTest::
missingFeatureAuthorityNeverBecomesSolverInput() {
    auto in=evidence();
    QVERIFY(in);
    in->exact_phase6_feature_definition_current=false;
    QCOMPARE(real::make_real_d03_d04_m07_m08_slots(*in).reason,
             real::D03D04SlotFailure::stale_or_unauthenticated_input);
    in=evidence();
    in->exact_feature_dependency.clear();
    QCOMPARE(real::make_real_d03_d04_m07_m08_slots(*in).reason,
             real::D03D04SlotFailure::invalid_feature_dependency);
    in=evidence();
    in->candidate_owner.resource_identity="PREP";
    QCOMPARE(real::make_real_d03_d04_m07_m08_slots(*in).reason,
             real::D03D04SlotFailure::stale_or_unauthenticated_input);
}
void M06NativeBindingTest::absentCurrentCandidateValueRemainsNull() {
    auto in=evidence();
    QVERIFY(in);
    in->actual_candidate.complete_native_windows.clear();
    in->actual_candidate.valid_rms_windows=0;
    in->actual_candidate.valid_crest_windows=0;
    const auto mapped=real::make_real_d03_d04_m07_m08_slots(*in);
    QVERIFY(mapped.feature_slots);
    QVERIFY((*mapped.feature_slots)[0].configured[0].current_valid);
    QVERIFY(!(*mapped.feature_slots)[0].configured[0].current_candidate_z);
    for(const auto& part:(*mapped.feature_slots)[1].configured)
        QVERIFY(!part.current_candidate_z);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M06NativeBindingTest)
#include "test_l3_m06_d03d04_to_m07_m08.moc"
