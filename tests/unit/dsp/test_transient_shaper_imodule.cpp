#include "../render/render_test_support.hpp"
#include <rgsml/dsp/transient_shaper_module.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/core/error.hpp>
#include <QtTest/QTest>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {
using namespace rgsml::dsp;
using namespace render_support;

[[nodiscard]] std::vector<double> signal(std::size_t n,bool right) {
    std::vector<double> out(n);
    for(std::size_t i=0;i<n;++i) {
        const auto t=static_cast<double>(i);
        out[i]=right?0.31*std::cos(0.09*t)-0.07*std::sin(0.18*t):
                     0.39*std::sin(0.053*t)+0.16*std::cos(0.17*t);
    }
    return out;
}
[[nodiscard]] bool byte_same(std::span<const double> x,
                              std::span<const double> y) {
    if(x.size()!=y.size())return false;
    for(std::size_t i=0;i<x.size();++i)
        if(std::bit_cast<std::uint64_t>(x[i])!=
           std::bit_cast<std::uint64_t>(y[i]))return false;
    return true;
}
[[nodiscard]] std::unique_ptr<TransientShaperModule> make(
    const ModuleDescriptor& d,const TransientShaperParameters& p) {
    auto value=TransientShaperModule::create(d,p);
    Q_ASSERT(value);
    return std::move(*value.value());
}
[[nodiscard]] bool has_category(
    const core::Error& e,const std::string& category) {
    for(const auto& detail:e.details())
        if(detail.key=="category"&&detail.value==category)return true;
    return false;
}
class TransientShaperIModuleTest final:public QObject {
    Q_OBJECT
private slots:
    void fullPrefixCheckpointAndEffectiveContext();
    void corruptStateAndBadTimelineRollback();
    void resetNeutralAndFinalizedCheckpointGate();
};
void TransientShaperIModuleTest::fullPrefixCheckpointAndEffectiveContext() {
    const auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto d=registry.value()->find_descriptor("rgsml.dsp.transient-shaper");
    QVERIFY(d);
    const auto& descriptor=d.value()->get();
    const auto params=TransientShaperParameters::create(6.0,-4.0);
    QVERIFY(params);
    const DspProcessSpec spec{
        format(audio::ChannelLayout::STEREO_LR),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,frame_count(127)};
    auto direct=make(descriptor,*params.value());
    auto prefix=make(descriptor,*params.value());
    auto resumed=make(descriptor,*params.value());
    const auto needs=direct->runtime_requirements(spec);
    QVERIFY(needs);
    QVERIFY(needs.value()->execution_model==DspExecutionModel::STREAMING_CAUSAL);
    QCOMPARE(needs.value()->pre_context_frames.value(),std::int64_t{5397190});
    QCOMPARE(needs.value()->algorithmic_latency_frames.value(),std::int64_t{0});
    QCOMPARE(needs.value()->look_ahead_frames.value(),std::int64_t{0});
    QCOMPARE(needs.value()->effective_tail_frames.value(),std::int64_t{0});
    QVERIFY(!needs.value()->requires_prepass);
    QVERIFY(direct->prepare(spec));
    QVERIFY(prefix->prepare(spec));
    QVERIFY(resumed->prepare(spec));
    constexpr std::size_t n=127,cut=41;
    const auto left=signal(n,false),right=signal(n,true);
    auto full_input=make_buffer(spec.audio_format,0,left,right);
    auto full_output=make_buffer(spec.audio_format,0,left,right);
    QVERIFY(full_input&&full_output);
    QVERIFY(direct->process(
        full_input.value()->view(),full_output.value()->mutable_view(),
        DspProcessContext{frame_range(0,127),true,true}));
    const auto all_left=*full_output.value()->view().channel(0).value();
    const auto all_right=*full_output.value()->view().channel(1).value();

    auto p_input=make_buffer(spec.audio_format,0,
        std::span<const double>(left).first(cut),
        std::span<const double>(right).first(cut));
    auto p_output=make_buffer(spec.audio_format,0,
        std::span<const double>(left).first(cut),
        std::span<const double>(right).first(cut));
    QVERIFY(p_input&&p_output);
    QVERIFY(prefix->process(
        p_input.value()->view(),p_output.value()->mutable_view(),
        DspProcessContext{frame_range(0,static_cast<std::int64_t>(cut)),
                          true,false}));
    const auto cp=prefix->runtime_checkpoint();
    QVERIFY(cp);
    QCOMPARE(cp.value()->payload.size(),std::size_t{48});
    QCOMPARE(cp.value()->next_input_frame->value(),
             static_cast<std::int64_t>(cut));
    QVERIFY(resumed->restore_runtime_checkpoint(*cp.value()));
    auto s_input=make_buffer(spec.audio_format,static_cast<std::int64_t>(cut),
        std::span<const double>(left).subspan(cut),
        std::span<const double>(right).subspan(cut));
    auto s_output=make_buffer(spec.audio_format,static_cast<std::int64_t>(cut),
        std::span<const double>(left).subspan(cut),
        std::span<const double>(right).subspan(cut));
    QVERIFY(s_input&&s_output);
    QVERIFY(resumed->process(
        s_input.value()->view(),s_output.value()->mutable_view(),
        DspProcessContext{
            frame_range(static_cast<std::int64_t>(cut),127),false,true}));
    QVERIFY(byte_same(
        *p_output.value()->view().channel(0).value(),all_left.first(cut)));
    QVERIFY(byte_same(
        *p_output.value()->view().channel(1).value(),all_right.first(cut)));
    QVERIFY(byte_same(
        *s_output.value()->view().channel(0).value(),all_left.subspan(cut)));
    QVERIFY(byte_same(
        *s_output.value()->view().channel(1).value(),all_right.subspan(cut)));
    const auto after_eos=resumed->runtime_checkpoint();
    QVERIFY(!after_eos);
    QVERIFY(after_eos.error()->code()==core::ErrorCode::UnsupportedOperation);
    QVERIFY(has_category(*after_eos.error(),
        "RUNTIME_CHECKPOINT_DURING_FINALIZE_UNSUPPORTED"));
}
void TransientShaperIModuleTest::corruptStateAndBadTimelineRollback() {
    const auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto descriptor=registry.value()->find_descriptor(
        "rgsml.dsp.transient-shaper");
    QVERIFY(descriptor);
    const auto params=TransientShaperParameters::create(7.0,3.0);
    QVERIFY(params);
    const DspProcessSpec spec{
        format(audio::ChannelLayout::STEREO_LR),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,frame_count(96)};
    auto origin=make(descriptor.value()->get(),*params.value());
    auto dest=make(descriptor.value()->get(),*params.value());
    QVERIFY(origin->prepare(spec));
    QVERIFY(dest->prepare(spec));
    auto l=signal(48,false),r=signal(48,true);
    auto source=make_buffer(spec.audio_format,0,l,r);
    auto target=make_buffer(spec.audio_format,0,l,r);
    QVERIFY(source&&target);
    QVERIFY(origin->process(source.value()->view(),target.value()->mutable_view(),
        DspProcessContext{frame_range(0,48),true,false}));
    auto good=origin->runtime_checkpoint();
    auto initial=dest->runtime_checkpoint();
    QVERIFY(good&&initial);
    const auto assert_unchanged=[&](const DspRuntimeCheckpoint& bad) {
        QVERIFY(!dest->restore_runtime_checkpoint(bad));
        const auto after=dest->runtime_checkpoint();
        QVERIFY(after);
        QVERIFY(*after.value()==*initial.value());
    };
    auto bad=*good.value();
    bad.payload[0]^=1U;
    assert_unchanged(bad);
    bad=*good.value();
    for(unsigned i=8;i<16;++i)
        bad.payload[i]=(i==14)?0xf8U:(i==15)?0x7fU:0xffU;
    assert_unchanged(bad); // nonfinite f state must never commit
    bad=*good.value();bad.sonic_fingerprint+="wrong";
    assert_unchanged(bad);
    bad=*good.value();bad.next_input_frame=core::FrameIndex{47};
    assert_unchanged(bad);
    QVERIFY(dest->restore_runtime_checkpoint(*good.value()));
    const auto restored=dest->runtime_checkpoint();
    QVERIFY(restored);
    QVERIFY(*restored.value()==*good.value());

    auto wrongInput=make_buffer(spec.audio_format,47,
        std::span<const double>(l).first(5),
        std::span<const double>(r).first(5));
    auto wrongOutput=make_buffer(spec.audio_format,47,
        std::span<const double>(l).first(5),
        std::span<const double>(r).first(5));
    QVERIFY(wrongInput&&wrongOutput);
    QVERIFY(!dest->process(
        wrongInput.value()->view(),wrongOutput.value()->mutable_view(),
        DspProcessContext{frame_range(47,52),false,false}));
    QVERIFY(*dest->runtime_checkpoint().value()==*restored.value());
    std::vector<double> invalidL(5,0.25),validR(5,0.12);
    invalidL[4]=std::numeric_limits<double>::quiet_NaN();
    const std::array<double,5> unchangedL{7,7,7,7,7};
    const std::array<double,5> unchangedR{8,8,8,8,8};
    auto badInput=make_buffer(spec.audio_format,48,invalidL,validR);
    auto badOutput=make_buffer(spec.audio_format,48,
        std::span<const double>{unchangedL},
        std::span<const double>{unchangedR});
    QVERIFY(badInput&&badOutput);
    QVERIFY(!dest->process(badInput.value()->view(),
        badOutput.value()->mutable_view(),
        DspProcessContext{frame_range(48,53),false,false}));
    QVERIFY(*dest->runtime_checkpoint().value()==*restored.value());
    for(const double v:*badOutput.value()->view().channel(0).value())
        QCOMPARE(v,7.0);
    for(const double v:*badOutput.value()->view().channel(1).value())
        QCOMPARE(v,8.0);
}
void TransientShaperIModuleTest::resetNeutralAndFinalizedCheckpointGate() {
    const auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto desc=registry.value()->find_descriptor(
        "rgsml.dsp.transient-shaper");
    QVERIFY(desc);
    const auto p=TransientShaperParameters::create(0.0,-0.0);
    QVERIFY(p);
    const DspProcessSpec spec{
        format(audio::ChannelLayout::MONO_C),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,frame_count(64)};
    auto module=make(desc.value()->get(),*p.value());
    QVERIFY(module->prepare(spec));
    const auto original=module->runtime_checkpoint();
    QVERIFY(original);
    QVERIFY(!original.value()->next_input_frame);
    QCOMPARE(original.value()->payload.size(),std::size_t{48});
    const std::vector<double> values{
        -0.0,0.1,std::numeric_limits<double>::denorm_min(),-0.0};
    auto src=make_buffer(spec.audio_format,0,values,{});
    auto dst=make_buffer(spec.audio_format,0,values,{});
    QVERIFY(src&&dst);
    QVERIFY(!module->process(src.value()->view(),dst.value()->mutable_view(),
        DspProcessContext{frame_range(0,4),false,false}));
    QVERIFY(*module->runtime_checkpoint().value()==*original.value());
    QVERIFY(module->process(src.value()->view(),dst.value()->mutable_view(),
        DspProcessContext{frame_range(0,4),true,true}));
    QVERIFY(byte_same(*src.value()->view().channel(0).value(),
                      *dst.value()->view().channel(0).value()));
    QVERIFY(!module->runtime_checkpoint());
    QVERIFY(!module->process(src.value()->view(),dst.value()->mutable_view(),
        DspProcessContext{frame_range(0,4),true,true}));
    module->reset();
    const auto fresh=module->runtime_checkpoint();
    QVERIFY(fresh);
    QVERIFY(*fresh.value()==*original.value());
    QVERIFY(module->process(src.value()->view(),dst.value()->mutable_view(),
        DspProcessContext{frame_range(0,4),true,false}));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::TransientShaperIModuleTest)
#include "test_transient_shaper_imodule.moc"
