#include <rgsml/audio/external_playback_fir_assets.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>

#include <QFile>
#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>

#include <algorithm>
#include <array>
#include <numeric>


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
    void allThirtyRatesHaveOneExactSuccessAndMissingAssetFailureContract();
    void missingCorruptAndOwnedLifetime();
    void full16AssetManifestAnd30DeviceRouteAdmissionBoundary();
    void relocatedFlatFirPackageKeepsBitwiseLegacyAndRejectsPrivateSpoof();

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
    // 14 original Q0 FIRs are used for every other DEVICE path. The two
    // historical coefficient sets remain bit-exact and separately pinned.
    auto unified=make(44100,96000);
    QVERIFY(static_cast<bool>(unified));
    QCOMPARE(unified.value()->output_rate().value(),std::int64_t{96000});
    const auto* pin=audio::internal::phase11_frozen_original_fir_pin(320,147);
    QVERIFY(pin);
    QCOMPARE(unified.value()->kernel_sha256(),std::string_view{pin->sha256});
    QVERIFY(!make(44100,44100)); // identity uses direct playback, no FIR
}

// One uniform observable contract: every nonidentity six-rate direction
// uses the same SHA-pinned loader and fails the same way when an asset is
// missing. Historical 2 and original-Q0 28 are NOT separate UI or FQ paths.
void ExternalPlaybackFirAssetsTests::
allThirtyRatesHaveOneExactSuccessAndMissingAssetFailureContract()
{
    constexpr std::array<std::int64_t,6> rates{
        44100,48000,88200,96000,176400,192000};
    const auto root=test_asset_root();
    QVERIFY(!root.empty());
    audio::configure_external_playback_fir_root(root.u16string());
    std::size_t count=0,unchanged=0,original=0;
    for(const auto src:rates)for(const auto dst:rates) {
        if(src==dst)continue;
        const auto result=make(src,dst);
        QVERIFY2(result,result?"":result.error()->message().c_str());
        QVERIFY(!result.value()->kernel_sha256().empty());
        const bool historic=(src==44100&&dst==48000)
            ||(src==48000&&dst==44100);
        if(historic) {
            ++unchanged;
        } else {
            ++original;
            const auto divisor=std::gcd(src,dst);
            const auto* pin=audio::internal::phase11_frozen_original_fir_pin(
                static_cast<std::uint32_t>(dst/divisor),
                static_cast<std::uint32_t>(src/divisor));
            QVERIFY(pin);
            QCOMPARE(result.value()->kernel_sha256(),
                     std::string_view{pin->sha256});
        }
        ++count;
    }
    QCOMPARE(count,std::size_t{30});
    QCOMPARE(unchanged,std::size_t{2});
    QCOMPARE(original,std::size_t{28});

    QTemporaryDir empty;
    QVERIFY(empty.isValid());
    audio::configure_external_playback_fir_root(
        std::filesystem::path{empty.path().toStdString()}.u16string());
    for(const auto src:rates)for(const auto dst:rates) {
        if(src==dst)continue;
        const auto result=make(src,dst);
        QVERIFY(!result);
        QCOMPARE(result.error()->message(),
                 std::string{"PLAYBACK_FIR_RESOURCE_MISSING_OR_UNSAFE"});
    }
    audio::configure_external_playback_fir_root(root.u16string());
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
    // No generated fallback: the Q0 source must be present, authentic
    // and rooted at the identical flat 16-file package.
    QVERIFY(!make(44100,96000));
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


// One public 16-entry SHA inventory has TWO legitimate build stages:
// PR Focused (2 built Playback + 14 pinned source identities) and FQ
// (all 16 approved actual binaries at the SAME flat resource root).
// Never accept a 2-file package masquerading as a complete 16-file FQ.
void ExternalPlaybackFirAssetsTests::
full16AssetManifestAnd30DeviceRouteAdmissionBoundary()
{
    const auto root=test_asset_root();
    QVERIFY(!root.empty());
    QFile manifest{QString::fromStdString((root/"fir_manifest.json").string())};
    QVERIFY(manifest.open(QIODevice::ReadOnly));
    QJsonParseError error{};
    const auto parsed=QJsonDocument::fromJson(manifest.readAll(),&error);
    QCOMPARE(error.error,QJsonParseError::NoError);
    QVERIFY(parsed.isObject());
    const auto obj=parsed.object();
    QCOMPARE(obj.value(QStringLiteral("schema")).toString(),
             QStringLiteral("rgsml.external-src-fir-assets/2"));
    QCOMPARE(obj.value(QStringLiteral("installationRoot")).toString(),
             QStringLiteral("resources/fir"));
    QVERIFY(!obj.value(QStringLiteral("releaseAuthority")).toBool());
    QVERIFY(obj.value(QStringLiteral("publicOriginalFirDistributionAuthorized")).toBool());
    const bool all16Packed=obj.value(QStringLiteral("complete16FilePackage")).toBool();
    QCOMPARE(obj.value(QStringLiteral("finalSrc")).toObject()
        .value(QStringLiteral("includedInPublicBuild")).toBool(),all16Packed);
    const auto files=obj.value(QStringLiteral("assets")).toArray();
    QCOMPARE(files.size(),16);
    QSet<QString> filenames;
    QHash<QString,QString> pinnedDigests;
    constexpr std::array<const char*,2> frozenFileNames{
        "playback_src_kernel_44100_48000.bin",
        "playback_src_kernel_48000_44100.bin"};
    constexpr std::array<const char*,2> frozenDigests{
        "ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4",
        "37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22"};
    for(int i=0;i<files.size();++i) {
        QVERIFY(files.at(i).isObject());
        const auto entry=files.at(i).toObject();
        const auto filename=entry.value(QStringLiteral("file")).toString();
        const auto sha=entry.value(QStringLiteral("sha256")).toString();
        QCOMPARE(sha.size(),64);
        QVERIFY(!filename.isEmpty());
        QVERIFY(!filenames.contains(filename));
        filenames.insert(filename);
        pinnedDigests.insert(filename,sha);
        if(i<2) {
            QCOMPARE(filename,QString::fromLatin1(frozenFileNames[static_cast<std::size_t>(i)]));
            QCOMPARE(sha,QString::fromLatin1(frozenDigests[static_cast<std::size_t>(i)]));
            QCOMPARE(entry.value(QStringLiteral("role")).toString(),
                QStringLiteral("PLAYBACK_FROZEN"));
            QVERIFY(std::filesystem::is_regular_file(root/filename.toStdString()));
        } else {
            QCOMPARE(entry.value(QStringLiteral("role")).toString(),
                QStringLiteral("FINAL_SRC_Q0_ORIGINAL"));
            QCOMPARE(entry.value(QStringLiteral("packaged")).toBool(),
                     all16Packed);
            const auto L=entry.value(QStringLiteral("ratioL")).toInt();
            const auto M=entry.value(QStringLiteral("ratioM")).toInt();
            QVERIFY(L>0 && M>0 && std::gcd(L,M)==1);
            const auto* pin=audio::internal::phase11_frozen_original_fir_pin(
                static_cast<std::uint32_t>(L),static_cast<std::uint32_t>(M));
            QVERIFY(pin);
            QCOMPARE(sha,QString::fromLatin1(pin->sha256));
            QCOMPARE(filename,QStringLiteral("RGSMSR1_L%1_M%2.bin").arg(L).arg(M));
            const auto original=root/filename.toStdString();
            if(all16Packed) {
                QVERIFY(std::filesystem::is_regular_file(original));
                QFile binary{QString::fromStdString(original.string())};
                QVERIFY(binary.open(QIODevice::ReadOnly));
                QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(
                    binary.readAll(),QCryptographicHash::Sha256).toHex()),sha);
            } else {
                // A non-FQ developer build cannot silently claim an
                // authenticated complete archive from SHA identities.
                QVERIFY(!std::filesystem::exists(original));
            }
        }
    }
    QCOMPARE(filenames.size(),16);
    const auto routes=obj.value(QStringLiteral("routes")).toArray();
    QCOMPARE(routes.size(),30);
    QSet<QString> seen;
    constexpr std::array<int,6> rates{
        44100,48000,88200,96000,176400,192000};
    int historicallyAdmitted=0;
    for(const auto routeJson:routes) {
        QVERIFY(routeJson.isObject());
        const auto route=routeJson.toObject();
        const int source=route.value(QStringLiteral("sourceRateHz")).toInt();
        const int target=route.value(QStringLiteral("targetRateHz")).toInt();
        const auto key=QStringLiteral("%1_%2").arg(source).arg(target);
        QVERIFY(source!=target);
        QVERIFY(std::find(rates.begin(),rates.end(),source)!=rates.end());
        QVERIFY(std::find(rates.begin(),rates.end(),target)!=rates.end());
        QVERIFY(!seen.contains(key));
        seen.insert(key);
        const auto kernel=route.value(QStringLiteral("kernelFile")).toString();
        QVERIFY(pinnedDigests.contains(kernel));
        const auto divisor=std::gcd(source,target);
        const auto L=target/divisor;
        const auto M=source/divisor;
        QCOMPARE(route.value(QStringLiteral("ratioL")).toInt(),L);
        QCOMPARE(route.value(QStringLiteral("ratioM")).toInt(),M);
        const bool frozen=(source==44100 && target==48000)
                          ||(source==48000 && target==44100);
        if(frozen) {
            ++historicallyAdmitted;
            QVERIFY(route.value(QStringLiteral("devicePlaybackAdmitted")).toBool());
            QCOMPARE(route.value(QStringLiteral("role")).toString(),
                QStringLiteral("PLAYBACK_FROZEN"));
            QCOMPARE(kernel,QStringLiteral("playback_src_kernel_%1.bin").arg(key));
        } else {
            QVERIFY(!route.value(QStringLiteral("devicePlaybackAdmitted")).toBool());
            QCOMPARE(route.value(QStringLiteral("role")).toString(),
                QStringLiteral("Q0_ORIGINAL_PENDING_DEVICE_QUALIFICATION"));
            QCOMPARE(kernel,QStringLiteral("RGSMSR1_L%1_M%2.bin").arg(L).arg(M));
            const auto* pin=audio::internal::phase11_frozen_original_fir_pin(
                static_cast<std::uint32_t>(L),static_cast<std::uint32_t>(M));
            QVERIFY(pin);
            QCOMPARE(pinnedDigests.value(kernel),QString::fromLatin1(pin->sha256));
        }
    }
    QCOMPARE(historicallyAdmitted,2);
    // Thirty distinct, directed nonidentity pairs over six Fs values
    // exhaust the intended matrix; no missing route can be hidden.
    for(const int source:rates)for(const int target:rates) {
        if(source!=target)
            QVERIFY(seen.contains(QStringLiteral("%1_%2").arg(source).arg(target)));
    }
}

void ExternalPlaybackFirAssetsTests::
relocatedFlatFirPackageKeepsBitwiseLegacyAndRejectsPrivateSpoof()
{
    const auto originalRoot=test_asset_root();
    QVERIFY(!originalRoot.empty());
    audio::configure_external_playback_fir_root(originalRoot.u16string());
    const auto baselineUp=make(44100,48000);
    const auto baselineDown=make(48000,44100);
    QVERIFY(baselineUp && baselineDown);
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto relocated=std::filesystem::path{temporary.path().toStdString()};
    std::error_code ec;
    for(const char* name:{"playback_src_kernel_44100_48000.bin",
                           "playback_src_kernel_48000_44100.bin",
                           "fir_manifest.json"}) {
        std::filesystem::copy_file(originalRoot/name,relocated/name,
            std::filesystem::copy_options::none,ec);
        QVERIFY(!ec);
    }
    // A forged original Q0 filename must never satisfy DEVICE admission.
    {
        std::ofstream fake{relocated/"RGSMSR1_L320_M147.bin",std::ios::binary};
        QVERIFY(static_cast<bool>(fake));
        fake << "unqualified-data";
        QVERIFY(static_cast<bool>(fake));
    }
    audio::configure_external_playback_fir_root(relocated.u16string());
    const auto relocatedUp=make(44100,48000);
    const auto relocatedDown=make(48000,44100);
    QVERIFY(relocatedUp && relocatedDown);
    QCOMPARE(relocatedUp.value()->kernel_size(),baselineUp.value()->kernel_size());
    QCOMPARE(relocatedDown.value()->kernel_size(),baselineDown.value()->kernel_size());
    for(std::size_t k=0;k<baselineUp.value()->kernel_size();++k)
        QCOMPARE(relocatedUp.value()->coefficient_bits(k),
                 baselineUp.value()->coefficient_bits(k));
    for(std::size_t k=0;k<baselineDown.value()->kernel_size();++k)
        QCOMPARE(relocatedDown.value()->coefficient_bits(k),
                 baselineDown.value()->coefficient_bits(k));
    // Repackaging only two historic assets + forged Q0 cannot silently
    // enable an unverified original. No synthetic fallback is permitted.
    QVERIFY(!make(44100,96000));
    // Refresh root after removal: tampering/absence is never masked by
    // a previously cached FILE; earlier adapters still own immutable bits.
    QVERIFY(std::filesystem::remove(
        relocated/"playback_src_kernel_48000_44100.bin",ec));
    QVERIFY(!ec);
    audio::configure_external_playback_fir_root(relocated.u16string());
    QVERIFY(!make(48000,44100));
    QCOMPARE(relocatedDown.value()->coefficient_bits(0),
        baselineDown.value()->coefficient_bits(0));
    audio::configure_external_playback_fir_root(originalRoot.u16string());
}

QTEST_APPLESS_MAIN(ExternalPlaybackFirAssetsTests)
#include "test_external_playback_fir_assets.moc"
