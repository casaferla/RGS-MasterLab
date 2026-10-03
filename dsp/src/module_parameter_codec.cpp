#include <rgsml/dsp/module_parameter_codec.hpp>

#include <rgsml/core/error.hpp>
#include <rgsml/core/uuid.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rgsml::dsp {
namespace {

using nlohmann::json;
using rgsml::core::Error;
using rgsml::core::ErrorCode;
using rgsml::core::Result;

constexpr auto kGainSchemaId = "rgsml.dsp.gain.parameters/1.0.0";
constexpr auto kEqSchemaId = "rgsml.dsp.parametric-eq.parameters/1.0.0";
constexpr auto kCompressorSchemaId = "rgsml.dsp.compressor.parameters/1.0.0";

[[nodiscard]] Error codec_error(ErrorCode code, std::string category, std::string message)
{
    return Error{code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] std::string filter_type_to_string(EqFilterType type)
{
    switch (type) {
    case EqFilterType::BELL: return "BELL";
    case EqFilterType::NOTCH: return "NOTCH";
    case EqFilterType::LOW_SHELF: return "LOW_SHELF";
    case EqFilterType::HIGH_SHELF: return "HIGH_SHELF";
    case EqFilterType::HIGH_PASS: return "HIGH_PASS";
    case EqFilterType::LOW_PASS: return "LOW_PASS";
    }
    return "BELL";
}

[[nodiscard]] Result<EqFilterType> string_to_filter_type(std::string_view str)
{
    if (str == "BELL") return Result<EqFilterType>::success(EqFilterType::BELL);
    if (str == "NOTCH") return Result<EqFilterType>::success(EqFilterType::NOTCH);
    if (str == "LOW_SHELF") return Result<EqFilterType>::success(EqFilterType::LOW_SHELF);
    if (str == "HIGH_SHELF") return Result<EqFilterType>::success(EqFilterType::HIGH_SHELF);
    if (str == "HIGH_PASS") return Result<EqFilterType>::success(EqFilterType::HIGH_PASS);
    if (str == "LOW_PASS") return Result<EqFilterType>::success(EqFilterType::LOW_PASS);
    return Result<EqFilterType>::failure(codec_error(
        ErrorCode::InvalidArgument,
        "INVALID_FILTER_TYPE",
        "Unrecognized filter type string in JSON."));
}

[[nodiscard]] std::string routing_to_string(EqRouting routing)
{
    switch (routing) {
    case EqRouting::STEREO: return "STEREO";
    case EqRouting::MID: return "MID";
    case EqRouting::SIDE: return "SIDE";
    case EqRouting::LEFT: return "LEFT";
    case EqRouting::RIGHT: return "RIGHT";
    }
    return "STEREO";
}

[[nodiscard]] Result<EqRouting> string_to_routing(std::string_view str)
{
    if (str == "STEREO") return Result<EqRouting>::success(EqRouting::STEREO);
    if (str == "MID") return Result<EqRouting>::success(EqRouting::MID);
    if (str == "SIDE") return Result<EqRouting>::success(EqRouting::SIDE);
    if (str == "LEFT") return Result<EqRouting>::success(EqRouting::LEFT);
    if (str == "RIGHT") return Result<EqRouting>::success(EqRouting::RIGHT);
    return Result<EqRouting>::failure(codec_error(
        ErrorCode::InvalidArgument,
        "INVALID_ROUTING",
        "Unrecognized routing string in JSON."));
}

[[nodiscard]] Result<SlopeDbPerOctave> integer_to_slope(std::int64_t val)
{
    switch (val) {
    case 6: return Result<SlopeDbPerOctave>::success(SlopeDbPerOctave::DB_6);
    case 12: return Result<SlopeDbPerOctave>::success(SlopeDbPerOctave::DB_12);
    case 18: return Result<SlopeDbPerOctave>::success(SlopeDbPerOctave::DB_18);
    case 24: return Result<SlopeDbPerOctave>::success(SlopeDbPerOctave::DB_24);
    case 36: return Result<SlopeDbPerOctave>::success(SlopeDbPerOctave::DB_36);
    case 48: return Result<SlopeDbPerOctave>::success(SlopeDbPerOctave::DB_48);
    default:
        return Result<SlopeDbPerOctave>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_SLOPE",
            "Slope value in JSON must be one of {6, 12, 18, 24, 36, 48}."));
    }
}

}  // namespace

Result<std::string> encode_gain_parameters_json(const GainParameters& params)
{
    json j;
    const double g = params.gain_db();
    j["gainDb"] = (g == 0.0 ? 0.0 : g);
    return Result<std::string>::success(j.dump());
}

Result<GainParameters> decode_gain_parameters_json(std::string_view json_text)
{
    json j = json::parse(json_text, nullptr, false);
    if (j.is_discarded()) {
        return Result<GainParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_JSON_SYNTAX",
            "Failed to parse Gain parameters JSON text."));
    }
    if (!j.is_object()) {
        return Result<GainParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_JSON_STRUCTURE",
            "Gain parameters JSON root must be an object."));
    }
    if (j.size() != 1U || !j.contains("gainDb")) {
        return Result<GainParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "STRICT_SCHEMA_VIOLATION",
            "Gain parameters JSON object must contain exactly the 'gainDb' key."));
    }
    const auto& gain_val = j["gainDb"];
    if (!gain_val.is_number()) {
        return Result<GainParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_PARAMETER_TYPE",
            "'gainDb' field must be a binary64 number."));
    }

    const double raw_gain = gain_val.get<double>();
    return GainParameters::create(raw_gain);
}

Result<std::string> encode_parametric_eq_parameters_json(const ParametricEqParameters& params)
{
    json bands_array = json::array();
    for (const auto& band : params.bands()) {
        json b;
        b["bandId"] = band.band_id().to_string();
        b["enabled"] = band.enabled();
        b["filterType"] = filter_type_to_string(band.filter_type());
        b["routing"] = routing_to_string(band.routing());

        std::visit(
            [&b](const auto& payload) {
                using T = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<T, BellPayload>) {
                    b["frequencyHz"] = payload.frequency_hz;
                    b["gainDb"] = payload.gain_db == 0.0 ? 0.0 : payload.gain_db;
                    b["q"] = payload.q;
                } else if constexpr (std::is_same_v<T, NotchPayload>) {
                    b["frequencyHz"] = payload.frequency_hz;
                    b["q"] = payload.q;
                } else if constexpr (std::is_same_v<T, ShelfPayload>) {
                    b["frequencyHz"] = payload.frequency_hz;
                    b["gainDb"] = payload.gain_db == 0.0 ? 0.0 : payload.gain_db;
                    b["shelfSlope"] = payload.shelf_slope;
                } else if constexpr (std::is_same_v<T, PassPayload>) {
                    b["frequencyHz"] = payload.frequency_hz;
                    b["slopeDbPerOctave"] = static_cast<std::uint16_t>(payload.slope_db_per_octave);
                }
            },
            band.payload());

        bands_array.push_back(std::move(b));
    }

    json root;
    root["bands"] = std::move(bands_array);
    return Result<std::string>::success(root.dump());
}

Result<ParametricEqParameters> decode_parametric_eq_parameters_json(std::string_view json_text)
{
    json root = json::parse(json_text, nullptr, false);
    if (root.is_discarded()) {
        return Result<ParametricEqParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_JSON_SYNTAX",
            "Failed to parse Parametric EQ parameters JSON text."));
    }
    if (!root.is_object()) {
        return Result<ParametricEqParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_JSON_STRUCTURE",
            "Parametric EQ parameters JSON root must be an object."));
    }
    if (root.size() != 1U || !root.contains("bands")) {
        return Result<ParametricEqParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "STRICT_SCHEMA_VIOLATION",
            "Parametric EQ parameters JSON object must contain exactly the 'bands' key."));
    }

    const auto& bands_val = root["bands"];
    if (!bands_val.is_array()) {
        return Result<ParametricEqParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_PARAMETER_TYPE",
            "'bands' field must be a JSON array."));
    }

    if (bands_val.empty() || bands_val.size() > 6U) {
        return Result<ParametricEqParameters>::failure(codec_error(
            ErrorCode::OutOfRange,
            "INVALID_BAND_COUNT",
            "Parametric EQ bands array must contain between 1 and 6 elements."));
    }

    std::vector<EqBandParameters> bands;
    bands.reserve(bands_val.size());

    for (const auto& band_json : bands_val) {
        if (!band_json.is_object()) {
            return Result<ParametricEqParameters>::failure(codec_error(
                ErrorCode::InvalidArgument,
                "INVALID_JSON_STRUCTURE",
                "Each band in 'bands' array must be a JSON object."));
        }

        if (!band_json.contains("bandId") || !band_json.contains("enabled")
            || !band_json.contains("filterType") || !band_json.contains("routing")) {
            return Result<ParametricEqParameters>::failure(codec_error(
                ErrorCode::InvalidArgument,
                "STRICT_SCHEMA_VIOLATION",
                "Band JSON object is missing required common fields."));
        }

        if (!band_json["bandId"].is_string() || !band_json["enabled"].is_boolean()
            || !band_json["filterType"].is_string() || !band_json["routing"].is_string()) {
            return Result<ParametricEqParameters>::failure(codec_error(
                ErrorCode::InvalidArgument,
                "INVALID_PARAMETER_TYPE",
                "Band common fields contain invalid types."));
        }

        auto band_id_res = rgsml::core::Uuid::parse(band_json["bandId"].get<std::string_view>());
        if (!band_id_res) {
            return Result<ParametricEqParameters>::failure(codec_error(
                ErrorCode::InvalidArgument,
                "INVALID_UUID",
                "bandId is not a valid UUID."));
        }
        const auto band_id = *band_id_res.value();
        const bool enabled = band_json["enabled"].get<bool>();

        auto filter_res = string_to_filter_type(band_json["filterType"].get<std::string_view>());
        if (!filter_res) {
            return Result<ParametricEqParameters>::failure(*filter_res.error());
        }
        const auto filter_type = *filter_res.value();

        auto routing_res = string_to_routing(band_json["routing"].get<std::string_view>());
        if (!routing_res) {
            return Result<ParametricEqParameters>::failure(*routing_res.error());
        }
        const auto routing = *routing_res.value();

        EqBandPayload payload;

        switch (filter_type) {
        case EqFilterType::BELL: {
            if (band_json.size() != 7U || !band_json.contains("frequencyHz")
                || !band_json.contains("gainDb") || !band_json.contains("q")) {
                return Result<ParametricEqParameters>::failure(codec_error(
                    ErrorCode::InvalidArgument,
                    "STRICT_SCHEMA_VIOLATION",
                    "BELL band JSON object must contain exactly {bandId, enabled, filterType, routing, frequencyHz, gainDb, q}."));
            }
            if (!band_json["frequencyHz"].is_number() || !band_json["gainDb"].is_number()
                || !band_json["q"].is_number()) {
                return Result<ParametricEqParameters>::failure(codec_error(
                    ErrorCode::InvalidArgument,
                    "INVALID_PARAMETER_TYPE",
                    "BELL band numeric fields must be numbers."));
            }
            payload = BellPayload{
                .frequency_hz = band_json["frequencyHz"].get<double>(),
                .gain_db = band_json["gainDb"].get<double>(),
                .q = band_json["q"].get<double>()};
            break;
        }
        case EqFilterType::NOTCH: {
            if (band_json.size() != 6U || !band_json.contains("frequencyHz")
                || !band_json.contains("q")) {
                return Result<ParametricEqParameters>::failure(codec_error(
                    ErrorCode::InvalidArgument,
                    "STRICT_SCHEMA_VIOLATION",
                    "NOTCH band JSON object must contain exactly {bandId, enabled, filterType, routing, frequencyHz, q}."));
            }
            if (!band_json["frequencyHz"].is_number() || !band_json["q"].is_number()) {
                return Result<ParametricEqParameters>::failure(codec_error(
                    ErrorCode::InvalidArgument,
                    "INVALID_PARAMETER_TYPE",
                    "NOTCH band numeric fields must be numbers."));
            }
            payload = NotchPayload{
                .frequency_hz = band_json["frequencyHz"].get<double>(),
                .q = band_json["q"].get<double>()};
            break;
        }
        case EqFilterType::LOW_SHELF:
        case EqFilterType::HIGH_SHELF: {
            if (band_json.size() != 7U || !band_json.contains("frequencyHz")
                || !band_json.contains("gainDb") || !band_json.contains("shelfSlope")) {
                return Result<ParametricEqParameters>::failure(codec_error(
                    ErrorCode::InvalidArgument,
                    "STRICT_SCHEMA_VIOLATION",
                    "SHELF band JSON object must contain exactly {bandId, enabled, filterType, routing, frequencyHz, gainDb, shelfSlope}."));
            }
            if (!band_json["frequencyHz"].is_number() || !band_json["gainDb"].is_number()
                || !band_json["shelfSlope"].is_number()) {
                return Result<ParametricEqParameters>::failure(codec_error(
                    ErrorCode::InvalidArgument,
                    "INVALID_PARAMETER_TYPE",
                    "SHELF band numeric fields must be numbers."));
            }
            payload = ShelfPayload{
                .frequency_hz = band_json["frequencyHz"].get<double>(),
                .gain_db = band_json["gainDb"].get<double>(),
                .shelf_slope = band_json["shelfSlope"].get<double>()};
            break;
        }
        case EqFilterType::HIGH_PASS:
        case EqFilterType::LOW_PASS: {
            if (band_json.size() != 6U || !band_json.contains("frequencyHz")
                || !band_json.contains("slopeDbPerOctave")) {
                return Result<ParametricEqParameters>::failure(codec_error(
                    ErrorCode::InvalidArgument,
                    "STRICT_SCHEMA_VIOLATION",
                    "PASS band JSON object must contain exactly {bandId, enabled, filterType, routing, frequencyHz, slopeDbPerOctave}."));
            }
            if (!band_json["frequencyHz"].is_number() || !band_json["slopeDbPerOctave"].is_number_integer()) {
                return Result<ParametricEqParameters>::failure(codec_error(
                    ErrorCode::InvalidArgument,
                    "INVALID_PARAMETER_TYPE",
                    "PASS band frequency must be number and slopeDbPerOctave must be integer."));
            }
            auto slope_res = integer_to_slope(band_json["slopeDbPerOctave"].get<std::int64_t>());
            if (!slope_res) {
                return Result<ParametricEqParameters>::failure(*slope_res.error());
            }
            payload = PassPayload{
                .frequency_hz = band_json["frequencyHz"].get<double>(),
                .slope_db_per_octave = *slope_res.value()};
            break;
        }
        }

        auto band_param_res = EqBandParameters::create(
            band_id, enabled, filter_type, routing, std::move(payload));
        if (!band_param_res) {
            return Result<ParametricEqParameters>::failure(*band_param_res.error());
        }

        bands.push_back(std::move(*band_param_res.value()));
    }

    return ParametricEqParameters::create(std::move(bands));
}

Result<std::string> encode_compressor_parameters_json(const CompressorParameters& params)
{
    json j;
    j["detectorMode"] = (params.detector_mode() == CompressorDetectorMode::PEAK) ? "PEAK" : "RMS";
    switch (params.channel_link()) {
    case CompressorChannelLink::LINKED_MAX: j["channelLink"] = "LINKED_MAX"; break;
    case CompressorChannelLink::LINKED_MEAN: j["channelLink"] = "LINKED_MEAN"; break;
    case CompressorChannelLink::DUAL_MONO: j["channelLink"] = "DUAL_MONO"; break;
    }
    j["thresholdDbfs"] = (params.threshold_dbfs() == 0.0 ? 0.0 : params.threshold_dbfs());
    j["ratio"] = params.ratio();
    j["kneeDb"] = (params.knee_db() == 0.0 ? 0.0 : params.knee_db());
    j["attackMs"] = params.attack_ms();
    j["releaseMs"] = params.release_ms();
    j["rmsTimeConstantMs"] = params.rms_time_constant_ms();
    j["lookAheadMs"] = (params.look_ahead_ms() == 0.0 ? 0.0 : params.look_ahead_ms());
    j["mixPercent"] = (params.mix_percent() == 0.0 ? 0.0 : params.mix_percent());
    j["makeupGainDb"] = (params.makeup_gain_db() == 0.0 ? 0.0 : params.makeup_gain_db());
    return Result<std::string>::success(j.dump());
}

Result<CompressorParameters> decode_compressor_parameters_json(std::string_view json_text)
{
    json j = json::parse(json_text, nullptr, false);
    if (j.is_discarded()) {
        return Result<CompressorParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_JSON_SYNTAX",
            "Failed to parse Compressor parameters JSON text."));
    }
    if (!j.is_object()) {
        return Result<CompressorParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_JSON_STRUCTURE",
            "Compressor parameters JSON root must be an object."));
    }
    if (j.size() != 11U
        || !j.contains("detectorMode")
        || !j.contains("channelLink")
        || !j.contains("thresholdDbfs")
        || !j.contains("ratio")
        || !j.contains("kneeDb")
        || !j.contains("attackMs")
        || !j.contains("releaseMs")
        || !j.contains("rmsTimeConstantMs")
        || !j.contains("lookAheadMs")
        || !j.contains("mixPercent")
        || !j.contains("makeupGainDb")) {
        return Result<CompressorParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "STRICT_SCHEMA_VIOLATION",
            "Compressor parameters JSON object must contain exactly the 11 required keys."));
    }

    if (!j["detectorMode"].is_string() || !j["channelLink"].is_string()
        || !j["thresholdDbfs"].is_number() || !j["ratio"].is_number()
        || !j["kneeDb"].is_number() || !j["attackMs"].is_number()
        || !j["releaseMs"].is_number() || !j["rmsTimeConstantMs"].is_number()
        || !j["lookAheadMs"].is_number() || !j["mixPercent"].is_number()
        || !j["makeupGainDb"].is_number()) {
        return Result<CompressorParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_PARAMETER_TYPE",
            "Compressor parameters contained an invalid field type."));
    }

    const auto det_str = j["detectorMode"].get<std::string_view>();
    CompressorDetectorMode det_mode = CompressorDetectorMode::RMS;
    if (det_str == "PEAK") {
        det_mode = CompressorDetectorMode::PEAK;
    } else if (det_str == "RMS") {
        det_mode = CompressorDetectorMode::RMS;
    } else {
        return Result<CompressorParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_COMPRESSOR_PARAMETER",
            "detectorMode string is unrecognized."));
    }

    const auto link_str = j["channelLink"].get<std::string_view>();
    CompressorChannelLink channel_link = CompressorChannelLink::LINKED_MAX;
    if (link_str == "LINKED_MAX") {
        channel_link = CompressorChannelLink::LINKED_MAX;
    } else if (link_str == "LINKED_MEAN") {
        channel_link = CompressorChannelLink::LINKED_MEAN;
    } else if (link_str == "DUAL_MONO") {
        channel_link = CompressorChannelLink::DUAL_MONO;
    } else {
        return Result<CompressorParameters>::failure(codec_error(
            ErrorCode::InvalidArgument,
            "INVALID_COMPRESSOR_PARAMETER",
            "channelLink string is unrecognized."));
    }

    return CompressorParameters::create(
        det_mode,
        channel_link,
        j["thresholdDbfs"].get<double>(),
        j["ratio"].get<double>(),
        j["kneeDb"].get<double>(),
        j["attackMs"].get<double>(),
        j["releaseMs"].get<double>(),
        j["rmsTimeConstantMs"].get<double>(),
        j["lookAheadMs"].get<double>(),
        j["mixPercent"].get<double>(),
        j["makeupGainDb"].get<double>());
}

Result<std::string> encode_module_parameters_json(const ModuleParameterPayload& payload)
{
    return std::visit(
        [](const auto& params) {
            using T = std::decay_t<decltype(params)>;
            if constexpr (std::is_same_v<T, GainParameters>) {
                return encode_gain_parameters_json(params);
            } else if constexpr (std::is_same_v<T, ParametricEqParameters>) {
                return encode_parametric_eq_parameters_json(params);
            } else if constexpr (std::is_same_v<T, CompressorParameters>) {
                return encode_compressor_parameters_json(params);
            }
        },
        payload);
}

Result<ModuleParameterPayload> decode_module_parameters_json(
    std::string_view schema_id,
    std::string_view json_text)
{
    if (schema_id == kGainSchemaId) {
        auto gain_res = decode_gain_parameters_json(json_text);
        if (!gain_res) {
            return Result<ModuleParameterPayload>::failure(*gain_res.error());
        }
        return Result<ModuleParameterPayload>::success(ModuleParameterPayload{std::move(*gain_res.value())});
    }
    if (schema_id == kEqSchemaId) {
        auto eq_res = decode_parametric_eq_parameters_json(json_text);
        if (!eq_res) {
            return Result<ModuleParameterPayload>::failure(*eq_res.error());
        }
        return Result<ModuleParameterPayload>::success(ModuleParameterPayload{std::move(*eq_res.value())});
    }
    if (schema_id == kCompressorSchemaId) {
        auto comp_res = decode_compressor_parameters_json(json_text);
        if (!comp_res) {
            return Result<ModuleParameterPayload>::failure(*comp_res.error());
        }
        return Result<ModuleParameterPayload>::success(ModuleParameterPayload{std::move(*comp_res.value())});
    }

    return Result<ModuleParameterPayload>::failure(codec_error(
        ErrorCode::InvalidArgument,
        "UNSUPPORTED_PARAMETER_SCHEMA",
        "Parameter schema ID is unrecognized or unsupported by codec."));
}

}  // namespace rgsml::dsp
