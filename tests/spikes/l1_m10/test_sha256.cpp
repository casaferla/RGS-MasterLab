#include <rgsml/core/sha256.hpp>

#include <QTest>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace {

std::span<const std::byte> bytes(std::string_view text)
{
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

class Sha256SpikeTest final : public QObject {
    Q_OBJECT

private slots:
    void knownAnswers();
    void irregularChunksMatchOneShot();
};

void Sha256SpikeTest::knownAnswers()
{
    const std::array<std::pair<std::string_view, std::string_view>, 3> vectors{{
        {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
    }};
    for (const auto& [input, expected] : vectors) {
        const auto hex = rgsml::core::sha256_hex(bytes(input));
        QCOMPARE(hex, expected);
        QCOMPARE(hex.size(), std::size_t{64});
        QVERIFY(std::all_of(hex.begin(), hex.end(), [](char ch) {
            return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
        }));
    }
}

void Sha256SpikeTest::irregularChunksMatchOneShot()
{
    constexpr std::string_view input =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    rgsml::core::Sha256 sha;
    std::size_t offset = 0;
    const std::array<std::size_t, 6> sizes{1, 7, 3, 17, 2, 11};
    std::size_t next = 0;
    while (offset < input.size()) {
        const auto count = std::min(sizes[next++ % sizes.size()], input.size() - offset);
        sha.update(bytes(input.substr(offset, count)));
        offset += count;
    }
    QCOMPARE(rgsml::core::sha256_hex(sha.finalize()),
             rgsml::core::sha256_hex(bytes(input)));
}

}  // namespace

QTEST_APPLESS_MAIN(Sha256SpikeTest)
#include "test_sha256.moc"
