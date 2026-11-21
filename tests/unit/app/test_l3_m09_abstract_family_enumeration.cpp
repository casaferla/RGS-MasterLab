#include "internal/l3_m09_abstract_family_enumeration.hpp"
#include <QtTest/QTest>
#include <vector>
namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
class M09FamilyEnumerationTest final:public QObject {
    Q_OBJECT
private slots:
    void allFrozenFamiliesPreserveCardinalityWithoutM10Truncation();
    void monoExcludesStereoBranchesButNotDSPDynamicAlternatives();
    void canonicalRolesAndTerminalRemainSeparate();
    void enumerationIsDeterministicAndNeverPublishes();
};
void M09FamilyEnumerationTest::
allFrozenFamiliesPreserveCardinalityWithoutM10Truncation() {
    const auto full=m09::enumerate_frozen_abstract_families(6,3,3,true);
    QVERIFY(full);
    QCOMPARE(full->ordered.size(),std::size_t{37632});
    QVERIFY(!full->authorizes_product_publication());
    QCOMPARE(full->ordered.front().tonal_eq_prefix,std::uint8_t{0});
    QCOMPARE(full->ordered.back().tonal_eq_prefix,std::uint8_t{6});
    QCOMPARE(full->ordered.back().spectral_dynamic_eq_prefix,std::uint8_t{3});
    QCOMPARE(full->ordered.back().stereo_side_eq_prefix,std::uint8_t{3});
    QCOMPARE(full->ordered.back().stereo_mode_index,std::uint8_t{6});
    QCOMPARE(full->ordered.back().loudness,
             m09::LoudnessMode::terminal_limiter_only);
    // M10 domain8/combined32/shortlist2 caps must NOT truncate M09.
}
void M09FamilyEnumerationTest::
monoExcludesStereoBranchesButNotDSPDynamicAlternatives() {
    const auto mono=m09::enumerate_frozen_abstract_families(6,3,0,false);
    QVERIFY(mono);
    QCOMPARE(mono->ordered.size(),std::size_t{896});
    for(const auto& c:mono->ordered) {
        QVERIFY(c.macro!=m09::MacroMode::stereo_mean);
        QCOMPARE(c.stereo_side_eq_prefix,std::uint8_t{0});
        QCOMPARE(c.stereo_mode_index,std::uint8_t{0});
    }
    QVERIFY(!m09::enumerate_frozen_abstract_families(6,3,1,false));
    QVERIFY(!m09::enumerate_frozen_abstract_families(7,3,0,true));
    QVERIFY(!m09::enumerate_frozen_abstract_families(6,4,0,true));
    QVERIFY(!m09::enumerate_frozen_abstract_families(6,3,4,true));
}
void M09FamilyEnumerationTest::
canonicalRolesAndTerminalRemainSeparate() {
    m09::AbstractFamilyChoice full{
        1,1,m09::MacroMode::linked_max,m09::MicroMode::attack,
        1,1,m09::LoudnessMode::gain_with_terminal_limiter};
    const std::vector<m09::IntentRole> expected{
        m09::IntentRole::tonal_eq,
        m09::IntentRole::spectral_dynamic_eq,
        m09::IntentRole::macro_compressor,
        m09::IntentRole::micro_transient,
        m09::IntentRole::stereo_side_eq,
        m09::IntentRole::stereo_ms,
        m09::IntentRole::loudness_gain,
        m09::IntentRole::terminal_limiter};
    QCOMPARE(m09::ordered_abstract_roles(full),expected);
    full.loudness=m09::LoudnessMode::terminal_limiter_only;
    const auto roles=m09::ordered_abstract_roles(full);
    QCOMPARE(roles.back(),m09::IntentRole::terminal_limiter);
    QVERIFY(!full.executable_or_pub_ready());
}
void M09FamilyEnumerationTest::
enumerationIsDeterministicAndNeverPublishes() {
    const auto x=m09::enumerate_frozen_abstract_families(1,1,1,true);
    const auto y=m09::enumerate_frozen_abstract_families(1,1,1,true);
    QVERIFY(x&&y);
    QCOMPARE(x->ordered.size(),y->ordered.size());
    for(std::size_t i=0;i<x->ordered.size();++i) {
        const auto& a=x->ordered[i];
        const auto& b=y->ordered[i];
        QCOMPARE(a.tonal_eq_prefix,b.tonal_eq_prefix);
        QCOMPARE(a.spectral_dynamic_eq_prefix,b.spectral_dynamic_eq_prefix);
        QCOMPARE(a.macro,b.macro);
        QCOMPARE(a.micro,b.micro);
        QCOMPARE(a.stereo_side_eq_prefix,b.stereo_side_eq_prefix);
        QCOMPARE(a.stereo_mode_index,b.stereo_mode_index);
        QCOMPARE(a.loudness,b.loudness);
    }
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M09FamilyEnumerationTest)
#include "test_l3_m09_abstract_family_enumeration.moc"
