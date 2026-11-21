#include "../render/render_test_support.hpp"
#include <rgsml/dsp/dynamic_eq_module.hpp>
#include <rgsml/dsp/parametric_eq_module.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <QtTest/QTest>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>
#include <vector>
#include <utility>

namespace rgsml::tests {
namespace {
using namespace rgsml::dsp;
using namespace render_support;

[[nodiscard]] ModuleDescriptor dynamic_descriptor() {
    ModuleDescriptorSpec s{};
    s.type_id="rgsml.dsp.dynamic-eq";
    s.display_name_key="rgsml.dsp.dynamic-eq.display-name";
    s.categories={ModuleCategory::FILTER_EQ};
    s.allowed_stages={ProcessingStage::MASTER};
    s.allowed_segments={ChainSegment::MANUAL};
    s.duplicable=true;
    s.bypassable=true;
    s.placement_class=PlacementClass::INLINE_CHAIN;
    s.algorithm_version="1.0.0";
    s.parameter_schema_id="rgsml.dsp.dynamic-eq.parameters/1.0.0";
    auto d=ModuleDescriptor::create(std::move(s));
    Q_ASSERT(d);
    return *d.value();
}
[[nodiscard]] core::Uuid identity(int n) {
    return *core::Uuid::parse(
        n==1?"11111111-1111-4111-8111-111111111111":
             "22222222-2222-4222-8222-222222222222").value();
}
[[nodiscard]] EqFilterType static_type(DynamicEqFilterType x) {
    switch(x) {
    case DynamicEqFilterType::BELL:return EqFilterType::BELL;
    case DynamicEqFilterType::LOW_SHELF:return EqFilterType::LOW_SHELF;
    case DynamicEqFilterType::HIGH_SHELF:return EqFilterType::HIGH_SHELF;
    }
    return EqFilterType::BELL; // unreachable for validated enum
}
[[nodiscard]] bool same_bits(std::span<const double> a,
                             std::span<const double> b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
        if(std::bit_cast<std::uint64_t>(a[i])!=
           std::bit_cast<std::uint64_t>(b[i]))return false;
    return true;
}
[[nodiscard]] DynamicEqBandSpec spec(
    int idx,DynamicEqFilterType type,EqRouting routing,
    bool ratio_one,double base_gain)
{
    DynamicEqBandSpec s;
    s.band_id=identity(idx);
    s.filter_type=type;
    s.routing=routing;
    s.shape=type==DynamicEqFilterType::BELL
        ?DynamicEqShape{DynamicEqBellShape{1100.0,0.707}}
        :DynamicEqShape{DynamicEqShelfShape{1100.0,0.8}};
    s.control.base_gain_db=base_gain;
    s.control.range_db=ratio_one?-4.0:0.0;
    s.control.ratio=ratio_one?1.0:2.0;
    s.control.threshold_dbfs=-60.0;
    return s;
}
[[nodiscard]] EqBandParameters static_band(const DynamicEqBandSpec& s) {
    const auto type=static_type(s.filter_type);
    const double gain=s.control.base_gain_db;
    if(type==EqFilterType::BELL) {
        const auto b=std::get<DynamicEqBellShape>(s.shape);
        return *EqBandParameters::create(s.band_id,s.enabled,type,s.routing,
            BellPayload{b.frequency_hz,gain,b.q}).value();
    }
    const auto sh=std::get<DynamicEqShelfShape>(s.shape);
    return *EqBandParameters::create(s.band_id,s.enabled,type,s.routing,
        ShelfPayload{sh.frequency_hz,gain,sh.shelf_slope}).value();
}
[[nodiscard]] bool run_case(
    audio::ChannelLayout layout,std::int64_t fs,
    const std::vector<DynamicEqBandSpec>& specs)
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    if(!registry)return false;
    const auto pd=registry.value()->find_descriptor("rgsml.dsp.parametric-eq");
    if(!pd)return false;
    const auto dd=dynamic_descriptor();
    std::vector<DynamicEqBandParameters> bands;
    std::vector<EqBandParameters> eq_bands;
    for(const auto& s:specs) {
        auto b=DynamicEqBandParameters::create(s);
        if(!b)return false;
        bands.push_back(*b.value());
        eq_bands.push_back(static_band(s));
    }
    auto params=DynamicEqParameters::create(std::move(bands));
    auto eq_params=ParametricEqParameters::create(std::move(eq_bands));
    if(!params||!eq_params)return false;
    auto mod=DynamicEqModule::create(dd,*params.value());
    auto static_mod=ParametricEqModule::create(pd.value()->get(),*eq_params.value());
    if(!mod||!static_mod)return false;
    constexpr std::size_t n=1901;
    const auto f=format(layout,fs);
    const DspProcessSpec ps{f,audio::FrameDomainId::SOURCE_PROCESSING_RATE,
                            frame_count(static_cast<std::int64_t>(n))};
    if(!mod.value()->get()->prepare(ps)||
       !static_mod.value()->get()->prepare(ps))return false;
    std::vector<double> l(n),r(n);
    for(std::size_t i=0;i<n;++i) {
        const double t=static_cast<double>(i);
        l[i]=0.27*std::sin(0.031*t)+0.18*std::cos(0.129*t);
        r[i]=0.22*std::cos(0.023*t)-0.17*std::sin(0.073*t);
    }
    const std::span<const double> right=layout==audio::ChannelLayout::STEREO_LR
        ?std::span<const double>{r}:std::span<const double>{};
    auto input=make_buffer(f,0,l,right);
    auto out_static=make_buffer(f,0,l,right);
    auto out_dyn=make_buffer(f,0,l,right);
    if(!input||!out_static||!out_dyn)return false;
    const DspProcessContext context{
        frame_range(0,static_cast<std::int64_t>(n)),true,true};
    if(!static_mod.value()->get()->process(
           input.value()->view(),out_static.value()->mutable_view(),context) ||
       !mod.value()->get()->process(
           input.value()->view(),out_dyn.value()->mutable_view(),context))
        return false;
    const auto a=out_static.value()->view().channel(0);
    const auto b=out_dyn.value()->view().channel(0);
    if(!a||!b||!same_bits(*a.value(),*b.value()))return false;
    if(layout==audio::ChannelLayout::STEREO_LR) {
        const auto c=out_static.value()->view().channel(1);
        const auto d=out_dyn.value()->view().channel(1);
        if(!c||!d||!same_bits(*c.value(),*d.value()))return false;
    }
    return true;
}
class DynamicEqStaticEquivalenceTest final : public QObject {
    Q_OBJECT
private slots:
    void rangeZeroAllFamiliesAndRoutes();
    void ratioOneAllFamiliesAndRoutes();
    void twoBandPersistedOrder();
};
void DynamicEqStaticEquivalenceTest::rangeZeroAllFamiliesAndRoutes() {
    for(const auto fs:{44100,48000,96000}) {
        for(const auto type:{DynamicEqFilterType::BELL,
                             DynamicEqFilterType::LOW_SHELF,
                             DynamicEqFilterType::HIGH_SHELF}) {
            for(const auto route:{EqRouting::STEREO,EqRouting::LEFT,
                                  EqRouting::RIGHT,EqRouting::MID,EqRouting::SIDE}) {
                const auto s=spec(1,type,route,false,6.0);
                QVERIFY(run_case(audio::ChannelLayout::STEREO_LR,fs,{s}));
                if(route==EqRouting::STEREO)
                    QVERIFY(run_case(audio::ChannelLayout::MONO_C,fs,{s}));
            }
        }
    }
}
void DynamicEqStaticEquivalenceTest::ratioOneAllFamiliesAndRoutes() {
    for(const auto type:{DynamicEqFilterType::BELL,
                         DynamicEqFilterType::LOW_SHELF,
                         DynamicEqFilterType::HIGH_SHELF}) {
        for(const auto route:{EqRouting::STEREO,EqRouting::MID,EqRouting::SIDE}) {
            QVERIFY(run_case(audio::ChannelLayout::STEREO_LR,48000,
                {spec(1,type,route,true,4.0)}));
        }
    }
}
void DynamicEqStaticEquivalenceTest::twoBandPersistedOrder() {
    for(const bool ratio_one:{false,true}) {
        QVERIFY(run_case(audio::ChannelLayout::STEREO_LR,48000,
            {spec(1,DynamicEqFilterType::BELL,EqRouting::MID,ratio_one,6.0),
             spec(2,DynamicEqFilterType::LOW_SHELF,EqRouting::LEFT,ratio_one,-8.0)}));
        QVERIFY(run_case(audio::ChannelLayout::STEREO_LR,48000,
            {spec(2,DynamicEqFilterType::LOW_SHELF,EqRouting::LEFT,ratio_one,-8.0),
             spec(1,DynamicEqFilterType::BELL,EqRouting::MID,ratio_one,6.0)}));
    }
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::DynamicEqStaticEquivalenceTest)
#include "test_dynamic_eq_static_equivalence.moc"
