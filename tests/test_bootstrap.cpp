#include <rgsml/core/build_info.hpp>

#include <QtTest/QTest>

#include <string_view>

namespace rgsml::tests {

void runCorePrimitiveTests();

}  // namespace rgsml::tests

class BootstrapTest final : public QObject {
    Q_OBJECT

private slots:
    void buildInfoIsAvailable()
    {
        using rgsml::core::BuildInfo;
        QCOMPARE(std::string_view(BuildInfo::productName), std::string_view("RGS MasterLab"));
        QCOMPARE(BuildInfo::versionMajor, 0);
        QCOMPARE(BuildInfo::versionMinor, 0);
        QCOMPARE(BuildInfo::versionPatch, 0);
    }

    void corePrimitives()
    {
        rgsml::tests::runCorePrimitiveTests();
    }
};

QTEST_APPLESS_MAIN(BootstrapTest)

#include "test_bootstrap.moc"
