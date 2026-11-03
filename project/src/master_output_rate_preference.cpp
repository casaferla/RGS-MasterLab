#include <rgsml/project/master_output_rate_preference.hpp>
#include <rgsml/core/error.hpp>
#include <nlohmann/json.hpp>

namespace rgsml::project {
namespace {
using Json=nlohmann::json;
constexpr const char* kTarget="targetMasterOutputRateHz";
[[nodiscard]] core::Error invalid_rate() {
    return core::Error{core::ErrorCode::InvalidArgument,
                       "Invalid Project/Render target master sample rate."};
}
[[nodiscard]] bool recognized(const Json& v) {
    if(!v.is_number_integer())return false;
    for(const auto rate:{44100,48000,88200,96000,176400,192000})
        if(v==rate)return true;
    return false;
}
} // namespace

bool MasterOutputRatePreference::supported(std::int64_t hz) noexcept {
    return hz==44100||hz==48000||hz==88200
        ||hz==96000||hz==176400||hz==192000;
}
core::Result<std::optional<std::int64_t>>
MasterOutputRatePreference::read(const OpaqueJsonValue& output) {
    try {
        const auto j=Json::parse(output.canonical_utf8());
        if(!j.is_object())
            return core::Result<std::optional<std::int64_t>>::failure(invalid_rate());
        if(!j.contains(kTarget))
            return core::Result<std::optional<std::int64_t>>::success(std::nullopt);
        const auto& v=j.at(kTarget);
        if(!recognized(v))
            return core::Result<std::optional<std::int64_t>>::failure(invalid_rate());
        return core::Result<std::optional<std::int64_t>>::success(
            std::optional<std::int64_t>{v.get<std::int64_t>()});
    } catch(const Json::exception&) {
        return core::Result<std::optional<std::int64_t>>::failure(invalid_rate());
    }
}
core::Result<OpaqueJsonValue>
MasterOutputRatePreference::with_target(
    const OpaqueJsonValue& existing,std::int64_t hz) {
    if(!supported(hz))
        return core::Result<OpaqueJsonValue>::failure(invalid_rate());
    try {
        auto j=Json::parse(existing.canonical_utf8());
        if(!j.is_object()||(j.contains(kTarget)&&!recognized(j.at(kTarget))))
            return core::Result<OpaqueJsonValue>::failure(invalid_rate());
        j[kTarget]=hz; // preserve independent unknown / future output keys
        return OpaqueJsonValue::parse(j.dump());
    } catch(const Json::exception&) {
        return core::Result<OpaqueJsonValue>::failure(invalid_rate());
    }
}
bool MasterOutputRatePreference::only_known_fields(
    const OpaqueJsonValue& output) {
    try {
        const auto j=Json::parse(output.canonical_utf8());
        if(!j.is_object())return false;
        return j.empty() || (j.size()==1 && j.contains(kTarget)
                             && recognized(j.at(kTarget)));
    } catch(const Json::exception&) {
        return false;
    }
}
} // namespace rgsml::project
