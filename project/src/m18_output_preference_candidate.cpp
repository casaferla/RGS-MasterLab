#include <rgsml/project/m18_output_preference_candidate.hpp>
#include <rgsml/core/error.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>

namespace rgsml::project {
namespace {
using Json=nlohmann::json;
// These deliberately provisional names are NOT approved project keys.
// No application / project load-save path may consume them yet.
constexpr const char* kEncoding="m18DraftTargetEncoding";
constexpr const char* kSeed="m18DraftDitherSeed";

[[nodiscard]] core::Error invalid() {
    return core::Error{core::ErrorCode::InvalidArgument,
        "M18 draft output preference invalid or incomplete."};
}

[[nodiscard]] bool recognized_encoding(const Json& x) {
    return x.is_string() && (x=="PCM24" ||x=="FLOAT32");
}

[[nodiscard]] bool recognized_seed(const Json& x) {
    // nlohmann represents nonnegative JSON integers as uint64 exactly.
    // Reject decimals (including 1.0), negative values and any double
    // fallback beyond uint64 max: never coerce through IEEE binary64.
    return x.is_number_unsigned();
}

[[nodiscard]] bool valid_selection(
    const M18OutputPreferenceCandidate& x) noexcept
{
    return (x.encoding==M18OutputEncodingCandidate::PCM24 &&
            x.dither_seed.has_value())
        || (x.encoding==M18OutputEncodingCandidate::IEEE_FLOAT32 &&
            !x.dither_seed.has_value());
}

[[nodiscard]] std::optional<M18OutputPreferenceCandidate> decode(
    const Json& j)
{
    if (!j.is_object())return std::nullopt;
    if (!j.contains(kEncoding))
        return std::nullopt;
    const auto& e=j.at(kEncoding);
    if (!recognized_encoding(e))return std::nullopt;
    if (e=="PCM24") {
        if (!j.contains(kSeed) ||!recognized_seed(j.at(kSeed)))
            return std::nullopt;
        return M18OutputPreferenceCandidate{
            M18OutputEncodingCandidate::PCM24,
            j.at(kSeed).get<std::uint64_t>()};
    }
    if (j.contains(kSeed))return std::nullopt;
    return M18OutputPreferenceCandidate{
        M18OutputEncodingCandidate::IEEE_FLOAT32,std::nullopt};
}
} // namespace

core::Result<std::optional<M18OutputPreferenceCandidate>>
M18OutputPreferenceCandidateCodec::read_candidate(
    const OpaqueJsonValue& output)
{
    using Return=core::Result<std::optional<M18OutputPreferenceCandidate>>;
    try {
        const auto j=Json::parse(output.canonical_utf8());
        if (!j.is_object())return Return::failure(invalid());
        if (!j.contains(kEncoding) &&!j.contains(kSeed))
            return Return::success(std::nullopt);
        const auto parsed=decode(j);
        if (!parsed)return Return::failure(invalid());
        return Return::success(std::optional{*parsed});
    } catch (const Json::exception&) {
        return Return::failure(invalid());
    }
}

core::Result<OpaqueJsonValue>
M18OutputPreferenceCandidateCodec::with_candidate(
    const OpaqueJsonValue& existing,
    const M18OutputPreferenceCandidate& selection)
{
    if (!valid_selection(selection))
        return core::Result<OpaqueJsonValue>::failure(invalid());
    try {
        auto j=Json::parse(existing.canonical_utf8());
        if (!j.is_object())
            return core::Result<OpaqueJsonValue>::failure(invalid());
        // Reject malformed historical draft, never silently overwrite it.
        if (j.contains(kEncoding)||j.contains(kSeed)) {
            if (!decode(j))
                return core::Result<OpaqueJsonValue>::failure(invalid());
        }
        j[kEncoding]=selection.encoding==M18OutputEncodingCandidate::PCM24
            ? "PCM24" : "FLOAT32";
        if (selection.encoding==M18OutputEncodingCandidate::PCM24)
            j[kSeed]=*selection.dither_seed;
        else
            j.erase(kSeed); // Float32 MUST NOT inherit a latent PCM24 seed.
        return OpaqueJsonValue::parse(j.dump());
    } catch (const Json::exception&) {
        return core::Result<OpaqueJsonValue>::failure(invalid());
    }
}
} // namespace rgsml::project
