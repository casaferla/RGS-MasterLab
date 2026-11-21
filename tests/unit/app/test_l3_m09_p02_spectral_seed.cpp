#include "internal/l3_m09_p02_spectral_seed.hpp"
#include <QtTest/QTest>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
[[nodiscard]] std::array<m09::SpectralSeedCell,3> cluster() {
    std::array<m09::SpectralSeedCell,3> cells{};
    const std::array<double,4> edges{100.0,110.0,120.0,130.0};
    for(std::size_t i=0;i<3;++i) {
        cells[i]={true,true,true,true,
            (edges[i]+edges[i+1])*0.5,
            edges[i],edges[i+1],
            static_cast<double>(i+1),0.25,1.0,
            0.5,0.25};
    }
    return cells;
}
class M09SpectralSeedTest final:public QObject {
    Q_OBJECT
private slots:
    void d10WeightedLogFrequencyAndSeedBound();
    void d11D12AndMissingBandsFailClosed();
    void canonicalContiguousGridAndSupportAreMandatory();
    void exactNativeEnergyType7UsesAllMemberMask();
    void noImputationOrDetectorEquivalence();
};
void M09SpectralSeedTest::d10WeightedLogFrequencyAndSeedBound() {
    const auto c=cluster();
    const auto result=m09::derive_d10_spectral_cluster_p02(c,true,true);
    QCOMPARE(result.reason,m09::SpectralSeedReason::limited_to_seed_range);
    QVERIFY(result.weighted_center_hz&&result.raw_spread_delta);
    QVERIFY(std::abs(*result.selection_score-6.0)<1e-12);
    const double center=std::exp((std::log(105.0)
         +2.0*std::log(115.0)+3.0*std::log(125.0))/6.0);
    QVERIFY(std::abs(*result.weighted_center_hz-center)<1e-11);
    QVERIFY(std::abs(*result.raw_spread_delta-14.0/6.0)<1e-12);
    QCOMPARE(*result.limited_dynamic_range_db,2.0);
    QCOMPARE(*result.cluster_lower_edge_hz,100.0);
    QCOMPARE(*result.cluster_upper_edge_hz,130.0);
    QVERIFY(result.lower_bound_priority);
    QVERIFY(!result.authorizes_product_publication());
}
void M09SpectralSeedTest::d11D12AndMissingBandsFailClosed() {
    auto c=cluster();
    c[1].d11_optimization_minus_fixed=-1.0;
    QCOMPARE(m09::derive_d10_spectral_cluster_p02(c,true,false).reason,
             m09::SpectralSeedReason::conflicting_d11_direction);
    c=cluster();
    c[0].d12_exact_compatible=false;
    QCOMPARE(m09::derive_d10_spectral_cluster_p02(c,true,false).reason,
             m09::SpectralSeedReason::missing_d12_compatibility);
    c=cluster();
    c[2].d10_requested=false;
    QCOMPARE(m09::derive_d10_spectral_cluster_p02(c,true,false).reason,
             m09::SpectralSeedReason::fewer_than_three_eligible_cells);
}
void M09SpectralSeedTest::
canonicalContiguousGridAndSupportAreMandatory() {
    auto c=cluster();
    c[2].canonical_low_edge_hz=122.0;
    QCOMPARE(m09::derive_d10_spectral_cluster_p02(c,true,false).reason,
             m09::SpectralSeedReason::unbound_canonical_grid_or_fixed);
    c=cluster();
    c[0].d10_pair_support=0.0;
    QCOMPARE(m09::derive_d10_spectral_cluster_p02(c,true,false).reason,
             m09::SpectralSeedReason::invalid_feature_or_grid);
    c=cluster();
    QCOMPARE(m09::derive_d10_spectral_cluster_p02(c,false,false).reason,
             m09::SpectralSeedReason::unbound_canonical_grid_or_fixed);
    c=cluster();
    c[1].d10_optimization_minus_fixed=-1.0;
    QCOMPARE(m09::derive_d10_spectral_cluster_p02(c,true,false).reason,
             m09::SpectralSeedReason::incoherent_d10_direction);
}
void M09SpectralSeedTest::exactNativeEnergyType7UsesAllMemberMask() {
    using Optional=std::optional<double>;
    const std::vector<std::vector<Optional>> source{
        {1.0,1.0,1.0}, // 10log10(3)
        {10.0,10.0,10.0}, // 10log10(30)
        {10.0,std::nullopt,10.0}, // dropped exact frame
        {100.0,100.0,100.0}, // 10log10(300)
        {1000.0,1000.0,1000.0} // 10log10(3000)
    };
    const auto result=m09::derive_cluster_type7_p75(source,3,true);
    QCOMPARE(result.reason,m09::NativeClusterSeriesReason::ready);
    QCOMPARE(result.common_valid_frames,std::size_t{4});
    QCOMPARE(result.excluded_missing_frames,std::size_t{1});
    QVERIFY(result.p75_dbfs);
    const double p75=0.75*(10.0*std::log10(300.0))+
                       0.25*(10.0*std::log10(3000.0));
    QVERIFY(std::abs(*result.p75_dbfs-p75)<1e-12);
    QVERIFY(!result.authorizes_dsp_detector_equivalence());
}
void M09SpectralSeedTest::noImputationOrDetectorEquivalence() {
    using Optional=std::optional<double>;
    const std::vector<std::vector<Optional>> missing{
        {1.0,2.0,std::nullopt},{1.0,std::nullopt,3.0}};
    QCOMPARE(m09::derive_cluster_type7_p75(missing,3,true).reason,
             m09::NativeClusterSeriesReason::
                no_common_positive_energy_frames);
    QCOMPARE(m09::derive_cluster_type7_p75(missing,3,false).reason,
             m09::NativeClusterSeriesReason::missing_exact_native_carrier);
    const std::vector<std::vector<Optional>> invalid{{1.0,-1.0,3.0}};
    QCOMPARE(m09::derive_cluster_type7_p75(invalid,3,true).reason,
             m09::NativeClusterSeriesReason::invalid_native_energy);
    const std::vector<std::vector<Optional>> wrong{{1.0,2.0}};
    QCOMPARE(m09::derive_cluster_type7_p75(wrong,3,true).reason,
             m09::NativeClusterSeriesReason::invalid_member_population);
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M09SpectralSeedTest)
#include "test_l3_m09_p02_spectral_seed.moc"
