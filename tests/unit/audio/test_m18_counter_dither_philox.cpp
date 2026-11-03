// M18 preproduction — independent-corrected fixed Philox word oracles.
// FROZEN format: key(seed), counter(output frame,channel,0x52475344),
// Philox4x32-10, exact 53-bit grid, non-subtractive 2-LSB-p-p TPDF.
// No M17, writer, playback, app UI or qualified M18 claims in this test.
#include "internal/m18_counter_dither_philox.hpp"

#include <QtTest/QTest>
#include <array>
#include <cstdint>
#include <limits>

namespace rgsml::tests {

class M18CounterDitherPhiloxTest final : public QObject {
    Q_OBJECT
private slots:
    void correctedIndependentOracleWords();
    void genericPhiloxRoundSchedule();
    void exactUniform53Construction();
    void blockPartitionAndChannelIndependenceOfAddress();
};

void M18CounterDitherPhiloxTest::correctedIndependentOracleWords()
{
    using audio::internal::M18PhiloxWords;
    struct Vector {
        std::uint64_t seed, frame;
        std::uint32_t channel;
        M18PhiloxWords expected;
    };
    // All cases from the independently reconstructed 2026-10-04
    // mathematical pre-audit. Earlier 2026-09-12 development tuples for
    // frames 0..3 were FALSIFIED, so they MUST NOT be used as oracles.
    constexpr std::array<Vector, 11> v{{
        {UINT64_C(0x0123456789ABCDEF), 0, 0,
            {{0xA66BC7F6U,0xB58E890BU,0xFC849318U,0x2C57BBBAU}}},
        {UINT64_C(0x0123456789ABCDEF), 1, 0,
            {{0x7A4D4CA8U,0xA88EDC40U,0x3EE95F3FU,0x7732B6AEU}}},
        {UINT64_C(0x0123456789ABCDEF), 2, 0,
            {{0xFA533ACEU,0x7052D5B8U,0xB220CF76U,0x0AA621C3U}}},
        {UINT64_C(0x0123456789ABCDEF), 3, 0,
            {{0x0399B5ABU,0xD5D94BD3U,0x4B58FCE8U,0xDD6CA161U}}},
        {0, 0, 1,
            {{0xF345C558U,0xCDC63815U,0xF37D4049U,0x9B3EBDE2U}}},
        {0, 0, UINT32_MAX,
            {{0x82E9660BU,0x037C0260U,0x6DF70A23U,0xD6EF4B28U}}},
        {UINT64_C(0x0123456789ABCDEF), UINT64_C(4294967295), 0,
            {{0xC5C4CD7CU,0x5E7543E6U,0x6FA5F827U,0xF3F62D3FU}}},
        {UINT64_C(0x0123456789ABCDEF), UINT64_C(4294967296), 0,
            {{0x2C54779BU,0x8228AEADU,0xAEE65C73U,0x1143BA5BU}}},
        {UINT64_C(0x0123456789ABCDEF), UINT64_C(4294967297), 0,
            {{0xE3D1D048U,0x47CE0F7EU,0x7EAD1A00U,0x5A677ED7U}}},
        {UINT64_MAX, 0, 1,
            {{0xE1DCD9FFU,0x4594FE99U,0x1C356896U,0xB888F0D2U}}},
        {UINT64_MAX, 0, UINT32_MAX,
            {{0x668C3F68U,0xD4D94A4CU,0x6AB2EFBFU,0x87F980A9U}}}
    }};
    for (const auto& x : v) {
        const auto actual = audio::internal::m18_philox4x32_10(
            x.seed, x.frame, x.channel);
        QVERIFY(actual == x.expected);
    }
}

void M18CounterDitherPhiloxTest::genericPhiloxRoundSchedule()
{
    using audio::internal::M18PhiloxWords;
    using audio::internal::M18PhiloxKey;
    const auto maximal=audio::internal::m18_philox4x32_10_core(
        M18PhiloxWords{{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX}},
        M18PhiloxKey{{UINT32_MAX,UINT32_MAX}});
    const M18PhiloxWords maximalExpected{{
        0x408F276DU,0x41C83B0EU,0xA20BC7C6U,0x6D5451FDU}};
    QVERIFY(maximal == maximalExpected);
    const auto pi=audio::internal::m18_philox4x32_10_core(
        M18PhiloxWords{{0x243F6A88U,0x85A308D3U,0x13198A2EU,0x03707344U}},
        M18PhiloxKey{{0xA4093822U,0x299F31D0U}});
    const M18PhiloxWords piExpected{{
        0xD16CFE09U,0x94FDCCEBU,0x5001E420U,0x24126EA1U}};
    QVERIFY(pi == piExpected);
}

void M18CounterDitherPhiloxTest::exactUniform53Construction()
{
    using namespace audio::internal;
    constexpr auto maxZ = UINT64_C(9007199254740991);
    QCOMPARE(m18_uniform53(UINT32_MAX,UINT32_MAX),maxZ);
    QCOMPARE(m18_uniform53(0,0),UINT64_C(0));
    QCOMPARE(m18_uniform53(0,0x800U),UINT64_C(1));
    constexpr auto pair=m18_tpdf_from_words(
        {{UINT32_MAX,UINT32_MAX,0,0}});
    QCOMPARE(pair.u0,0x1.fffffffffffffp-1);
    QCOMPARE(pair.u1,0.0);
    QCOMPARE(pair.dither_lsb(),0x1.fffffffffffffp-1);
    const auto minpair=m18_tpdf_from_words(
        {{0,0,UINT32_MAX,UINT32_MAX}});
    QCOMPARE(minpair.dither_lsb(),-0x1.fffffffffffffp-1);
    for (std::uint64_t frame : {UINT64_C(0),UINT64_C(1),
             UINT64_C(0xFFFFFFFF),UINT64_C(0x100000000)}) {
        for (std::uint32_t channel : {0U,1U,31U}) {
            const auto d=m18_tpdf(UINT64_C(0x0123456789ABCDEF),
                                   frame,channel);
            QVERIFY(d.u0>=0.0 && d.u0<1.0);
            QVERIFY(d.u1>=0.0 && d.u1<1.0);
            QVERIFY(d.dither_lsb()>-1.0 && d.dither_lsb()<1.0);
        }
    }
}

void M18CounterDitherPhiloxTest::blockPartitionAndChannelIndependenceOfAddress()
{
    using audio::internal::m18_philox4x32_10;
    constexpr auto seed=UINT64_C(0x0123456789ABCDEF);
    constexpr std::uint64_t start=UINT64_C(0xFFFFFFFB);
    std::array<audio::internal::M18PhiloxWords, 17> oneBlock{};
    for (std::size_t i=0;i<oneBlock.size();++i)
        oneBlock[i]=m18_philox4x32_10(seed,start+i,0);
    // Repartition into ragged chunks, including the 2^32 frame carry.
    std::array<audio::internal::M18PhiloxWords, 17> ragged{};
    std::size_t cursor=0;
    for (const std::size_t chunk : {1U,2U,3U,7U,4U}) {
        for (std::size_t i=0;i<chunk;++i)
            ragged[cursor+i]=m18_philox4x32_10(
                seed,start+cursor+i,0);
        cursor+=chunk;
    }
    QCOMPARE(cursor,oneBlock.size());
    QVERIFY(oneBlock == ragged);
    // Distinct counter words for L and R, not mutable shared RNG state.
    for (std::size_t i=0;i<oneBlock.size();++i)
        QVERIFY(oneBlock[i] != m18_philox4x32_10(seed,start+i,1));
}

} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::M18CounterDitherPhiloxTest)
#include "test_m18_counter_dither_philox.moc"
