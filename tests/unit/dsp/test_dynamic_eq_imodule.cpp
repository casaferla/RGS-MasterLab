#include "../render/render_test_support.hpp"
#include <rgsml/dsp/dynamic_eq_module.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
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
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {
using namespace rgsml::dsp;
using namespace render_support;

[[nodiscard]] ModuleDescriptor descriptor() {
    ModuleDescriptorSpec spec{};
    spec.type_id="rgsml.dsp.dynamic-eq";
    spec.display_name_key="rgsml.dsp.dynamic-eq.display-name";
    spec.categories={ModuleCategory::FILTER_EQ};
    spec.allowed_stages={ProcessingStage::RESTORE_PREP,ProcessingStage::MASTER};
    spec.allowed_segments={ChainSegment::MANUAL,ChainSegment::DNA_LINKED,
                           ChainSegment::REF_LINKED};
    spec.duplicable=true;
    spec.bypassable=true;
    spec.placement_class=PlacementClass::INLINE_CHAIN;
    spec.must_be_last=false;
    spec.single_active_instance=false;
    spec.algorithm_version="1.0.0";
    spec.parameter_schema_id="rgsml.dsp.dynamic-eq.parameters/1.0.0";
    auto d=ModuleDescriptor::create(std::move(spec));
    Q_ASSERT(d);
    return *d.value();
}
[[nodiscard]] DynamicEqParameters params(bool other_id=false) {
    DynamicEqBandSpec a;
    a.band_id=*core::Uuid::parse(
        other_id?"33333333-3333-4333-8333-333333333333":
                 "11111111-1111-4111-8111-111111111111").value();
    a.routing=EqRouting::STEREO;
    a.control.range_db=-8.0;
    a.control.threshold_dbfs=-52.0;
    DynamicEqBandSpec b;
    b.band_id=*core::Uuid::parse(
        "22222222-2222-4222-8222-222222222222").value();
    b.filter_type=DynamicEqFilterType::LOW_SHELF;
    b.routing=EqRouting::MID;
    b.shape=DynamicEqShelfShape{360.0,0.7};
    b.control.range_db=6.0;
    b.control.threshold_dbfs=-45.0;
    auto aa=DynamicEqBandParameters::create(a);
    auto bb=DynamicEqBandParameters::create(b);
    Q_ASSERT(aa&&bb);
    auto p=DynamicEqParameters::create({*aa.value(),*bb.value()});
    Q_ASSERT(p);
    return *p.value();
}
[[nodiscard]] std::unique_ptr<DynamicEqModule> make(
    const ModuleDescriptor& d,const DynamicEqParameters& p)
{
    auto m=DynamicEqModule::create(d,p);
    Q_ASSERT(m);
    return std::move(*m.value());
}
[[nodiscard]] std::vector<double> values(std::size_t n,bool right) {
    std::vector<double> v(n);
    for(std::size_t i=0;i<n;++i) {
        const double j=static_cast<double>(i);
        v[i]=right?0.32*std::cos(0.091*j)-0.17*std::sin(0.029*j):
                   0.46*std::sin(0.068*j)+0.19*std::cos(0.023*j);
    }
    return v;
}
[[nodiscard]] bool identical(std::span<const double> a,
                             std::span<const double> b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
        if(std::bit_cast<std::uint64_t>(a[i])!=
           std::bit_cast<std::uint64_t>(b[i]))return false;
    return true;
}
class DynamicEqIModuleTest final:public QObject {
    Q_OBJECT
private slots:
    void checkpointExactContinuationAndIdentity();
    void badCheckpointAndContextAreAtomic();
    void resetEosAndPrecontextGate();
};
void DynamicEqIModuleTest::checkpointExactContinuationAndIdentity() {
    const auto d=descriptor();
    const auto p=params();
    constexpr std::size_t n=127,cut=41;
    auto l=values(n,false),r=values(n,true);
    const DspProcessSpec spec{format(audio::ChannelLayout::STEREO_LR),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,frame_count(127)};
    auto direct=make(d,p),prefix=make(d,p),resumed=make(d,params(true));
    QVERIFY(direct->prepare(spec));
    QVERIFY(prefix->prepare(spec));
    QVERIFY(resumed->prepare(spec));
    const auto unknown=direct->runtime_requirements(spec);
    QVERIFY(!unknown); // no fabricated unconditional finite memory horizon

    auto all_in=make_buffer(spec.audio_format,0,l,r);
    auto all_out=make_buffer(spec.audio_format,0,l,r);
    QVERIFY(all_in&&all_out);
    QVERIFY(direct->process(all_in.value()->view(),all_out.value()->mutable_view(),
        DspProcessContext{frame_range(0,127),true,true}));
    const auto all_l=*all_out.value()->view().channel(0).value();
    const auto all_r=*all_out.value()->view().channel(1).value();

    auto pre_in=make_buffer(spec.audio_format,0,
        std::span<const double>(l).first(cut),
        std::span<const double>(r).first(cut));
    auto pre_out=make_buffer(spec.audio_format,0,
        std::span<const double>(l).first(cut),
        std::span<const double>(r).first(cut));
    QVERIFY(pre_in&&pre_out);
    QVERIFY(prefix->process(pre_in.value()->view(),pre_out.value()->mutable_view(),
        DspProcessContext{frame_range(0,static_cast<std::int64_t>(cut)),true,false}));
    const auto cp=prefix->runtime_checkpoint();
    QVERIFY(cp);
    QCOMPARE(cp.value()->next_input_frame->value(),static_cast<std::int64_t>(cut));
    QCOMPARE(cp.value()->payload.size(),std::size_t{192}); // 8 + 2*11*8
    const auto initial=make(d,params(true));
    QVERIFY(initial->prepare(spec));
    const auto unbound=initial->runtime_checkpoint();
    QVERIFY(unbound);
    QCOMPARE(cp.value()->sonic_fingerprint,unbound.value()->sonic_fingerprint);
    QVERIFY(resumed->restore_runtime_checkpoint(*cp.value()));

    auto suf_in=make_buffer(spec.audio_format,static_cast<std::int64_t>(cut),
        std::span<const double>(l).subspan(cut),
        std::span<const double>(r).subspan(cut));
    auto suf_out=make_buffer(spec.audio_format,static_cast<std::int64_t>(cut),
        std::span<const double>(l).subspan(cut),
        std::span<const double>(r).subspan(cut));
    QVERIFY(suf_in&&suf_out);
    QVERIFY(resumed->process(suf_in.value()->view(),suf_out.value()->mutable_view(),
        DspProcessContext{frame_range(static_cast<std::int64_t>(cut),127),false,true}));
    QVERIFY(identical(*pre_out.value()->view().channel(0).value(),all_l.first(cut)));
    QVERIFY(identical(*pre_out.value()->view().channel(1).value(),all_r.first(cut)));
    QVERIFY(identical(*suf_out.value()->view().channel(0).value(),all_l.subspan(cut)));
    QVERIFY(identical(*suf_out.value()->view().channel(1).value(),all_r.subspan(cut)));
    QVERIFY(!resumed->runtime_checkpoint()); // post-EOS invalid
}
void DynamicEqIModuleTest::badCheckpointAndContextAreAtomic() {
    const auto d=descriptor();
    const auto p=params();
    const DspProcessSpec spec{format(audio::ChannelLayout::STEREO_LR),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,frame_count(96)};
    auto source=make(d,p),dest=make(d,p);
    QVERIFY(source->prepare(spec));
    QVERIFY(dest->prepare(spec));
    const auto l=values(48,false),r=values(48,true);
    auto in=make_buffer(spec.audio_format,0,l,r);
    auto out=make_buffer(spec.audio_format,0,l,r);
    QVERIFY(in&&out);
    QVERIFY(source->process(in.value()->view(),out.value()->mutable_view(),
        DspProcessContext{frame_range(0,48),true,false}));
    auto checkpoint=source->runtime_checkpoint();
    QVERIFY(checkpoint);
    const auto base=dest->runtime_checkpoint();
    QVERIFY(base);
    const auto require_rejection=[&](const DspRuntimeCheckpoint& candidate) {
        QVERIFY(!dest->restore_runtime_checkpoint(candidate));
        auto after=dest->runtime_checkpoint();
        QVERIFY(after);
        QVERIFY(*after.value()==*base.value());
    };
    auto corrupted=*checkpoint.value();
    corrupted.payload[0]^=1U;
    require_rejection(corrupted);
    corrupted=*checkpoint.value();
    corrupted.payload[16]=0xffU;
    corrupted.payload[17]=0xffU;
    corrupted.payload[18]=0xffU;
    corrupted.payload[19]=0xffU;
    corrupted.payload[20]=0xffU;
    corrupted.payload[21]=0xffU;
    corrupted.payload[22]=0xf8U;
    corrupted.payload[23]=0x7fU; // NaN detector state
    require_rejection(corrupted);
    corrupted=*checkpoint.value();
    corrupted.sonic_fingerprint+="invalid";
    require_rejection(corrupted);
    corrupted=*checkpoint.value();
    corrupted.next_input_frame=core::FrameIndex{47};
    require_rejection(corrupted); // carrier/payload epoch mismatch
    QVERIFY(dest->restore_runtime_checkpoint(*checkpoint.value()));
    auto saved=dest->runtime_checkpoint();
    QVERIFY(saved);
    QCOMPARE(saved.value()->next_input_frame->value(),48);
    QVERIFY(!dest->restore_runtime_checkpoint(*base.value()));
    QCOMPARE(*dest->runtime_checkpoint().value(),*saved.value());

    std::vector<double> x(5,0.2),y(5,0.1);
    auto wrong_in=make_buffer(spec.audio_format,47,x,y);
    auto wrong_out=make_buffer(spec.audio_format,47,x,y);
    QVERIFY(wrong_in&&wrong_out);
    QVERIFY(!dest->process(wrong_in.value()->view(),
                           wrong_out.value()->mutable_view(),
                           DspProcessContext{frame_range(48,53),false,false}));
    QCOMPARE(*dest->runtime_checkpoint().value(),*saved.value());

    x[2]=std::numeric_limits<double>::quiet_NaN();
    auto bad_in=make_buffer(spec.audio_format,48,x,y);
    const std::array<double,5> sentinel_l{7,7,7,7,7};
    const std::array<double,5> sentinel_r{8,8,8,8,8};
    auto bad_out=make_buffer(spec.audio_format,48,
        std::span<const double>{sentinel_l},
        std::span<const double>{sentinel_r});
    QVERIFY(bad_in&&bad_out);
    QVERIFY(!dest->process(bad_in.value()->view(),bad_out.value()->mutable_view(),
        DspProcessContext{frame_range(48,53),false,false}));
    QCOMPARE(*dest->runtime_checkpoint().value(),*saved.value());
    for(double v:*bad_out.value()->view().channel(0).value())QCOMPARE(v,7.0);
    for(double v:*bad_out.value()->view().channel(1).value())QCOMPARE(v,8.0);
}
void DynamicEqIModuleTest::resetEosAndPrecontextGate() {
    const auto d=descriptor();
    const auto p=params();
    const DspProcessSpec spec{format(audio::ChannelLayout::STEREO_LR),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,frame_count(12)};
    auto module=make(d,p);
    QVERIFY(module->prepare(spec));
    const auto a=module->runtime_checkpoint();
    QVERIFY(a);
    QVERIFY(!a.value()->next_input_frame);
    std::vector<double> l(4,0.4),r(4,-0.4);
    auto src=make_buffer(spec.audio_format,0,l,r);
    auto dst=make_buffer(spec.audio_format,0,l,r);
    QVERIFY(src&&dst);
    QVERIFY(!module->process(src.value()->view(),dst.value()->mutable_view(),
        DspProcessContext{frame_range(0,4),false,false}));
    QCOMPARE(*module->runtime_checkpoint().value(),*a.value());
    QVERIFY(module->process(src.value()->view(),dst.value()->mutable_view(),
        DspProcessContext{frame_range(0,4),true,true}));
    QVERIFY(!module->process(src.value()->view(),dst.value()->mutable_view(),
        DspProcessContext{frame_range(0,4),true,true}));
    module->reset();
    QVERIFY(*module->runtime_checkpoint().value()==*a.value());
    QVERIFY(module->process(src.value()->view(),dst.value()->mutable_view(),
        DspProcessContext{frame_range(0,4),true,false}));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::DynamicEqIModuleTest)
#include "test_dynamic_eq_imodule.moc"
