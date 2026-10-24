#include "stereo_ms_independent_fixture.hpp"

#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/stereo_ms_module.hpp>
#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>

namespace rgsml::tests {
namespace {

using namespace rgsml::dsp;
using namespace render_support;
using namespace stereo_ms_fixture;

// An independent textbook normalized Direct Form I oracle. Production uses
// four separate TDF-II cascades. This oracle computes its own coefficients,
// does not call the production coefficient designer or recursive kernel.
struct Section final {
    double b0, b1, b2, a1, a2;
};
struct History final {
    double x1{}, x2{}, y1{}, y2{};
};
using Cascade = std::array<History, 2>;

[[nodiscard]] double dfi(double x, Section c, History& s) noexcept
{
    const double y = c.b0*x + c.b1*s.x1 + c.b2*s.x2
                   - c.a1*s.y1 - c.a2*s.y2;
    s.x2=s.x1; s.x1=x; s.y2=s.y1; s.y1=y;
    return y;
}
[[nodiscard]] double run(double x, Section c, Cascade& memory) noexcept
{
    for (auto& state : memory) x=dfi(x,c,state);
    return x;
}

class IndependentReference final {
public:
    explicit IndependentReference(MonoBassMode mode) : mode_(mode)
    {
        const double k=std::tan(std::numbers::pi_v<double>*kCutoff/kRate);
        if (mode==MonoBassMode::LR12) {
            const double b=k/(1.0+k), h=1.0/(1.0+k);
            const double a=-(1.0-k)/(1.0+k);
            low_={b,b,0.0,a,0.0};
            high_={h,-h,0.0,a,0.0};
        } else {
            const double kk=k*k;
            const double d=1.0+std::numbers::sqrt2_v<double>*k+kk;
            const double l=kk/d, h=1.0/d;
            const double a=2.0*(kk-1.0)/d;
            const double a2=(1.0-std::numbers::sqrt2_v<double>*k+kk)/d;
            low_={l,2.0*l,l,a,a2};
            high_={h,-2.0*h,h,a,a2};
        }
    }

    [[nodiscard]] std::array<double,2> sample(double l,double r,double beta)
    {
        const double m=(l+r)/std::numbers::sqrt2_v<double>
            *std::pow(10.0,-1.0/20.0);
        const double s=(l-r)/std::numbers::sqrt2_v<double>
            *std::pow(10.0,2.0/20.0);
        const double polarity=mode_==MonoBassMode::LR12 ? -1.0 : 1.0;
        const double mo=run(m,low_,hist_[0])
            +polarity*run(m,high_,hist_[1]);
        const double so=beta*run(s,low_,hist_[2])
            +polarity*run(s,high_,hist_[3]);
        return {(mo+so)/std::numbers::sqrt2_v<double>,
                (mo-so)/std::numbers::sqrt2_v<double>};
    }
private:
    MonoBassMode mode_;
    Section low_{};
    Section high_{};
    std::array<Cascade,4> hist_{};
};

class StereoMsIndependentMathTest final : public QObject {
    Q_OBJECT
private slots:
    void frozenFixtureHash();
    void longSequenceAgainstIndependentDFI();
};

void StereoMsIndependentMathTest::frozenFixtureHash()
{
    const auto left=samples(UINT32_C(0x4D313500));
    QCOMPARE(binary64_le_sha256(left),
        QByteArrayLiteral("b7614063d407b0b555c63b2168d7fba614a009e2c5da08aa6134bd675b467772"));
}

void StereoMsIndependentMathTest::longSequenceAgainstIndependentDFI()
{
    auto registry=ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    const auto left=samples(UINT32_C(0x4D313500));
    const auto right=samples(UINT32_C(0x4D313501));
    const auto layout=rgsml::audio::ChannelLayout::STEREO_LR;
    const DspProcessSpec spec{
        format(layout),rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(static_cast<std::int64_t>(kFrames))};
    auto input=make_buffer(layout,0,left,right);
    QVERIFY(input);
    const auto desc=registry.value()->find_descriptor("rgsml.dsp.stereo-ms");
    QVERIFY(desc);
    for (const auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        for (const double beta : {0.0,0.25,0.5,0.75,1.0}) {
            auto p=StereoMsParameters::create(-1.0,2.0,false,
                                               mode,120.0,beta*100.0);
            QVERIFY(p);
            auto created=StereoMsModule::create(desc.value()->get(),*p.value());
            QVERIFY(created);
            auto& module=*created.value();
            QVERIFY(module->prepare(spec));
            auto output=make_buffer(layout,0,left,right);
            QVERIFY(output);
            QVERIFY(module->process(input.value()->view(),
                output.value()->mutable_view(),
                DspProcessContext{frame_range(0,static_cast<std::int64_t>(kFrames)),
                                  true,true}));
            const auto lout=*output.value()->view().channel(0).value();
            const auto rout=*output.value()->view().channel(1).value();
            IndependentReference reference{mode};
            double maxError=0.0;
            for (std::size_t n=0;n<kFrames;++n) {
                const auto expected=reference.sample(left[n],right[n],beta);
                QVERIFY(std::isfinite(lout[n]));
                QVERIFY(std::isfinite(rout[n]));
                maxError=std::max(maxError,std::abs(lout[n]-expected[0]));
                maxError=std::max(maxError,std::abs(rout[n]-expected[1]));
            }
            // DFI and production TDF-II differ in evaluation order: compare
            // with a numerical bound, never a false bit-exact assertion.
            QVERIFY2(maxError<1e-8,
                     "Production Stereo/M-S diverges from independent DFI.");
        }
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::StereoMsIndependentMathTest)
#include "test_stereo_ms_independent_math.moc"
