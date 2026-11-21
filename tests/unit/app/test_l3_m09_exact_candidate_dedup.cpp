#include "internal/l3_m09_exact_candidate_dedup.hpp"
#include <QtTest/QTest>
#include <cstdint>
#include <string>
#include <vector>

namespace rgsml::tests {
namespace {
namespace m09=rgsml::app::l3_automatch::m09_frozen;
[[nodiscard]] m09::ExactBytes bytes(const char* text) {
    m09::ExactBytes result;
    for(const unsigned char* p=
            reinterpret_cast<const unsigned char*>(text);*p;++p)
        result.push_back(*p);
    return result;
}
[[nodiscard]] m09::CandidateCarrier candidate(
    const char* key,const char* seed,const char* dependency,
    const char* source="first") {
    return {bytes(key),bytes(seed),bytes(dependency),
            bytes("exact-snapshot-A"),{std::string{source}}};
}
class M09ExactDedupTest final:public QObject {
    Q_OBJECT
private slots:
    void exactTripleAndProvenanceOrder();
    void strictCollisionPoisonsGeneration();
    void variantsAreSeparateAndNoTypeIdOnlyDedup();
    void dependencyAndSnapshotAreNotSilentlyRewritten();
    void malformedEvidenceCannotEscape();
};
void M09ExactDedupTest::exactTripleAndProvenanceOrder() {
    m09::ExactCandidateGeneration generation{bytes("exact-snapshot-A")};
    QCOMPARE(generation.add(candidate("domain/tonal/eq/bell",
                                      "gain1.5","source-M07","first")),
             m09::InsertStatus::inserted);
    QCOMPARE(generation.add(candidate("domain/tonal/eq/bell",
                                      "gain1.5","source-M07","second")),
             m09::InsertStatus::exact_duplicate);
    const auto* accepted=generation.descriptions();
    QVERIFY(accepted);
    QCOMPARE(accepted->size(),std::size_t{1});
    QCOMPARE((*accepted)[0].ordered_provenance.size(),std::size_t{2});
    QCOMPARE((*accepted)[0].ordered_provenance[0],std::string{"first"});
    QCOMPARE((*accepted)[0].ordered_provenance[1],std::string{"second"});
    QVERIFY(!generation.authorizes_product_publication());
    QVERIFY(!(*accepted)[0].authorizes_product_publication());
}
void M09ExactDedupTest::strictCollisionPoisonsGeneration() {
    m09::ExactCandidateGeneration generation{bytes("exact-snapshot-A")};
    QCOMPARE(generation.add(candidate("complete-semantic-key",
                                      "seed-A","dep-A")),
             m09::InsertStatus::inserted);
    QCOMPARE(generation.add(candidate("complete-semantic-key",
                                      "seed-B","dep-B")),
             m09::InsertStatus::candidate_key_seed_collision);
    QVERIFY(generation.has_seed_collision());
    QVERIFY(generation.failed());
    QVERIFY(!generation.descriptions()); // cannot emit prior first-wins
    QCOMPARE(generation.add(candidate("other","A","dep")),
             m09::InsertStatus::generation_already_failed);
}
void M09ExactDedupTest::variantsAreSeparateAndNoTypeIdOnlyDedup() {
    m09::ExactCandidateGeneration generation{bytes("exact-snapshot-A")};
    QCOMPARE(generation.add(candidate(
        "stereo/side-eq/variant-LR12-120","seed1","dep")),
        m09::InsertStatus::inserted);
    QCOMPARE(generation.add(candidate(
        "stereo/side-eq/variant-LR24-120","seed2","dep")),
        m09::InsertStatus::inserted);
    QCOMPARE(generation.add(candidate(
        "tonal/parametric-eq/bell","seed3","dep")),
        m09::InsertStatus::inserted);
    QCOMPARE(generation.add(candidate(
        "stereo/parametric-eq/side","seed4","dep")),
        m09::InsertStatus::inserted);
    QVERIFY(generation.descriptions());
    QCOMPARE(generation.descriptions()->size(),std::size_t{4});
}
void M09ExactDedupTest::dependencyAndSnapshotAreNotSilentlyRewritten() {
    m09::ExactCandidateGeneration generation{bytes("exact-snapshot-A")};
    QCOMPARE(generation.add(candidate("key","seed","dependency-A")),
             m09::InsertStatus::inserted);
    QCOMPARE(generation.add(candidate("key","seed","dependency-B")),
             m09::InsertStatus::inserted); // not exact triple
    QVERIFY(generation.descriptions());
    QCOMPARE(generation.descriptions()->size(),std::size_t{2});
    auto stale=candidate("key","seed","dependency-C");
    stale.generation_snapshot=bytes("exact-snapshot-B");
    QCOMPARE(generation.add(std::move(stale)),
             m09::InsertStatus::invalid_input);
    QVERIFY(!generation.descriptions());
    // A changed snapshot belongs in a NEW generation, not a collision.
    m09::ExactCandidateGeneration fresh{bytes("exact-snapshot-B")};
    auto current=candidate("key","changed-seed","dependency-C");
    current.generation_snapshot=bytes("exact-snapshot-B");
    QCOMPARE(fresh.add(std::move(current)),
             m09::InsertStatus::inserted);
    QVERIFY(fresh.descriptions());
}
void M09ExactDedupTest::malformedEvidenceCannotEscape() {
    m09::ExactCandidateGeneration invalid_snapshot{m09::ExactBytes{}};
    QVERIFY(!invalid_snapshot.descriptions());
    QCOMPARE(invalid_snapshot.add(candidate("k","s","d")),
             m09::InsertStatus::generation_already_failed);
    m09::ExactCandidateGeneration generation{bytes("exact-snapshot-A")};
    auto bad=candidate("key","seed","dependency");
    bad.ordered_provenance={""};
    QCOMPARE(generation.add(std::move(bad)),
             m09::InsertStatus::invalid_input);
    QVERIFY(!generation.descriptions());
    QVERIFY(!generation.has_seed_collision());
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M09ExactDedupTest)
#include "test_l3_m09_exact_candidate_dedup.moc"
