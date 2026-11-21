#include "../../../dsp/src/internal/dynamic_eq_c01_coefficients.hpp"
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <QtTest/QTest>
#include <cmath>
#include <limits>
#include <vector>

namespace rgsml::tests {
using namespace rgsml::dsp;
using rgsml::dsp::internal::C01CoefficientGenerator;
class C01CoefficientTest final:public QObject {
    Q_OBJECT
private slots:
    void neutralCoefficientsAndDetector();
    void endpointGridAndFailureSemantics();
};
void C01CoefficientTest::neutralCoefficientsAndDetector() {
    DynamicEqBandSpec spec;
    spec.band_id=*core::Uuid::parse(
        "11111111-1111-4111-8111-111111111111").value();
    for(const auto type:{DynamicEqFilterType::BELL,
                         DynamicEqFilterType::LOW_SHELF,
                         DynamicEqFilterType::HIGH_SHELF}) {
        spec.filter_type=type;
        spec.shape=type==DynamicEqFilterType::BELL
            ?DynamicEqShape{DynamicEqBellShape{1000.0,0.707}}
            :DynamicEqShape{DynamicEqShelfShape{1000.0,1.0}};
        auto band=DynamicEqBandParameters::create(spec);
        QVERIFY(band);
        auto gen=C01CoefficientGenerator::create(*band.value(),48000,2);
        QVERIFY(gen);
        auto zero=gen.value()->audio_coefficients(0.0);
        auto cut=gen.value()->audio_coefficients(-12.0);
        auto boost=gen.value()->audio_coefficients(12.0);
        auto det=gen.value()->detector_coefficients();
        QVERIFY(zero&&cut&&boost&&det);
        QVERIFY(std::abs(zero.value()->b0-1.0)<1e-12);
        QVERIFY(std::abs(zero.value()->b1-zero.value()->a1)<1e-12);
        QVERIFY(std::abs(zero.value()->b2-zero.value()->a2)<1e-12);
        QVERIFY(*cut.value()!=*boost.value());
        QVERIFY(det.value()->b0>0.0);
    }
}
void C01CoefficientTest::endpointGridAndFailureSemantics() {
    DynamicEqBandSpec spec;
    spec.band_id=*core::Uuid::parse(
        "22222222-2222-4222-8222-222222222222").value();
    for(const auto fs:{44100u,48000u,96000u,192000u}) {
        for(const auto type:{DynamicEqFilterType::BELL,
                             DynamicEqFilterType::LOW_SHELF,
                             DynamicEqFilterType::HIGH_SHELF}) {
            spec.filter_type=type;
            spec.shape=type==DynamicEqFilterType::BELL
                ?DynamicEqShape{DynamicEqBellShape{20.0,0.1}}
                :DynamicEqShape{DynamicEqShelfShape{20.0,0.1}};
            auto band=DynamicEqBandParameters::create(spec);
            QVERIFY(band);
            auto gen=C01CoefficientGenerator::create(*band.value(),fs,1);
            QVERIFY(gen);
            for(const auto gain:{-18.0,0.0,18.0}) {
                auto coeff=gen.value()->audio_coefficients(gain);
                QVERIFY(coeff);
                QVERIFY(std::isfinite(coeff.value()->a1));
                QVERIFY(std::isfinite(coeff.value()->a2));
            }
            QVERIFY(gen.value()->detector_coefficients());
            QVERIFY(!gen.value()->audio_coefficients(
                std::numeric_limits<double>::infinity()));
            QVERIFY(!gen.value()->audio_coefficients(18.01));
        }
    }
    spec.enabled=false;
    auto disabled=DynamicEqBandParameters::create(spec);
    QVERIFY(disabled);
    QVERIFY(!C01CoefficientGenerator::create(*disabled.value(),48000,2));
}
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::C01CoefficientTest)
#include "test_dynamic_eq_c01_coefficients.moc"
