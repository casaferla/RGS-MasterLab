#include <rgsml/audio/external_playback_fir_assets.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>

#include <QTest>
#include <QByteArray>
#include <QTemporaryDir>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

using namespace rgsml;

namespace {
[[nodiscard]] std::filesystem::path test_asset_root()
{
#ifdef _WIN32
    // Keep the isolated native test compatible with production MSVC
    // warnings-as-errors, without hiding C4996 globally.
    char* value=nullptr;
    std::size_t chars=0;
    if(_dupenv_s(&value,&chars,"RGSML_PLAYBACK_FIR_ROOT")!=0
       ||value==nullptr||chars<2) {
        std::free(value);
        return {};
    }
    const std::filesystem::path root{value};
    std::free(value);
    return root;
#else
    const auto* value=std::getenv("RGSML_PLAYBACK_FIR_ROOT");
    return value ? std::filesystem::path{value} : std::filesystem::path{};
#endif
}
[[nodiscard]] core::Result<audio::PlaybackSampleRateAdapter> make(
    std::int64_t input,std::int64_t output)
{
    const auto in=core::SampleRate::create(input);
    const auto out=core::SampleRate::create(output);
    const auto frames=core::FrameCount::create(1000);
    if(!in ||!out ||!frames)
        return core::Result<audio::PlaybackSampleRateAdapter>::failure(
            core::Error{core::ErrorCode::InvalidArgument,
                        "Invalid frozen external-FIR test geometry"});
    return audio::PlaybackSampleRateAdapter::create(
        audio::PlaybackRateSpec{*in.value(),*out.value(),
            audio::ChannelLayout::STEREO_LR,*frames.value()});
}
} // namespace

class ExternalPlaybackFirAssetsTests final : public QObject {
    Q_OBJECT
private slots:
    void exactFrozenLegacyAssetPack();
    void missingCorruptAndOwnedLifetime();
};

void ExternalPlaybackFirAssetsTests::exactFrozenLegacyAssetPack()
{
    const auto root=test_asset_root();
    QVERIFY(!root.empty());
    audio::configure_external_playback_fir_root(root.u16string());
    auto forward=make(44100,48000);
    auto backward=make(48000,44100);
    QVERIFY(static_cast<bool>(forward));
    QVERIFY(static_cast<bool>(backward));
    QCOMPARE(forward.value()->kernel_size(),std::size_t{30721});
    QCOMPARE(backward.value()->kernel_size(),std::size_t{28225});
    QVERIFY(forward.value()->kernel_sha256()==
        std::string_view{"ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4"});
    QVERIFY(backward.value()->kernel_sha256()==
        std::string_view{"37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22"});
    QCOMPARE(forward.value()->coefficient_bits(0),std::uint64_t{0xbe82172c5b578940});
    QCOMPARE(backward.value()->coefficient_bits(0),std::uint64_t{0xbe82172c5b5d4f61});
    QVERIFY(!make(44100,96000)); // playback v1 is NOT Phase11 Final SRC
}

void ExternalPlaybackFirAssetsTests::missingCorruptAndOwnedLifetime()
{
    const auto root=test_asset_root();
    QVERIFY(!root.empty());
    audio::configure_external_playback_fir_root(root.u16string());
    auto original=make(44100,48000);
    QVERIFY(static_cast<bool>(original));
    const auto first=original.value()->coefficient_bits(0);

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto empty=std::filesystem::path{temp.path().toStdString()};
    audio::configure_external_playback_fir_root(empty.u16string());
    QVERIFY(!make(44100,48000));
    // Previously admitted existing adapters retain their OWN immutable asset.
    QCOMPARE(original.value()->coefficient_bits(0),first);

    std::error_code ec;
    const auto tamper=empty/"playback_src_kernel_44100_48000.bin";
    std::filesystem::copy_file(
        root/"playback_src_kernel_44100_48000.bin",tamper,
        std::filesystem::copy_options::none,ec);
    QVERIFY(!ec);
    {
        std::fstream file(tamper,std::ios::in|std::ios::out|std::ios::binary);
        QVERIFY(static_cast<bool>(file));
        char byte=0;
        file.read(&byte,1);
        QVERIFY(static_cast<bool>(file));
        byte=static_cast<char>(static_cast<unsigned char>(byte)^0x01U);
        file.seekp(0);
        file.write(&byte,1);
        QVERIFY(static_cast<bool>(file));
    }
    QVERIFY(!make(44100,48000)); // SHA pin prevents installation of fake FIR
    audio::configure_external_playback_fir_root(root.u16string());
    QVERIFY(static_cast<bool>(make(44100,48000)));
}

QTEST_APPLESS_MAIN(ExternalPlaybackFirAssetsTests)
#include "test_external_playback_fir_assets.moc"
