// M22 real Product capture: compile-time binding to the EXISTING M18 owner.
// This test is deliberately not a synthetic Product admission or release
// simulation: a genuinely installed current M18 owner is required at runtime.
#include "internal/m22_product_chain_epoch_capture_candidate.hpp"
#include <rgsml/core/result.hpp>
#include <QtTest/QTest>
#include <memory>
#include <type_traits>
#include <utility>

namespace rgsml::tests {
using Probe=app::internal::M22ProductChainEpochCaptureCandidate;
static_assert(std::is_same_v<
    decltype(Probe::capture(
        std::declval<const Probe::Controllers&>(),
        std::declval<const Probe::Fanout&>(),
        std::declval<std::shared_ptr<const Probe::Stage>>())),
    core::Result<Probe::Capture>>);
static_assert(std::is_same_v<
    decltype(Probe::still_current(
        std::declval<const Probe::Controllers&>(),
        std::declval<const Probe::Fanout&>(),
        std::declval<const Probe::Capture&>())),bool>);
static_assert(std::is_same_v<
    decltype(std::declval<const Probe::Capture&>().authorizes_release()),
    bool>);
static_assert(!std::is_default_constructible_v<Probe::Capture>);

class M22ProductChainEpochContractTest final: public QObject {
    Q_OBJECT
private slots:
    void captureRequiresRealProductAndCannotBeDefaultMinted() {
        QVERIFY(!std::is_default_constructible_v<Probe::Capture>);
        // There is no setter, fake epoch generator, separate master
        // publisher, self-declared FIR pin or export operation here.
        // Actual Qt owner-thread / staged-stage checks remain in capture().
        QVERIFY(std::is_copy_constructible_v<Probe::Capture>);
        QVERIFY(std::is_move_constructible_v<Probe::Capture>);
    }
};
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::M22ProductChainEpochContractTest)
#include "test_m22_product_chain_epoch_capture_contract.moc"
