#include <nlohmann/json.hpp>

#include <QTest>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {

using Json = nlohmann::json;

// Private qualification profile. No project schema or production JSON API.
class BoundedSax final : public nlohmann::json_sax<Json> {
public:
    bool null() override { return event(); }
    bool boolean(bool) override { return event(); }
    bool number_integer(number_integer_t) override { return event(); }
    bool number_unsigned(number_unsigned_t) override { return event(); }
    bool number_float(number_float_t value, const string_t& raw) override
    {
        // The library may convert an overflowing integer token to double.
        // Do not accept that implicit integer-to-float coercion.
        return std::isfinite(value)
            && raw.find_first_of(".eE") != std::string::npos && event();
    }
    bool string(string_t& value) override
    {
        return value.size() <= kMaxBytes && event();
    }
    bool binary(binary_t&) override { return false; }
    bool start_object(std::size_t) override
    {
        if (!event() || stack_.size() >= kMaxDepth) {
            return false;
        }
        stack_.emplace_back(std::unordered_set<std::string>{});
        return true;
    }
    bool key(string_t& value) override
    {
        return !stack_.empty() && stack_.back().has_value()
            && value.size() <= kMaxBytes
            && stack_.back()->insert(value).second && event();
    }
    bool end_object() override
    {
        if (stack_.empty() || !stack_.back()) {
            return false;
        }
        stack_.pop_back();
        return true;
    }
    bool start_array(std::size_t) override
    {
        if (!event() || stack_.size() >= kMaxDepth) {
            return false;
        }
        stack_.emplace_back(std::nullopt);
        return true;
    }
    bool end_array() override
    {
        if (stack_.empty() || stack_.back()) {
            return false;
        }
        stack_.pop_back();
        return true;
    }
    bool parse_error(std::size_t, const std::string&,
                     const nlohmann::detail::exception&) override
    {
        return false;
    }

    static constexpr std::size_t kMaxBytes = 1024U * 1024U;
    static constexpr std::size_t kMaxDepth = 64U;
    static constexpr std::size_t kMaxEvents = 100'000U;

private:
    bool event() { return ++events_ <= kMaxEvents; }
    std::size_t events_{0};
    std::vector<std::optional<std::unordered_set<std::string>>> stack_;
};

std::optional<Json> parse_strict(std::string_view input)
{
    if (input.size() > BoundedSax::kMaxBytes
        || (input.size() >= 3U && input.substr(0, 3) == "\xEF\xBB\xBF")) {
        return std::nullopt;
    }
    BoundedSax prepass;
    if (!Json::sax_parse(input.begin(), input.end(), &prepass,
                         Json::input_format_t::json, true, false)) {
        return std::nullopt;
    }
    try {
        // Second pass is reachable only after RGSML's bounded SAX success.
        return Json::parse(input.begin(), input.end(), nullptr, true, false);
    } catch (const Json::exception&) {
        return std::nullopt;
    }
}

bool finite_json(const Json& value)
{
    if (value.is_number_float()) {
        return std::isfinite(value.get<double>());
    }
    if (value.is_array()) {
        for (const auto& element : value) {
            if (!finite_json(element)) {
                return false;
            }
        }
    } else if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (!finite_json(it.value())) {
                return false;
            }
        }
    }
    return true;
}

std::optional<std::string> serialize_strict(const Json& value)
{
    if (!finite_json(value)) {
        return std::nullopt;
    }
    return value.dump(-1, ' ', false, Json::error_handler_t::strict);
}

class StrictJsonSpikeTest final : public QObject {
    Q_OBJECT
private slots:
    void syntaxAndBounds();
    void integerAndFloatTypes();
    void canonicalSerialization();
};

void StrictJsonSpikeTest::syntaxAndBounds()
{
    QVERIFY(parse_strict(R"({"valid":[true,null,"é"]})"));
    QVERIFY(!parse_strict(R"({"a":1,"a":2})"));
    QVERIFY(!parse_strict(R"({"outer":{"x":1,"x":2}})"));
    QVERIFY(!parse_strict("{\"a\":1/* comment */}"));
    QVERIFY(!parse_strict("[1,]"));
    QVERIFY(!parse_strict("{} garbage"));
    QVERIFY(!parse_strict(std::string{"\x22\xC3\x28\x22", 4}));
    QVERIFY(parse_strict(std::string(64, '[') + "0" + std::string(64, ']')));
    QVERIFY(!parse_strict(std::string(65, '[') + "0" + std::string(65, ']')));
    QVERIFY(!parse_strict(std::string(BoundedSax::kMaxBytes + 1, ' ')));
    QVERIFY(!parse_strict("\xEF\xBB\xBF{}"));
}

void StrictJsonSpikeTest::integerAndFloatTypes()
{
    const auto signedMin = parse_strict("-9223372036854775808");
    QVERIFY(signedMin);
    QVERIFY(signedMin->is_number_integer());
    QCOMPARE(signedMin->get<std::int64_t>(), std::numeric_limits<std::int64_t>::min());
    const auto signedMax = parse_strict("9223372036854775807");
    QVERIFY(signedMax && signedMax->is_number_integer());
    QCOMPARE(signedMax->get<std::int64_t>(), std::numeric_limits<std::int64_t>::max());
    const auto unsignedMax = parse_strict("18446744073709551615");
    QVERIFY(unsignedMax);
    QVERIFY(unsignedMax->is_number_unsigned());
    QCOMPARE(unsignedMax->get<std::uint64_t>(), std::numeric_limits<std::uint64_t>::max());
    QVERIFY(!parse_strict("18446744073709551616"));
    QVERIFY(!parse_strict("-9223372036854775809"));
    QVERIFY(!parse_strict("1e309"));
    const auto integer = parse_strict("1");
    const auto floating = parse_strict("1.0");
    QVERIFY(integer && floating);
    QVERIFY(integer->is_number_integer() && !integer->is_number_float());
    QVERIFY(floating->is_number_float() && !floating->is_number_integer());
    constexpr auto difficult = std::bit_cast<double>(std::uint64_t{0x3fd5555555555555ULL});
    const auto encoded = serialize_strict(Json(difficult));
    QVERIFY(encoded);
    const auto decoded = parse_strict(*encoded);
    QVERIFY(decoded && decoded->is_number_float());
    QCOMPARE(std::bit_cast<std::uint64_t>(decoded->get<double>()),
             std::bit_cast<std::uint64_t>(difficult));
}

void StrictJsonSpikeTest::canonicalSerialization()
{
    Json value = {{"z", Json::array({3, 1, 2})}, {"a", "é"}};
    const auto encoded = serialize_strict(value);
    QVERIFY(encoded);
    QCOMPARE(*encoded, std::string(R"({"a":"é","z":[3,1,2]})"));
    QVERIFY(encoded->find('\n') == std::string::npos);
    QVERIFY(encoded->compare(0, 3, "\xEF\xBB\xBF") != 0);
    QVERIFY(!serialize_strict(Json(std::numeric_limits<double>::quiet_NaN())));
    QVERIFY(!serialize_strict(Json(std::numeric_limits<double>::infinity())));
    QVERIFY(!serialize_strict(Json(-std::numeric_limits<double>::infinity())));
}

}  // namespace

QTEST_APPLESS_MAIN(StrictJsonSpikeTest)
#include "test_strict_json.moc"
