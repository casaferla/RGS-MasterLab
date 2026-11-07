#include <rgsml/project/m18_master_wav_preference.hpp>
#include <rgsml/core/error.hpp>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <optional>

namespace rgsml::project {
namespace {
using Json=nlohmann::json;
constexpr const char* kEncoding="finalMasterWavEncoding";
constexpr const char* kPcm24Seed="finalMasterPcm24TpdfSeed";

[[nodiscard]] core::Error invalid() {
    return core::Error{core::ErrorCode::InvalidArgument,
        "M18 canonical WAV output encoding or TPDF seed is invalid."};
}
[[nodiscard]] std::optional<M18MasterWavPreference>
decode(const Json& j)
{
    if(!j.is_object()||!j.contains(kEncoding)
       ||!j.at(kEncoding).is_string())return std::nullopt;
    const auto& format=j.at(kEncoding);
    if(format=="PCM24_TPDF") {
        if(!j.contains(kPcm24Seed)
           ||!j.at(kPcm24Seed).is_number_unsigned())
            return std::nullopt;
        return M18MasterWavPreference{
            M18MasterWavEncoding::PCM24_TPDF,
            j.at(kPcm24Seed).get<std::uint64_t>()};
    }
    if(format=="IEEE_FLOAT32"&&!j.contains(kPcm24Seed))
        return M18MasterWavPreference{
            M18MasterWavEncoding::IEEE_FLOAT32,std::nullopt};
    return std::nullopt;
}
} // namespace

bool M18MasterWavPreferenceCodec::valid(
    const M18MasterWavPreference& choice) noexcept
{
    return (choice.encoding==M18MasterWavEncoding::PCM24_TPDF
            &&choice.pcm24_dither_seed.has_value())
        ||(choice.encoding==M18MasterWavEncoding::IEEE_FLOAT32
            &&!choice.pcm24_dither_seed.has_value());
}

core::Result<std::optional<M18MasterWavPreference>>
M18MasterWavPreferenceCodec::read(const OpaqueJsonValue& output)
{
    using R=core::Result<std::optional<M18MasterWavPreference>>;
    try {
        const auto j=Json::parse(output.canonical_utf8());
        if(!j.is_object())return R::failure(invalid());
        if(!j.contains(kEncoding)&&!j.contains(kPcm24Seed))
            return R::success(std::nullopt);
        const auto selected=decode(j);
        if(!selected)return R::failure(invalid());
        return R::success(std::optional{*selected});
    }catch(const Json::exception&) {
        return R::failure(invalid());
    }
}

core::Result<OpaqueJsonValue>
M18MasterWavPreferenceCodec::with_choice(
    const OpaqueJsonValue& existing,
    const M18MasterWavPreference& choice)
{
    using R=core::Result<OpaqueJsonValue>;
    if(!valid(choice))return R::failure(invalid());
    try {
        auto j=Json::parse(existing.canonical_utf8());
        if(!j.is_object())return R::failure(invalid());
        if((j.contains(kEncoding)||j.contains(kPcm24Seed))
           &&!decode(j))return R::failure(invalid());
        if(choice.encoding==M18MasterWavEncoding::PCM24_TPDF) {
            j[kEncoding]="PCM24_TPDF";
            j[kPcm24Seed]=*choice.pcm24_dither_seed;
        }else {
            j[kEncoding]="IEEE_FLOAT32";
            j.erase(kPcm24Seed);
        }
        return OpaqueJsonValue::parse(j.dump());
    }catch(const Json::exception&) {
        return R::failure(invalid());
    }
}
} // namespace rgsml::project
