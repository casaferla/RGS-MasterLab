#include "internal/transient_shaper_c00_engine.hpp"
#include "internal/transient_shaper_effective_memory.hpp"
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <rgsml/core/error.hpp>
#include <QtTest/QTest>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

namespace rgsml::tests {
namespace {
using namespace rgsml;
using dsp::TransientShaperParameters;
using dsp::internal::TransientShaperC00Engine;
using dsp::internal::TransientShaperC00State;
[[nodiscard]] std::vector<double> programme(std::size_t n) {
    std::vector<double> out(n);
    for(std::size_t i=0;i<n;++i) {
        const auto x=static_cast<double>(i);
        out[i]=0.28*std::sin(2.0*std::numbers::pi*220.0*x/48000.0)
              +0.11*std::sin(2.0*std::numbers::pi*3451.0*x/48000.0);
    }
    return out;
}
[[nodiscard]] std::vector<double> independent_oracle(
    const std::vector<double>& in,double attack,double sustain) {
    constexpr double fs=48000.0;
    const double af=std::exp(-1.0/(0.030*fs));
    const double am=std::exp(-1.0/(0.010*fs));
    const double al=std::exp(-1.0/(0.150*fs));
    const double ag=std::exp(-1.0/(0.00025*fs));
    double f=0.0,m=0.0,l=0.0,g_attack=0.0,g_sustain=0.0;
    std::vector<double> out;
    out.reserve(in.size());
    for(const auto x:in) {
        const double peak=std::abs(x);
        const double updated_f=peak>=f?peak:af*f+(1.0-af)*peak;
        const double updated_m=am*m+(1.0-am)*updated_f;
        const double updated_l=peak>=l?peak:al*l+(1.0-al)*peak;
        const double rise=std::max(updated_f-updated_m,0.0)/
                          (updated_f+1e-9);
        const double decay=std::max(updated_l-updated_f,0.0)/
                           (updated_l+1e-9);
        g_attack=ag*g_attack+(1.0-ag)*(attack*rise);
        g_sustain=ag*g_sustain+(1.0-ag)*(sustain*decay);
        f=updated_f;m=updated_m;l=updated_l;
        const auto linear=std::pow(10.0,(g_attack+g_sustain)/20.0);
        out.push_back(x*linear);
    }
    return out;
}

class TransientShaperC00Test final:public QObject {
    Q_OBJECT
private slots:
    void parametersAndRates();
    void exactNeutralSignedZero();
    void frozenImpulseAndIndependentOracle();
    void chunkingResetAndStateRestore();
    void stereoLinkAndTransactionalFailures();
};
void TransientShaperC00Test::parametersAndRates() {
    auto neutral=TransientShaperParameters::create_default();
    QVERIFY(neutral);
    QVERIFY(neutral.value()->neutral());
    QVERIFY(TransientShaperParameters::create(-12.0,12.0));
    QVERIFY(TransientShaperParameters::create(12.0,-12.0));
    QVERIFY(!TransientShaperParameters::create(12.001,0.0));
    QVERIFY(!TransientShaperParameters::create(0.0,-12.001));
    QVERIFY(!TransientShaperParameters::create(
        std::numeric_limits<double>::quiet_NaN(),0.0));
    QVERIFY(!TransientShaperParameters::create(
        0.0,std::numeric_limits<double>::infinity()));
    for(const auto fs:{44100U,48000U,96000U,192000U})
        for(const auto channels:{1U,2U}) {
            const auto e=TransientShaperC00Engine::create(
                *neutral.value(),fs,channels,4096);
            QVERIFY(e);
            QCOMPARE(e.value()->sample_rate_hz(),fs);
            QCOMPARE(e.value()->channels(),channels);
            QCOMPARE(e.value()->maximum_block_frames(),std::size_t{4096});
        }
    QVERIFY(!TransientShaperC00Engine::create(*neutral.value(),22050,1,64));
    QVERIFY(!TransientShaperC00Engine::create(*neutral.value(),48000,3,64));
    QVERIFY(!TransientShaperC00Engine::create(*neutral.value(),48000,1,0));

    // Accepted P07 metadata only: no 112-second buffer allocation, no
    // exact mid-track cropped-prefix claim.
    const auto h48=dsp::internal::
        transient_shaper_conservative_precontext_frames(48000);
    QVERIFY(h48);
    QCOMPARE(h48.value()->value(),std::int64_t{5397190});
    const auto h44=dsp::internal::
        transient_shaper_conservative_precontext_frames(44100);
    const auto h96=dsp::internal::
        transient_shaper_conservative_precontext_frames(96000);
    const auto h192=dsp::internal::
        transient_shaper_conservative_precontext_frames(192000);
    QVERIFY(h44&&h96&&h192);
    QVERIFY(h44.value()->value()<h48.value()->value());
    QVERIFY(h48.value()->value()<h96.value()->value());
    QVERIFY(h96.value()->value()<h192.value()->value());
    QVERIFY(!dsp::internal::
        transient_shaper_conservative_precontext_frames(88200));
}
void TransientShaperC00Test::exactNeutralSignedZero() {
    auto params=TransientShaperParameters::create_default();
    QVERIFY(params);
    auto e=TransientShaperC00Engine::create(*params.value(),48000,1,64);
    QVERIFY(e);
    const std::vector<double> in{
        0.0,-0.0,std::numeric_limits<double>::denorm_min(),
        -std::numeric_limits<double>::denorm_min(),0.3,-0.0,0.0};
    std::vector<double> out(in.size(),42.0);
    QVERIFY(e.value()->process_block(in,{},out,{}));
    for(std::size_t i=0;i<in.size();++i)
        QCOMPARE(std::bit_cast<std::uint64_t>(out[i]),
                 std::bit_cast<std::uint64_t>(in[i]));
    QVERIFY(e.value()->state().f>0.0);
    QVERIFY(e.value()->state().m>0.0);
    QVERIFY(e.value()->state().l>0.0);
    QCOMPARE(e.value()->state().g_attack_db,0.0);
    QCOMPARE(e.value()->state().g_sustain_db,0.0);
    e.value()->reset();
    QVERIFY(e.value()->state()==TransientShaperC00State{});
    std::fill(out.begin(),out.end(),0.0);
    QVERIFY(e.value()->process_block(in,{},out,{}));
    for(std::size_t i=0;i<in.size();++i)
        QCOMPARE(std::bit_cast<std::uint64_t>(out[i]),
                 std::bit_cast<std::uint64_t>(in[i]));
}
void TransientShaperC00Test::frozenImpulseAndIndependentOracle() {
    const auto attack=TransientShaperParameters::create(6.0,0.0);
    QVERIFY(attack);
    const std::array<double,4> expected{{0.5191,0.4787,0.2446,0.1236}};
    for(std::size_t i=0;i<4;++i) {
        constexpr std::array<std::uint32_t,4> rates{{
            44100U,48000U,96000U,192000U}};
        auto e=TransientShaperC00Engine::create(
            *attack.value(),rates[i],1,1);
        QVERIFY(e);
        const std::array<double,1> impulse{{1.0}};
        std::array<double,1> out{{0.0}};
        QVERIFY(e.value()->process_block(impulse,{},out,{}));
        const double observed=20.0*std::log10(out[0]);
        QVERIFY(std::abs(observed-expected[i])<0.005);
        QVERIFY(out[0]>1.0);
    }
    const auto p=TransientShaperParameters::create(8.0,-5.0);
    QVERIFY(p);
    auto e=TransientShaperC00Engine::create(*p.value(),48000,1,4096);
    QVERIFY(e);
    const auto in=programme(3000);
    std::vector<double> out(in.size(),0.0);
    QVERIFY(e.value()->process_block(in,{},out,{}));
    const auto ref=independent_oracle(in,8.0,-5.0);
    QCOMPARE(out.size(),ref.size());
    for(std::size_t i=0;i<out.size();++i)
        QVERIFY(std::abs(out[i]-ref[i])<1e-10);
    QVERIFY(std::abs(e.value()->state().g_attack_db)<=8.0+1e-10);
    QVERIFY(std::abs(e.value()->state().g_sustain_db)<=5.0+1e-10);
}
void TransientShaperC00Test::chunkingResetAndStateRestore() {
    const auto p=TransientShaperParameters::create(6.0,4.0);
    QVERIFY(p);
    const auto in=programme(4096);
    auto single=TransientShaperC00Engine::create(*p.value(),48000,1,4096);
    auto pieces=TransientShaperC00Engine::create(*p.value(),48000,1,4096);
    QVERIFY(single&&pieces);
    std::vector<double> full(in.size()),ragged(in.size());
    QVERIFY(single.value()->process_block(in,{},full,{}));
    constexpr std::array<std::size_t,11> sizes{{
        1,2,3,7,31,64,127,256,511,1024,4096}};
    std::size_t pos=0,step=0;
    while(pos<in.size()) {
        const auto count=std::min(sizes[step++%sizes.size()],in.size()-pos);
        const auto source=std::span<const double>(in.data()+pos,count);
        const auto target=std::span<double>(ragged.data()+pos,count);
        QVERIFY(pieces.value()->process_block(source,{},target,{}));
        pos+=count;
    }
    for(std::size_t i=0;i<full.size();++i)
        QCOMPARE(std::bit_cast<std::uint64_t>(full[i]),
                 std::bit_cast<std::uint64_t>(ragged[i]));
    QVERIFY(single.value()->state()==pieces.value()->state());

    auto first=TransientShaperC00Engine::create(*p.value(),48000,1,4096);
    auto restored=TransientShaperC00Engine::create(*p.value(),48000,1,4096);
    QVERIFY(first&&restored);
    std::vector<double> prefix(1733);
    QVERIFY(first.value()->process_block(
        std::span<const double>(in.data(),prefix.size()),{},prefix,{}));
    const auto state=first.value()->state();
    QVERIFY(restored.value()->restore_state(state));
    std::vector<double> a(4096-1733),b(4096-1733);
    const auto suffix=std::span<const double>(
        in.data()+1733,in.size()-1733);
    QVERIFY(first.value()->process_block(suffix,{},a,{}));
    QVERIFY(restored.value()->process_block(suffix,{},b,{}));
    for(std::size_t i=0;i<a.size();++i)
        QCOMPARE(std::bit_cast<std::uint64_t>(a[i]),
                 std::bit_cast<std::uint64_t>(b[i]));
    auto corrupted=state;
    corrupted.f=-1.0;
    const auto before=restored.value()->state();
    QVERIFY(!restored.value()->restore_state(corrupted));
    QVERIFY(restored.value()->state()==before);

    auto aliased=TransientShaperC00Engine::create(
        *p.value(),48000,1,4096);
    QVERIFY(aliased);
    auto inPlace=in;
    QVERIFY(aliased.value()->process_block(inPlace,{},inPlace,{}));
    for(std::size_t i=0;i<full.size();++i)
        QCOMPARE(std::bit_cast<std::uint64_t>(full[i]),
                 std::bit_cast<std::uint64_t>(inPlace[i]));
}
void TransientShaperC00Test::stereoLinkAndTransactionalFailures() {
    const auto p=TransientShaperParameters::create(6.0,-4.0);
    QVERIFY(p);
    auto stereo=TransientShaperC00Engine::create(
        *p.value(),48000,2,1024);
    auto swapped=TransientShaperC00Engine::create(
        *p.value(),48000,2,1024);
    QVERIFY(stereo&&swapped);
    const auto left=programme(1024);
    std::vector<double> right(left.size());
    for(std::size_t i=0;i<left.size();++i)right[i]=-left[i];
    std::vector<double> a(left.size()),b(left.size()),
                        c(left.size()),d(left.size());
    QVERIFY(stereo.value()->process_block(left,right,a,b));
    QVERIFY(swapped.value()->process_block(right,left,c,d));
    QVERIFY(stereo.value()->state()==swapped.value()->state());
    for(std::size_t i=0;i<a.size();++i) {
        QCOMPARE(std::bit_cast<std::uint64_t>(a[i]),
                 std::bit_cast<std::uint64_t>(d[i]));
        QCOMPARE(std::bit_cast<std::uint64_t>(b[i]),
                 std::bit_cast<std::uint64_t>(c[i]));
        QVERIFY(std::abs(a[i]+b[i])<1e-12);
    }
    // A last-frame nonfinite input must reject the ENTIRE block without
    // touching state or even one caller output byte.
    auto corrupt=left;
    corrupt.back()=std::numeric_limits<double>::quiet_NaN();
    std::vector<double> sentinel(left.size(),-77.0);
    std::vector<double> sentinelRight(left.size(),+88.0);
    const auto old=stereo.value()->state();
    QVERIFY(!stereo.value()->process_block(
        corrupt,right,sentinel,sentinelRight));
    QVERIFY(stereo.value()->state()==old);
    QVERIFY(std::all_of(sentinel.begin(),sentinel.end(),
                        [](double v){return v==-77.0;}));
    QVERIFY(std::all_of(sentinelRight.begin(),sentinelRight.end(),
                        [](double v){return v==88.0;}));
    QVERIFY(!stereo.value()->process_block(
        left,right,sentinel,sentinel)); // overlapped output planes
    QVERIFY(stereo.value()->state()==old);

    // Transient boost above DBL_MAX must fail, NEVER clip or commit.
    const auto extreme=TransientShaperParameters::create(12.0,0.0);
    QVERIFY(extreme);
    auto overflow=TransientShaperC00Engine::create(
        *extreme.value(),48000,1,1);
    QVERIFY(overflow);
    const std::array<double,1> huge{{
        std::numeric_limits<double>::max()}};
    std::array<double,1> untouched{{-123.0}};
    QVERIFY(!overflow.value()->process_block(huge,{},untouched,{}));
    QCOMPARE(untouched[0],-123.0);
    QVERIFY(overflow.value()->state()==TransientShaperC00State{});
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::TransientShaperC00Test)
#include "test_transient_shaper_c00_engine.moc"
