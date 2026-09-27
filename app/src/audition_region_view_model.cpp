#include "audition_region_view_model.hpp"

#include "playback_transport_view_model.hpp"

#include <rgsml/core/checked_integer.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace rgsml::app {
namespace {

[[nodiscard]] core::Status invalid_region(const char* message)
{
    return core::Status::failure(core::Error{
        core::ErrorCode::InvalidFrameRange,
        message});
}

[[nodiscard]] core::Result<core::FrameIndex> invalid_time(const char* message)
{
    return core::Result<core::FrameIndex>::failure(core::Error{
        core::ErrorCode::ParseFailure,
        message});
}

[[nodiscard]] bool all_digits(QStringView text) noexcept
{
    if (text.isEmpty()) {
        return false;
    }
    for (const auto character : text) {
        if (!character.isDigit()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::int64_t parse_bounded_decimal(
    QStringView text,
    std::int64_t limit,
    bool& saturated) noexcept
{
    std::int64_t value = 0;
    saturated = false;
    for (const auto character : text) {
        const auto digit = character.digitValue();
        if (value > (limit - digit) / 10) {
            saturated = true;
            return limit;
        }
        value = value * 10 + digit;
    }
    return value;
}

[[nodiscard]] QString playback_error_message(core::ErrorCode code)
{
    switch (code) {
    case core::ErrorCode::OutOfRange:
    case core::ErrorCode::InvalidFrameRange:
        return QStringLiteral("Audition Region is outside the current Source.");
    case core::ErrorCode::InvalidState:
        return QStringLiteral("Audition command is unavailable in the current playback state.");
    default:
        return QStringLiteral("Audition command failed; the previous state was preserved.");
    }
}

[[nodiscard]] core::Result<std::int64_t> round_fractional_seconds_to_frames(
    QStringView decimalDigits,
    std::int64_t sampleRate)
{
    if (decimalDigits.isEmpty() || sampleRate <= 0
        || sampleRate > static_cast<std::int64_t>(
            std::numeric_limits<std::uint32_t>::max())) {
        return core::Result<std::int64_t>::failure(core::Error{
            core::ErrorCode::InvalidArgument,
            "Fractional time conversion requires a valid WAV sample rate."});
    }

    // Multiply the exact base-10 fraction numerator by the WAV rate without a
    // binary floating conversion or a fixed-width product.
    std::vector<int> product;
    product.reserve(static_cast<std::size_t>(decimalDigits.size()) + 10U);
    std::uint64_t carry = 0;
    for (qsizetype index = decimalDigits.size(); index > 0; --index) {
        const auto value = static_cast<std::uint64_t>(
            decimalDigits[index - 1].digitValue())
            * static_cast<std::uint64_t>(sampleRate) + carry;
        product.push_back(static_cast<int>(value % 10U));
        carry = value / 10U;
    }
    while (carry != 0U) {
        product.push_back(static_cast<int>(carry % 10U));
        carry /= 10U;
    }
    while (product.size() <= static_cast<std::size_t>(decimalDigits.size())) {
        product.push_back(0);
    }

    const auto fractionalDigitCount = static_cast<std::size_t>(decimalDigits.size());
    std::int64_t quotient = 0;
    for (std::size_t index = product.size(); index > fractionalDigitCount; --index) {
        quotient = quotient * 10 + product[index - 1U];
    }

    int halfComparison = 0;
    for (std::size_t position = fractionalDigitCount; position > 0; --position) {
        const auto actual = product[position - 1U];
        const auto half = position == fractionalDigitCount ? 5 : 0;
        if (actual != half) {
            halfComparison = actual < half ? -1 : 1;
            break;
        }
    }
    if (halfComparison > 0 || (halfComparison == 0 && quotient % 2 != 0)) {
        ++quotient;
    }
    return core::Result<std::int64_t>::success(quotient);
}

[[nodiscard]] std::int64_t round_subsecond_units(
    std::int64_t remainder,
    std::int64_t units,
    std::int64_t sampleRate) noexcept
{
    const auto product = core::checked_multiply(remainder, units);
    if (!product || sampleRate <= 0) {
        return 0;
    }
    auto quotient = *product.value() / sampleRate;
    const auto residual = *product.value() % sampleRate;
    const auto opposite = sampleRate - residual;
    if (residual > opposite
        || (residual == opposite && quotient % 2 != 0)) {
        ++quotient;
    }
    return quotient;
}

}  // namespace

AuditionRegionViewModel::AuditionRegionViewModel(
    PlaybackTransportViewModel* playback,
    QObject* parent)
    : QObject(parent)
    , playback_(playback)
{
    if (playback_) {
        connect(
            playback_,
            &PlaybackTransportViewModel::playbackChanged,
            this,
            &AuditionRegionViewModel::synchronize_playback);
    }
}

bool AuditionRegionViewModel::has_region() const noexcept { return region_.has_value(); }
std::optional<core::FrameRange> AuditionRegionViewModel::region() const noexcept
{
    return region_;
}
bool AuditionRegionViewModel::loop_enabled() const noexcept { return loopEnabled_; }
bool AuditionRegionViewModel::controls_enabled() const noexcept
{
    return waveformReady_ && sourceFrames_ > 0;
}
bool AuditionRegionViewModel::can_loop() const noexcept
{
    return controls_enabled() && region_.has_value()
        && playback_ && playback_->playback_available();
}
QString AuditionRegionViewModel::start_hours() const
{
    const auto value = region_ ? format_segments(region_->begin().value()) : std::nullopt;
    return value ? value->hours : QStringLiteral("00");
}
QString AuditionRegionViewModel::start_minutes() const
{
    const auto value = region_ ? format_segments(region_->begin().value()) : std::nullopt;
    return value ? value->minutes : QStringLiteral("00");
}
QString AuditionRegionViewModel::start_seconds() const
{
    const auto value = region_ ? format_segments(region_->begin().value()) : std::nullopt;
    return value ? value->seconds : QStringLiteral("00");
}
QString AuditionRegionViewModel::start_fraction() const
{
    const auto value = region_ ? format_segments(region_->begin().value()) : std::nullopt;
    return value ? value->fraction : QStringLiteral("000000000");
}
QString AuditionRegionViewModel::end_hours() const
{
    const auto value = region_ ? format_segments(region_->end().value()) : std::nullopt;
    return value ? value->hours : QStringLiteral("00");
}
QString AuditionRegionViewModel::end_minutes() const
{
    const auto value = region_ ? format_segments(region_->end().value()) : std::nullopt;
    return value ? value->minutes : QStringLiteral("00");
}
QString AuditionRegionViewModel::end_seconds() const
{
    const auto value = region_ ? format_segments(region_->end().value()) : std::nullopt;
    return value ? value->seconds : QStringLiteral("00");
}
QString AuditionRegionViewModel::end_fraction() const
{
    const auto value = region_ ? format_segments(region_->end().value()) : std::nullopt;
    return value ? value->fraction : QStringLiteral("000000000");
}
QString AuditionRegionViewModel::duration_text() const
{
    return region_ ? format_frame(region_->end().value() - region_->begin().value()) : QStringLiteral("—");
}
QString AuditionRegionViewModel::start_frame_text() const
{
    return region_ ? QString::number(region_->begin().value()) : QStringLiteral("—");
}
QString AuditionRegionViewModel::end_frame_text() const
{
    return region_ ? QString::number(region_->end().value()) : QStringLiteral("—");
}
QString AuditionRegionViewModel::error_message() const { return errorMessage_; }

core::Status AuditionRegionViewModel::validate(core::FrameRange candidate) const
{
    if (sourceFrames_ <= 0
        || candidate.begin().value() < 0
        || candidate.begin().value() >= candidate.end().value()
        || candidate.end().value() > sourceFrames_) {
        return invalid_region("Audition Region must be a non-empty half-open range inside the current Source.");
    }
    return core::Status::success();
}

core::Status AuditionRegionViewModel::set_region(core::FrameRange candidate)
{
    const auto valid = validate(candidate);
    if (!valid) {
        publish_error(*valid.error());
        return valid;
    }
    if (loopEnabled_) {
        if (!playback_) {
            const auto failure = core::Status::failure(core::Error{
                core::ErrorCode::InvalidState,
                "Playback transport is unavailable."});
            publish_error(*failure.error());
            return failure;
        }
        const auto beforeLoopUpdate = playback_->playback_snapshot();
        if (!beforeLoopUpdate) {
            publish_error(*beforeLoopUpdate.error());
            return core::Status::failure(*beforeLoopUpdate.error());
        }
        const auto loopResult = playback_->set_loop_source_range(candidate);
        synchronize_playback();
        if (!loopResult) {
            publish_error(*loopResult.error());
            return loopResult;
        }
        publish_region(candidate);
        return reposition_if_outside(
            candidate, beforeLoopUpdate.value()->position);
    }
    publish_region(candidate);
    return core::Status::success();
}

core::Status AuditionRegionViewModel::set_start(core::FrameIndex start)
{
    if (!region_) {
        return invalid_region("Create an Audition Region before editing its start.");
    }
    const auto bounded = std::clamp(
        start.value(), std::int64_t{0}, region_->end().value() - 1);
    auto candidate = core::FrameRange::create(
        core::FrameIndex{bounded}, region_->end());
    return candidate ? set_region(*candidate.value()) : invalid_region("Invalid region start.");
}

core::Status AuditionRegionViewModel::set_end_exclusive(core::FrameIndex end)
{
    if (!region_) {
        return invalid_region("Create an Audition Region before editing its end.");
    }
    const auto bounded = std::clamp(
        end.value(), region_->begin().value() + 1, sourceFrames_);
    auto candidate = core::FrameRange::create(
        region_->begin(), core::FrameIndex{bounded});
    return candidate ? set_region(*candidate.value()) : invalid_region("Invalid region end.");
}

core::Status AuditionRegionViewModel::clear_region()
{
    if (!region_) {
        return core::Status::success();
    }
    if (loopEnabled_) {
        if (!playback_) {
            return invalid_region("Playback transport is unavailable.");
        }
        const auto disabled = playback_->set_loop_source_range(std::nullopt);
        if (!disabled) {
            publish_error(*disabled.error());
            return disabled;
        }
    }
    publish_region(std::nullopt);
    return core::Status::success();
}

core::Status AuditionRegionViewModel::set_loop_enabled(bool enabled)
{
    if (enabled && (!region_ || !playback_ || !playback_->playback_available())) {
        const auto failure = core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "Loop Region requires a valid region and prepared playback."});
        publish_error(*failure.error());
        return failure;
    }
    if (!playback_) {
        const auto failure = core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "Playback transport is unavailable."});
        publish_error(*failure.error());
        return failure;
    }
    std::optional<core::FrameIndex> positionBeforeLoopUpdate;
    if (enabled) {
        const auto snapshot = playback_->playback_snapshot();
        if (!snapshot) {
            publish_error(*snapshot.error());
            return core::Status::failure(*snapshot.error());
        }
        positionBeforeLoopUpdate = snapshot.value()->position;
    }
    const auto result = playback_->set_loop_source_range(
        enabled ? region_ : std::nullopt);
    if (!result) {
        publish_error(*result.error());
        return result;
    }
    loopEnabled_ = enabled;
    lastCanLoop_ = can_loop();
    if (enabled && positionBeforeLoopUpdate) {
        return reposition_if_outside(*region_, *positionBeforeLoopUpdate);
    }
    errorMessage_.clear();
    emit changed();
    return core::Status::success();
}

core::Status AuditionRegionViewModel::seek(core::FrameIndex position)
{
    if (!playback_) {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "Playback transport is unavailable."});
    }
    const auto result = playback_->seek_source_frame(position);
    if (!result) {
        publish_error(*result.error());
    } else if (!errorMessage_.isEmpty()) {
        errorMessage_.clear();
        emit changed();
    }
    return result;
}

void AuditionRegionViewModel::source_committed(
    core::FrameCount frameCount,
    core::SampleRate sampleRate)
{
    sourceFrames_ = frameCount.value();
    sampleRate_ = sampleRate.value();
    waveformReady_ = false;
    region_.reset();
    loopEnabled_ = false;
    lastCanLoop_ = false;
    errorMessage_.clear();
    emit changed();
}

void AuditionRegionViewModel::set_waveform_ready(bool ready)
{
    if (waveformReady_ == ready) {
        return;
    }
    waveformReady_ = ready;
    lastCanLoop_ = can_loop();
    emit changed();
}

void AuditionRegionViewModel::synchronize_playback()
{
    if (!playback_) {
        return;
    }
    const bool currentCanLoop = can_loop();
    if (lastCanLoop_ != currentCanLoop) {
        lastCanLoop_ = currentCanLoop;
        emit changed();
    }
}

core::Status AuditionRegionViewModel::reposition_if_outside(
    core::FrameRange activeLoop,
    core::FrameIndex positionBeforeLoopUpdate)
{
    if (!playback_) {
        return core::Status::success();
    }
    const auto position = positionBeforeLoopUpdate.value();
    if (position >= activeLoop.begin().value()
        && position < activeLoop.end().value()) {
        errorMessage_.clear();
        emit changed();
        return core::Status::success();
    }
    const auto seekResult = playback_->seek_source_frame(activeLoop.begin());
    if (!seekResult) {
        publish_error(*seekResult.error(), true);
        return seekResult;
    }
    errorMessage_.clear();
    emit changed();
    return core::Status::success();
}

core::Result<core::FrameIndex> AuditionRegionViewModel::parse_segmented_time(
    const QString& hoursText,
    const QString& minutesText,
    const QString& secondsText,
    const QString& fractionText) const
{
    if (sampleRate_ <= 0 || sourceFrames_ <= 0) {
        return invalid_time("No ready Source timeline is available.");
    }
    if (!all_digits(hoursText) || !all_digits(minutesText)
        || !all_digits(secondsText) || !all_digits(fractionText)) {
        return invalid_time("Complete every Region time segment using decimal digits only.");
    }
    if (minutesText.size() > 2 || secondsText.size() > 2) {
        return invalid_time("Minutes and seconds use at most two digits.");
    }
    if (fractionText.size() > 9) {
        return invalid_time("Region fractions use between one and nine decimal digits.");
    }

    bool minuteSaturated = false;
    const auto minutes = parse_bounded_decimal(
        minutesText, 59, minuteSaturated);
    bool secondSaturated = false;
    const auto seconds = parse_bounded_decimal(
        secondsText, 59, secondSaturated);
    if (minuteSaturated || secondSaturated || minutes > 59 || seconds > 59) {
        return invalid_time("Minutes and seconds must be between 00 and 59.");
    }

    bool hourSaturated = false;
    const auto hours = parse_bounded_decimal(
        hoursText,
        std::numeric_limits<std::int64_t>::max() / 3'600,
        hourSaturated);
    if (hourSaturated) {
        return invalid_time("Region hours exceed the supported Source timeline range.");
    }

    const auto hourSeconds = core::checked_multiply(hours, 3'600);
    const auto minuteSeconds = core::checked_multiply(minutes, 60);
    const auto prefix = hourSeconds && minuteSeconds
        ? core::checked_add(*hourSeconds.value(), *minuteSeconds.value())
        : core::Result<std::int64_t>::failure(core::Error{
            core::ErrorCode::IntegerOverflow, "Time exceeds supported range."});
    const auto wholeSeconds = prefix
        ? core::checked_add(*prefix.value(), seconds)
        : core::Result<std::int64_t>::failure(core::Error{
            core::ErrorCode::IntegerOverflow, "Time exceeds supported range."});
    if (!wholeSeconds) {
        return invalid_time("Region time exceeds the supported Source timeline range.");
    }
    const auto baseFrames = core::checked_multiply(
        *wholeSeconds.value(), sampleRate_);
    if (!baseFrames) {
        return invalid_time("Region time exceeds the supported Source timeline range.");
    }
    const auto rounded = round_fractional_seconds_to_frames(
        fractionText, sampleRate_);
    if (!rounded) {
        return core::Result<core::FrameIndex>::failure(*rounded.error());
    }
    const auto combined = core::checked_add(
        *baseFrames.value(), *rounded.value());
    if (!combined) {
        return invalid_time("Region time exceeds the supported Source timeline range.");
    }
    return core::Result<core::FrameIndex>::success(core::FrameIndex{
        std::min(*combined.value(), sourceFrames_)});
}

std::optional<AuditionRegionViewModel::TimeSegments>
AuditionRegionViewModel::format_segments(std::int64_t frame) const
{
    if (sampleRate_ <= 0 || frame < 0) {
        return std::nullopt;
    }
    const auto seconds = frame / sampleRate_;
    const auto remainder = frame % sampleRate_;
    std::int64_t units = 1;
    for (int digits = 1; digits <= 9; ++digits) {
        units *= 10;
        auto fractionUnits = round_subsecond_units(
            remainder, units, sampleRate_);
        auto displayedSeconds = seconds;
        if (fractionUnits == units) {
            const auto incremented = core::checked_increment(displayedSeconds);
            if (!incremented) {
                continue;
            }
            displayedSeconds = *incremented.value();
            fractionUnits = 0;
        }
        const auto fraction = QStringLiteral("%1")
            .arg(fractionUnits, digits, 10, QLatin1Char('0'));
        const auto base = core::checked_multiply(displayedSeconds, sampleRate_);
        const auto fractional = round_fractional_seconds_to_frames(
            fraction, sampleRate_);
        const auto candidate = base && fractional
            ? core::checked_add(*base.value(), *fractional.value())
            : core::Result<std::int64_t>::failure(core::Error{
                core::ErrorCode::IntegerOverflow,
                "Formatted time exceeds supported range."});
        if (!candidate || *candidate.value() != frame) {
            continue;
        }
        return TimeSegments{
            QStringLiteral("%1").arg(
                displayedSeconds / 3'600, 2, 10, QLatin1Char('0')),
            QStringLiteral("%1").arg(
                (displayedSeconds / 60) % 60, 2, 10, QLatin1Char('0')),
            QStringLiteral("%1").arg(
                displayedSeconds % 60, 2, 10, QLatin1Char('0')),
            fraction.leftJustified(9, QLatin1Char('0')),
        };
    }
    return std::nullopt;
}

QString AuditionRegionViewModel::format_frame(std::int64_t frame) const
{
    const auto value = format_segments(frame);
    if (!value) {
        return {};
    }
    return QStringLiteral("%1:%2:%3.%4")
        .arg(value->hours)
        .arg(value->minutes)
        .arg(value->seconds)
        .arg(value->fraction);
}

void AuditionRegionViewModel::publish_region(
    std::optional<core::FrameRange> region)
{
    region_ = std::move(region);
    if (!region_) {
        loopEnabled_ = false;
        lastCanLoop_ = false;
    }
    errorMessage_.clear();
    emit changed();
}

void AuditionRegionViewModel::publish_error(const core::Error& error, bool partial)
{
    publish_error(partial
        ? QStringLiteral("Loop Region is armed, but repositioning playback failed.")
        : playback_error_message(error.code()));
}

void AuditionRegionViewModel::publish_error(QString message)
{
    if (errorMessage_ == message) {
        return;
    }
    errorMessage_ = std::move(message);
    emit changed();
}

void AuditionRegionViewModel::commitStartSegments(
    const QString& hours,
    const QString& minutes,
    const QString& seconds,
    const QString& fraction)
{
    const auto parsed = parse_segmented_time(
        hours, minutes, seconds, fraction);
    if (!parsed || !region_) {
        publish_error(parsed ? QStringLiteral("Create an Audition Region before editing its start.")
                             : QString::fromStdString(parsed.error()->message()));
        return;
    }
    if (parsed.value()->value() >= region_->end().value()) {
        publish_error(QStringLiteral("Region start must be before the exclusive end."));
        return;
    }
    const auto candidate = core::FrameRange::create(
        *parsed.value(), region_->end());
    if (candidate) {
        static_cast<void>(set_region(*candidate.value()));
    }
}

void AuditionRegionViewModel::commitEndSegments(
    const QString& hours,
    const QString& minutes,
    const QString& seconds,
    const QString& fraction)
{
    const auto parsed = parse_segmented_time(
        hours, minutes, seconds, fraction);
    if (!parsed || !region_) {
        publish_error(parsed ? QStringLiteral("Create an Audition Region before editing its end.")
                             : QString::fromStdString(parsed.error()->message()));
        return;
    }
    if (parsed.value()->value() <= region_->begin().value()) {
        publish_error(QStringLiteral("Region end must be after the start."));
        return;
    }
    const auto candidate = core::FrameRange::create(
        region_->begin(), *parsed.value());
    if (candidate) {
        static_cast<void>(set_region(*candidate.value()));
    }
}

void AuditionRegionViewModel::requestLoopEnabled(bool enabled)
{
    static_cast<void>(set_loop_enabled(enabled));
}

void AuditionRegionViewModel::requestClearRegion()
{
    static_cast<void>(clear_region());
}

void AuditionRegionViewModel::nudgeStartBackward()
{
    if (region_) {
        static_cast<void>(set_start(core::FrameIndex{region_->begin().value() - 1}));
    }
}
void AuditionRegionViewModel::nudgeStartForward()
{
    if (region_) {
        const auto incremented = core::checked_increment(region_->begin().value());
        if (incremented) {
            static_cast<void>(set_start(core::FrameIndex{*incremented.value()}));
        }
    }
}
void AuditionRegionViewModel::nudgeEndBackward()
{
    if (region_) {
        static_cast<void>(set_end_exclusive(core::FrameIndex{region_->end().value() - 1}));
    }
}
void AuditionRegionViewModel::nudgeEndForward()
{
    if (region_) {
        const auto incremented = core::checked_increment(region_->end().value());
        if (incremented) {
            static_cast<void>(set_end_exclusive(core::FrameIndex{*incremented.value()}));
        }
    }
}

void AuditionRegionViewModel::clearError()
{
    if (!errorMessage_.isEmpty()) {
        errorMessage_.clear();
        emit changed();
    }
}

}  // namespace rgsml::app
