#include <rgsml/core/checked_integer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/processing_state.hpp>
#include <rgsml/core/rational.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/strong_id.hpp>
#include <rgsml/core/uuid.hpp>

#include <QtTest/QTest>

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::core;

struct FirstIdTag final {
};
struct SecondIdTag final {
};

static_assert(!std::is_default_constructible_v<Result<int>>);
static_assert(!std::is_default_constructible_v<Status>);
static_assert(!std::is_default_constructible_v<Error>);
static_assert(std::is_move_constructible_v<Result<std::unique_ptr<int>>>);
static_assert(!std::is_copy_constructible_v<Result<std::unique_ptr<int>>>);
static_assert(!std::is_default_constructible_v<StrongId<FirstIdTag>>);
static_assert(!std::is_convertible_v<StrongId<FirstIdTag>, StrongId<SecondIdTag>>);
static_assert(!std::is_convertible_v<FrameIndex, FrameCount>);
static_assert(!std::is_convertible_v<FrameCount, FrameIndex>);
static_assert(!std::is_convertible_v<SampleRate, FrameIndex>);
static_assert(!std::is_convertible_v<RationalTime, Rational>);

void verifyResultAndError()
{
    auto success = Result<int>::success(42);
    QVERIFY(success.has_value());
    QVERIFY(success.value() != nullptr);
    QCOMPARE(*success.value(), 42);
    QVERIFY(success.error() == nullptr);

    const Error propagated{
        ErrorCode::OutOfRange,
        "outside",
        {{"first", "one"}, {"second", "two"}}};
    auto failure = Result<int>::failure(propagated);
    QVERIFY(!failure.has_value());
    QVERIFY(failure.value() == nullptr);
    QVERIFY(failure.error() != nullptr);
    QCOMPARE(failure.error()->code(), ErrorCode::OutOfRange);
    QCOMPARE(failure.error()->message(), std::string("outside"));
    QCOMPARE(failure.error()->details().size(), std::size_t{2});
    QCOMPARE(failure.error()->details()[0].key, std::string("first"));
    QCOMPARE(failure.error()->details()[1].key, std::string("second"));

    const auto constFailure = std::as_const(failure);
    QVERIFY(constFailure.value() == nullptr);
    QVERIFY(constFailure.error() != nullptr);
    QCOMPARE(constFailure.error()->code(), ErrorCode::OutOfRange);

    auto moveOnly = Result<std::unique_ptr<int>>::success(std::make_unique<int>(7));
    auto moved = std::move(moveOnly);
    QVERIFY(moved.value() != nullptr);
    QVERIFY(*moved.value() != nullptr);
    QCOMPARE(**moved.value(), 7);

    auto statusSuccess = Status::success();
    QVERIFY(statusSuccess.has_value());
    QVERIFY(statusSuccess.error() == nullptr);
    auto statusFailure = Status::failure(
        Error{ErrorCode::InvalidArgument, "invalid"});
    QVERIFY(!statusFailure.has_value());
    QVERIFY(statusFailure.error() != nullptr);
    QCOMPARE(statusFailure.error()->code(), ErrorCode::InvalidArgument);

    constexpr std::array<std::pair<ErrorCode, std::string_view>, 8> tokens{{
        {ErrorCode::InvalidArgument, "invalid_argument"},
        {ErrorCode::OutOfRange, "out_of_range"},
        {ErrorCode::IntegerOverflow, "integer_overflow"},
        {ErrorCode::DivisionByZero, "division_by_zero"},
        {ErrorCode::ParseFailure, "parse_failure"},
        {ErrorCode::InvalidUuid, "invalid_uuid"},
        {ErrorCode::InvalidRational, "invalid_rational"},
        {ErrorCode::InvalidFrameRange, "invalid_frame_range"},
    }};
    for (const auto& [code, token] : tokens) {
        QCOMPARE(error_code_token(code), token);
        auto parsed = parse_error_code(token);
        QVERIFY(parsed.value() != nullptr);
        QCOMPARE(*parsed.value(), code);
    }
    auto unknown = parse_error_code("INVALID_ARGUMENT");
    QVERIFY(unknown.error() != nullptr);
    QCOMPARE(unknown.error()->code(), ErrorCode::ParseFailure);
}

void verifyUuidAndStrongId()
{
    constexpr std::string_view canonical = "00112233-4455-6677-8899-aabbccddeeff";
    auto parsed = Uuid::parse(canonical);
    QVERIFY(parsed.value() != nullptr);
    QCOMPARE(parsed.value()->to_string(), std::string(canonical));
    QVERIFY(!parsed.value()->is_nil());

    const Uuid::Bytes expectedBytes{
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    QCOMPARE(parsed.value()->bytes(), expectedBytes);
    QCOMPARE(Uuid{expectedBytes}.to_string(), std::string(canonical));
    const auto nextBytes = Uuid::Bytes{
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xef, 0x00};
    QVERIFY(Uuid{expectedBytes} < Uuid{nextBytes});

    auto uppercase = Uuid::parse("00112233-4455-6677-8899-AABBCCDDEEFF");
    QVERIFY(uppercase.value() != nullptr);
    QCOMPARE(uppercase.value()->to_string(), std::string(canonical));

    auto nil = Uuid::parse("00000000-0000-0000-0000-000000000000");
    QVERIFY(nil.value() != nullptr);
    QVERIFY(nil.value()->is_nil());
    const Uuid allOnes{Uuid::Bytes{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};
    QCOMPARE(allOnes.to_string(), std::string("ffffffff-ffff-ffff-ffff-ffffffffffff"));

    constexpr std::array<std::string_view, 9> invalid{{
        "",
        "00112233-4455-6677-8899-aabbccddeef",
        "00112233-4455-6677-8899-aabbccddeeff0",
        "00112233445566778899aabbccddeeff",
        "{00112233-4455-6677-8899-aabbccddeeff}",
        "00112233_4455-6677-8899-aabbccddeeff",
        "00112233-4455-6677-8899-aabbccddeefg",
        " 00112233-4455-6677-8899-aabbccddeeff",
        "00112233-4455-6677-8899-aabbccddeeff ",
    }};
    for (const auto text : invalid) {
        auto rejected = Uuid::parse(text);
        QVERIFY(rejected.error() != nullptr);
        QCOMPARE(rejected.error()->code(), ErrorCode::InvalidUuid);
    }

    auto typed = StrongId<FirstIdTag>::from_uuid(*parsed.value());
    QVERIFY(typed.value() != nullptr);
    QCOMPARE(typed.value()->uuid(), *parsed.value());
    QCOMPARE(typed.value()->to_string(), std::string(canonical));
    auto rejectedNil = StrongId<FirstIdTag>::from_uuid(Uuid{});
    QVERIFY(rejectedNil.error() != nullptr);
    QCOMPARE(rejectedNil.error()->code(), ErrorCode::InvalidUuid);
}

void verifyCheckedInteger()
{
    constexpr std::array<std::int64_t, 7> dividends{-7, -6, -1, 0, 1, 6, 7};
    constexpr std::array<std::int64_t, 4> divisors{1, 2, 3, 7};
    for (const auto dividend : dividends) {
        for (const auto divisor : divisors) {
            auto floor = floor_div_signed(dividend, divisor);
            auto ceil = ceil_div_signed(dividend, divisor);
            auto modulo = floor_mod(dividend, divisor);
            QVERIFY(floor.value() != nullptr);
            QVERIFY(ceil.value() != nullptr);
            QVERIFY(modulo.value() != nullptr);

            const auto truncated = dividend / divisor;
            const auto truncatedRemainder = dividend % divisor;
            const auto expectedFloor = truncated - (truncatedRemainder < 0 ? 1 : 0);
            const auto expectedCeil = truncated + (truncatedRemainder > 0 ? 1 : 0);
            QCOMPARE(*floor.value(), expectedFloor);
            QCOMPARE(*ceil.value(), expectedCeil);
            QVERIFY(*modulo.value() >= 0);
            QVERIFY(*modulo.value() < divisor);
            QCOMPARE(dividend, divisor * *floor.value() + *modulo.value());
        }
    }

    auto negativeFloor = floor_div_signed(-7, 3);
    auto negativeCeil = ceil_div_signed(-7, 3);
    auto negativeModulo = floor_mod(-7, 3);
    QCOMPARE(*negativeFloor.value(), std::int64_t{-3});
    QCOMPARE(*negativeCeil.value(), std::int64_t{-2});
    QCOMPARE(*negativeModulo.value(), std::int64_t{2});
    QCOMPARE(std::int64_t{-7} / std::int64_t{3}, std::int64_t{-2});

    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    auto minimumFloor = floor_div_signed(minimum, 1);
    auto minimumCeil = ceil_div_signed(minimum, maximum);
    auto minimumModulo = floor_mod(minimum, maximum);
    QCOMPARE(*minimumFloor.value(), minimum);
    QCOMPARE(*minimumCeil.value(), std::int64_t{-1});
    QCOMPARE(*minimumModulo.value(), maximum - 1);

    constexpr std::array<std::int64_t, 4> boundaryDividends{
        minimum, minimum + 1, maximum - 1, maximum};
    constexpr std::array<std::int64_t, 4> boundaryDivisors{1, 2, 3, maximum};
    for (const auto dividend : boundaryDividends) {
        for (const auto divisor : boundaryDivisors) {
            auto floor = floor_div_signed(dividend, divisor);
            auto ceil = ceil_div_signed(dividend, divisor);
            auto modulo = floor_mod(dividend, divisor);
            QVERIFY(floor.value() != nullptr);
            QVERIFY(ceil.value() != nullptr);
            QVERIFY(modulo.value() != nullptr);
            const auto quotient = dividend / divisor;
            const auto remainder = dividend % divisor;
            QCOMPARE(*floor.value(), quotient - (remainder < 0 ? 1 : 0));
            QCOMPARE(*ceil.value(), quotient + (remainder > 0 ? 1 : 0));
            QVERIFY(*modulo.value() >= 0);
            QVERIFY(*modulo.value() < divisor);
        }
    }

    for (const auto invalidDivisor : {std::int64_t{0}, std::int64_t{-1}}) {
        QVERIFY(floor_div_signed(1, invalidDivisor).error() != nullptr);
        QVERIFY(ceil_div_signed(1, invalidDivisor).error() != nullptr);
        QVERIFY(floor_mod(1, invalidDivisor).error() != nullptr);
    }

    QCOMPARE(*checked_add(maximum - 1, 1).value(), maximum);
    QCOMPARE(*checked_add(minimum + 1, -1).value(), minimum);
    QCOMPARE(checked_add(maximum, 1).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(checked_add(minimum, -1).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(*checked_subtract(minimum + 1, 1).value(), minimum);
    QCOMPARE(*checked_subtract(maximum - 1, -1).value(), maximum);
    QCOMPARE(checked_subtract(minimum, 1).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(checked_subtract(maximum, -1).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(*checked_multiply(minimum, 1).value(), minimum);
    QCOMPARE(*checked_multiply(maximum, 1).value(), maximum);
    QCOMPARE(checked_multiply(minimum, -1).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(checked_multiply(maximum, 2).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(*checked_negate(maximum).value(), -maximum);
    QCOMPARE(checked_negate(minimum).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(*checked_increment(maximum - 1).value(), maximum);
    QCOMPARE(checked_increment(maximum).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(*next_even(0).value(), std::int64_t{0});
    QCOMPARE(*next_even(7).value(), std::int64_t{8});
    QCOMPARE(*next_even(maximum - 1).value(), maximum - 1);
    QCOMPARE(next_even(maximum).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(next_even(-1).error()->code(), ErrorCode::InvalidArgument);
}

void verifyRational()
{
    auto reduced = Rational::create(6, 8);
    QVERIFY(reduced.value() != nullptr);
    QCOMPARE(reduced.value()->numerator(), std::int64_t{3});
    QCOMPARE(reduced.value()->denominator(), std::int64_t{4});
    QCOMPARE(reduced.value()->to_string(), std::string("3/4"));

    auto negative = Rational::create(-6, 8);
    QVERIFY(negative.value() != nullptr);
    QCOMPARE(negative.value()->to_string(), std::string("-3/4"));
    QCOMPARE(Rational::create(1, 0).error()->code(), ErrorCode::InvalidRational);
    QCOMPARE(Rational::create(1, -2).error()->code(), ErrorCode::InvalidRational);
    auto equivalent = Rational::create(2, 4);
    auto oneHalf = Rational::create(1, 2);
    QVERIFY(equivalent.value() != nullptr);
    QVERIFY(oneHalf.value() != nullptr);
    QCOMPARE(*equivalent.value(), *oneHalf.value());

    auto zero = Rational::create(0, std::numeric_limits<std::int64_t>::max());
    QVERIFY(zero.value() != nullptr);
    QCOMPARE(zero.value()->to_string(), std::string("0/1"));

    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    auto minimumRational = Rational::create(minimum, 1);
    QVERIFY(minimumRational.value() != nullptr);
    QCOMPARE(minimumRational.value()->to_string(), std::string("-9223372036854775808/1"));
    auto parsedMinimum = Rational::parse("-9223372036854775808/1");
    QVERIFY(parsedMinimum.value() != nullptr);
    QCOMPARE(*parsedMinimum.value(), *minimumRational.value());
    auto reducedMinimum = Rational::create(minimum, 2);
    QVERIFY(reducedMinimum.value() != nullptr);
    QCOMPARE(reducedMinimum.value()->to_string(), std::string("-4611686018427387904/1"));
    auto boundaryDenominator = Rational::create(minimum, maximum);
    QVERIFY(boundaryDenominator.value() != nullptr);
    QCOMPARE(boundaryDenominator.value()->denominator(), maximum);

    for (const auto text : {
             std::string_view{""}, std::string_view{"1"}, std::string_view{"1/0"},
             std::string_view{"+1/2"}, std::string_view{"01/2"}, std::string_view{"-01/2"},
             std::string_view{"1/+2"}, std::string_view{"1/-2"}, std::string_view{"1/02"},
             std::string_view{" 1/2"}, std::string_view{"1/2 "}, std::string_view{"2/4"},
             std::string_view{"-0/1"}}) {
        auto rejected = Rational::parse(text);
        QVERIFY(rejected.error() != nullptr);
        QCOMPARE(rejected.error()->code(), ErrorCode::ParseFailure);
    }
    for (const auto text : {
             std::string_view{"0/1"}, std::string_view{"1/2"}, std::string_view{"-7/3"},
             std::string_view{"9223372036854775807/1"}}) {
        auto parsed = Rational::parse(text);
        QVERIFY(parsed.value() != nullptr);
        QCOMPARE(parsed.value()->to_string(), std::string(text));
    }

    auto firstLarge = Rational::create(maximum, maximum - 1);
    auto secondLarge = Rational::create(maximum - 1, maximum - 2);
    QVERIFY(firstLarge.value() != nullptr);
    QVERIFY(secondLarge.value() != nullptr);
    QVERIFY(*firstLarge.value() < *secondLarge.value());
    auto firstNegativeLarge = Rational::create(-maximum, maximum - 1);
    auto secondNegativeLarge = Rational::create(-(maximum - 1), maximum - 2);
    QVERIFY(*firstNegativeLarge.value() > *secondNegativeLarge.value());

    auto largeHalf = Rational::create(maximum, 2);
    auto negativeLargeHalf = Rational::create(-maximum, 2);
    auto cancelledSum = largeHalf.value()->add(*negativeLargeHalf.value());
    QVERIFY(cancelledSum.value() != nullptr);
    QCOMPARE(cancelledSum.value()->to_string(), std::string("0/1"));

    // Each scaled numerator term exceeds INT64_MAX, but exact cancellation
    // through the shared denominator factor leaves the representable 1/6.
    auto wideLeft = Rational::create(
        3200000000000000001LL, 3200000000000000002LL);
    auto wideRight = Rational::create(
        -4000000000000000001LL, 4800000000000000003LL);
    QVERIFY(wideLeft.value() != nullptr);
    QVERIFY(wideRight.value() != nullptr);
    auto wideCancellation = wideLeft.value()->add(*wideRight.value());
    QVERIFY(wideCancellation.value() != nullptr);
    QCOMPARE(wideCancellation.value()->to_string(), std::string("1/6"));

    auto reciprocalFactor = Rational::create(2, maximum);
    auto cancelledProduct = largeHalf.value()->multiply(*reciprocalFactor.value());
    QVERIFY(cancelledProduct.value() != nullptr);
    QCOMPARE(cancelledProduct.value()->to_string(), std::string("1/1"));
    auto minimumQuotient = minimumRational.value()->divide(*minimumRational.value());
    QVERIFY(minimumQuotient.value() != nullptr);
    QCOMPARE(minimumQuotient.value()->to_string(), std::string("1/1"));

    auto maximumRational = Rational::create(maximum, 1);
    auto one = Rational::create(1, 1);
    auto two = Rational::create(2, 1);
    QCOMPARE(maximumRational.value()->add(*one.value()).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(maximumRational.value()->multiply(*two.value()).error()->code(), ErrorCode::IntegerOverflow);
    QCOMPARE(one.value()->divide(*zero.value()).error()->code(), ErrorCode::DivisionByZero);
    QCOMPARE(*one.value()->add(*zero.value()).value(), *one.value());
    QCOMPARE(*one.value()->multiply(*one.value()).value(), *one.value());

    for (std::int64_t leftNumerator = -5; leftNumerator <= 5; ++leftNumerator) {
        for (std::int64_t leftDenominator = 1; leftDenominator <= 5; ++leftDenominator) {
            auto left = Rational::create(leftNumerator, leftDenominator);
            for (std::int64_t rightNumerator = -5; rightNumerator <= 5; ++rightNumerator) {
                for (std::int64_t rightDenominator = 1; rightDenominator <= 5; ++rightDenominator) {
                    auto right = Rational::create(rightNumerator, rightDenominator);
                    auto expectedSum = Rational::create(
                        leftNumerator * rightDenominator + rightNumerator * leftDenominator,
                        leftDenominator * rightDenominator);
                    auto actualSum = left.value()->add(*right.value());
                    QVERIFY(actualSum.value() != nullptr);
                    QCOMPARE(*actualSum.value(), *expectedSum.value());

                    auto expectedDifference = Rational::create(
                        leftNumerator * rightDenominator - rightNumerator * leftDenominator,
                        leftDenominator * rightDenominator);
                    auto actualDifference = left.value()->subtract(*right.value());
                    QVERIFY(actualDifference.value() != nullptr);
                    QCOMPARE(*actualDifference.value(), *expectedDifference.value());

                    auto expectedProduct = Rational::create(
                        leftNumerator * rightNumerator,
                        leftDenominator * rightDenominator);
                    auto actualProduct = left.value()->multiply(*right.value());
                    QVERIFY(actualProduct.value() != nullptr);
                    QCOMPARE(*actualProduct.value(), *expectedProduct.value());

                    const auto leftCross = leftNumerator * rightDenominator;
                    const auto rightCross = rightNumerator * leftDenominator;
                    QCOMPARE(*left.value() < *right.value(), leftCross < rightCross);
                    QCOMPARE(*left.value() == *right.value(), leftCross == rightCross);

                    if (rightNumerator != 0) {
                        std::int64_t expectedNumerator = leftNumerator * rightDenominator;
                        std::int64_t expectedDenominator = leftDenominator * rightNumerator;
                        if (expectedDenominator < 0) {
                            expectedNumerator = -expectedNumerator;
                            expectedDenominator = -expectedDenominator;
                        }
                        auto expectedQuotient = Rational::create(expectedNumerator, expectedDenominator);
                        auto actualQuotient = left.value()->divide(*right.value());
                        QVERIFY(actualQuotient.value() != nullptr);
                        QCOMPARE(*actualQuotient.value(), *expectedQuotient.value());
                    }
                }
            }
        }
    }

    const std::array<Rational, 5> ordered{
        *Rational::create(-2, 1).value(),
        *Rational::create(-1, 3).value(),
        *Rational::create(0, 1).value(),
        *Rational::create(1, 3).value(),
        *Rational::create(2, 1).value()};
    for (std::size_t first = 0; first < ordered.size(); ++first) {
        QCOMPARE(ordered[first], ordered[first]);
        for (std::size_t second = first + 1; second < ordered.size(); ++second) {
            QVERIFY(ordered[first] < ordered[second]);
            QVERIFY(!(ordered[second] < ordered[first]));
            for (std::size_t third = second + 1; third < ordered.size(); ++third) {
                QVERIFY(ordered[first] < ordered[third]);
            }
        }
    }
}

void verifyFrameAndTime()
{
    for (const auto value : {44100, 48000, 96000, 192000}) {
        auto rate = SampleRate::create(value);
        QVERIFY(rate.value() != nullptr);
        QCOMPARE(rate.value()->value(), std::int64_t{value});
        auto exact = frame_to_time(FrameIndex{1}, *rate.value());
        QVERIFY(exact.value() != nullptr);
        QCOMPARE(exact.value()->seconds().to_string(), std::string("1/") + std::to_string(value));
    }
    QCOMPARE(SampleRate::create(0).error()->code(), ErrorCode::InvalidArgument);
    QCOMPARE(SampleRate::create(-1).error()->code(), ErrorCode::InvalidArgument);

    QCOMPARE(FrameIndex{-7}.value(), std::int64_t{-7});
    QCOMPARE(FrameIndex{0}.value(), std::int64_t{0});
    QCOMPARE(FrameIndex{7}.value(), std::int64_t{7});
    QCOMPARE(FrameCount::create(-1).error()->code(), ErrorCode::OutOfRange);
    QCOMPARE(FrameCount::create(0).value()->value(), std::int64_t{0});
    QCOMPARE(FrameCount::create(7).value()->value(), std::int64_t{7});

    auto range = FrameRange::create(FrameIndex{-5}, FrameIndex{7});
    QVERIFY(range.value() != nullptr);
    QCOMPARE(range.value()->begin().value(), std::int64_t{-5});
    QCOMPARE(range.value()->end().value(), std::int64_t{7});
    QCOMPARE(range.value()->length().value()->value(), std::int64_t{12});
    auto empty = FrameRange::create(FrameIndex{4}, FrameIndex{4});
    QVERIFY(empty.value() != nullptr);
    QCOMPARE(empty.value()->length().value()->value(), std::int64_t{0});
    auto canonicalEmpty = FrameRange::create(FrameIndex{10}, FrameIndex{10});
    auto singleFrame = FrameRange::create(FrameIndex{10}, FrameIndex{11});
    QCOMPARE(canonicalEmpty.value()->length().value()->value(), std::int64_t{0});
    QCOMPARE(singleFrame.value()->length().value()->value(), std::int64_t{1});
    QCOMPARE(
        FrameRange::create(FrameIndex{1}, FrameIndex{0}).error()->code(),
        ErrorCode::InvalidFrameRange);
    auto huge = FrameRange::create(
        FrameIndex{std::numeric_limits<std::int64_t>::min()},
        FrameIndex{std::numeric_limits<std::int64_t>::max()});
    QVERIFY(huge.value() != nullptr);
    QCOMPARE(huge.value()->length().error()->code(), ErrorCode::IntegerOverflow);

    auto rate48000 = SampleRate::create(48000);
    auto negativeSecond = frame_to_time(FrameIndex{-48000}, *rate48000.value());
    QVERIFY(negativeSecond.value() != nullptr);
    QCOMPARE(negativeSecond.value()->seconds().to_string(), std::string("-1/1"));
    auto negativeFrame = frame_to_time(FrameIndex{-1}, *rate48000.value());
    QCOMPARE(negativeFrame.value()->seconds().to_string(), std::string("-1/48000"));
    auto count44100 = FrameCount::create(44100);
    auto rate44100 = SampleRate::create(44100);
    auto duration = frame_count_to_duration(*count44100.value(), *rate44100.value());
    QVERIFY(duration.value() != nullptr);
    QCOMPARE(duration.value()->seconds().to_string(), std::string("1/1"));
    auto frameSecond = frame_to_time(FrameIndex{44100}, *rate44100.value());
    QCOMPARE(frameSecond.value()->seconds().to_string(), std::string("1/1"));
}

void verifyProcessingState()
{
    constexpr std::array<std::pair<ProcessingState, std::string_view>, 4> states{{
        {ProcessingState::RAW, "RAW"},
        {ProcessingState::PREPARED, "PREPARED"},
        {ProcessingState::PROCESSED, "PROCESSED"},
        {ProcessingState::GOLD, "GOLD"},
    }};
    for (const auto& [state, token] : states) {
        QCOMPARE(processing_state_token(state), token);
        auto parsed = parse_processing_state(token);
        QVERIFY(parsed.value() != nullptr);
        QCOMPARE(*parsed.value(), state);
    }
    for (const auto invalid : {
             std::string_view{}, std::string_view{"raw"}, std::string_view{"MASTERED"},
             std::string_view{"UNKNOWN"}}) {
        auto rejected = parse_processing_state(invalid);
        QVERIFY(rejected.error() != nullptr);
        QCOMPARE(rejected.error()->code(), ErrorCode::ParseFailure);
    }
}

}  // namespace

void runCorePrimitiveTests()
{
    verifyResultAndError();
    verifyUuidAndStrongId();
    verifyCheckedInteger();
    verifyRational();
    verifyFrameAndTime();
    verifyProcessingState();
}

}  // namespace rgsml::tests
