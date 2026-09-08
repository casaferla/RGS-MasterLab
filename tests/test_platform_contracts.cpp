#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/resource_io.hpp>
#include <rgsml/core/resource_reference.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace rgsml::core;

template <typename T>
concept HasWriteSurface = requires(T& value, std::span<const std::byte> bytes) {
    value.write(bytes);
};

template <typename T>
concept HasReadSurface = requires(T& value, std::span<std::byte> bytes) {
    value.read(bytes);
};

static_assert(std::is_abstract_v<IResourceReader>);
static_assert(std::is_abstract_v<IResourceWriter>);
static_assert(std::is_abstract_v<IAudioPlaybackService>);
static_assert(std::has_virtual_destructor_v<IResourceReader>);
static_assert(std::has_virtual_destructor_v<IResourceWriter>);
static_assert(std::has_virtual_destructor_v<IAudioPlaybackService>);
static_assert(!HasWriteSurface<IResourceReader>);
static_assert(!HasReadSurface<IResourceWriter>);

[[nodiscard]] Error make_error(ErrorCode code, std::string message)
{
    return Error{code, std::move(message)};
}

[[nodiscard]] ResourceReference make_reference(
    bool canRead,
    bool canWrite,
    std::string locator = "synthetic-item",
    std::string displayName = {})
{
    auto reference = ResourceReference::create(
        "fixture.provider", std::move(locator), canRead, canWrite, std::move(displayName));
    Q_ASSERT(reference.value() != nullptr);
    return *reference.value();
}

[[nodiscard]] ResourceCapabilities reader_capabilities(
    bool seek = true,
    bool knownSize = true) noexcept
{
    return ResourceCapabilities::create(seek, knownSize, false, false);
}

[[nodiscard]] ResourceCapabilities writer_capabilities(
    bool seek = true,
    bool resize = true,
    bool flush = true) noexcept
{
    return ResourceCapabilities::create(seek, true, resize, flush);
}

class FakeReader final : public IResourceReader {
public:
    FakeReader(
        ResourceReference reference,
        std::vector<std::byte> content,
        ResourceCapabilities capabilities)
        : reference_(std::move(reference))
        , content_(std::move(content))
        , capabilities_(capabilities)
    {
    }

    [[nodiscard]] const ResourceReference& reference() const noexcept override
    {
        return reference_;
    }

    [[nodiscard]] ResourceCapabilities capabilities() const noexcept override
    {
        return capabilities_;
    }

    [[nodiscard]] Result<std::uint64_t> size_bytes() const override
    {
        if (closed_) {
            return Result<std::uint64_t>::failure(
                make_error(ErrorCode::InvalidState, "Reader is closed."));
        }
        if (!capabilities_.supports(ResourceCapability::HasKnownSize)) {
            return Result<std::uint64_t>::failure(
                make_error(ErrorCode::UnsupportedOperation, "Size is unknown."));
        }
        return Result<std::uint64_t>::success(
            static_cast<std::uint64_t>(content_.size()));
    }

    [[nodiscard]] Result<std::uint64_t> position_bytes() const override
    {
        if (closed_) {
            return Result<std::uint64_t>::failure(
                make_error(ErrorCode::InvalidState, "Reader is closed."));
        }
        return Result<std::uint64_t>::success(position_);
    }

    [[nodiscard]] Result<std::size_t> read(
        std::span<std::byte> destination) override
    {
        if (closed_) {
            return Result<std::size_t>::failure(
                make_error(ErrorCode::InvalidState, "Reader is closed."));
        }
        if (!reference_.permissions().can_read()) {
            return Result<std::size_t>::failure(
                make_error(ErrorCode::AccessDenied, "Read permission is required."));
        }
        if (failNextRead_) {
            failNextRead_ = false;
            return Result<std::size_t>::failure(
                make_error(ErrorCode::IoFailure, "Synthetic read failure."));
        }
        if (destination.empty() || position_ >= content_.size()) {
            return Result<std::size_t>::success(0);
        }

        const auto remaining = content_.size() - static_cast<std::size_t>(position_);
        const auto transferred = std::min({destination.size(), remaining, maxTransfer_});
        std::copy_n(
            content_.begin() + static_cast<std::ptrdiff_t>(position_),
            static_cast<std::ptrdiff_t>(transferred),
            destination.begin());
        position_ += static_cast<std::uint64_t>(transferred);
        if (failAfterTransfer_) {
            failAfterTransfer_ = false;
            failNextRead_ = true;
        }
        return Result<std::size_t>::success(transferred);
    }

    [[nodiscard]] Status seek_bytes(std::uint64_t absoluteOffset) override
    {
        if (closed_) {
            return Status::failure(make_error(ErrorCode::InvalidState, "Reader is closed."));
        }
        if (!capabilities_.supports(ResourceCapability::CanSeek)) {
            return Status::failure(
                make_error(ErrorCode::UnsupportedOperation, "Seek is unsupported."));
        }
        if (absoluteOffset > content_.size()) {
            return Status::failure(
                make_error(ErrorCode::OutOfRange, "Seek is outside the resource."));
        }
        position_ = absoluteOffset;
        return Status::success();
    }

    [[nodiscard]] Status close() override
    {
        closed_ = true;
        return Status::success();
    }

    void limit_transfer(std::size_t maximum) noexcept
    {
        maxTransfer_ = maximum;
    }

    void fail_after_next_partial_transfer(std::size_t maximum) noexcept
    {
        maxTransfer_ = maximum;
        failAfterTransfer_ = true;
    }

private:
    ResourceReference reference_;
    std::vector<std::byte> content_;
    ResourceCapabilities capabilities_;
    std::uint64_t position_{0};
    std::size_t maxTransfer_{std::numeric_limits<std::size_t>::max()};
    bool failNextRead_{false};
    bool failAfterTransfer_{false};
    bool closed_{false};
};

class FakeWriter final : public IResourceWriter {
public:
    FakeWriter(ResourceReference reference, ResourceCapabilities capabilities)
        : reference_(std::move(reference))
        , capabilities_(capabilities)
    {
    }

    [[nodiscard]] const ResourceReference& reference() const noexcept override
    {
        return reference_;
    }

    [[nodiscard]] ResourceCapabilities capabilities() const noexcept override
    {
        return capabilities_;
    }

    [[nodiscard]] Result<std::uint64_t> position_bytes() const override
    {
        if (closed_) {
            return Result<std::uint64_t>::failure(
                make_error(ErrorCode::InvalidState, "Writer is closed."));
        }
        return Result<std::uint64_t>::success(position_);
    }

    [[nodiscard]] Result<std::size_t> write(
        std::span<const std::byte> source) override
    {
        if (closed_) {
            return Result<std::size_t>::failure(
                make_error(ErrorCode::InvalidState, "Writer is closed."));
        }
        if (!reference_.permissions().can_write()) {
            return Result<std::size_t>::failure(
                make_error(ErrorCode::AccessDenied, "Write permission is required."));
        }
        if (source.empty()) {
            return Result<std::size_t>::success(0);
        }
        if (noProgressFailure_) {
            noProgressFailure_ = false;
            return Result<std::size_t>::failure(
                make_error(ErrorCode::IoFailure, "Write made no progress."));
        }

        const auto transferred = std::min(source.size(), maxTransfer_);
        if (position_ > std::numeric_limits<std::size_t>::max() - transferred) {
            return Result<std::size_t>::failure(
                make_error(ErrorCode::IntegerOverflow, "Write size overflow."));
        }
        const auto end = static_cast<std::size_t>(position_) + transferred;
        if (end > content_.size()) {
            content_.resize(end);
        }
        std::copy_n(
            source.begin(),
            static_cast<std::ptrdiff_t>(transferred),
            content_.begin() + static_cast<std::ptrdiff_t>(position_));
        position_ += static_cast<std::uint64_t>(transferred);
        return Result<std::size_t>::success(transferred);
    }

    [[nodiscard]] Status seek_bytes(std::uint64_t absoluteOffset) override
    {
        if (closed_) {
            return Status::failure(make_error(ErrorCode::InvalidState, "Writer is closed."));
        }
        if (!capabilities_.supports(ResourceCapability::CanSeek)) {
            return Status::failure(
                make_error(ErrorCode::UnsupportedOperation, "Seek is unsupported."));
        }
        if (absoluteOffset > content_.size()) {
            return Status::failure(
                make_error(ErrorCode::OutOfRange, "Seek is outside the resource."));
        }
        position_ = absoluteOffset;
        return Status::success();
    }

    [[nodiscard]] Status resize_bytes(std::uint64_t sizeBytes) override
    {
        if (closed_) {
            return Status::failure(make_error(ErrorCode::InvalidState, "Writer is closed."));
        }
        if (!capabilities_.supports(ResourceCapability::CanResize)) {
            return Status::failure(
                make_error(ErrorCode::UnsupportedOperation, "Resize is unsupported."));
        }
        if (sizeBytes > std::numeric_limits<std::size_t>::max()) {
            return Status::failure(
                make_error(ErrorCode::OutOfRange, "Requested size is too large."));
        }
        content_.resize(static_cast<std::size_t>(sizeBytes));
        position_ = std::min(position_, sizeBytes);
        return Status::success();
    }

    [[nodiscard]] Status flush() override
    {
        if (closed_) {
            return Status::failure(make_error(ErrorCode::InvalidState, "Writer is closed."));
        }
        if (!capabilities_.supports(ResourceCapability::CanFlush)) {
            return Status::failure(
                make_error(ErrorCode::UnsupportedOperation, "Flush is unsupported."));
        }
        if (flushFailure_) {
            return Status::failure(
                make_error(ErrorCode::IoFailure, "Synthetic flush failure."));
        }
        ++flushCount_;
        return Status::success();
    }

    [[nodiscard]] Status close() override
    {
        closed_ = true;
        return Status::success();
    }

    [[nodiscard]] const std::vector<std::byte>& content() const noexcept
    {
        return content_;
    }

    [[nodiscard]] std::size_t flush_count() const noexcept
    {
        return flushCount_;
    }

    void limit_transfer(std::size_t maximum) noexcept
    {
        maxTransfer_ = maximum;
    }

    void fail_next_without_progress() noexcept
    {
        noProgressFailure_ = true;
    }

    void set_flush_failure(bool enabled) noexcept
    {
        flushFailure_ = enabled;
    }

private:
    ResourceReference reference_;
    ResourceCapabilities capabilities_;
    std::vector<std::byte> content_;
    std::uint64_t position_{0};
    std::size_t maxTransfer_{std::numeric_limits<std::size_t>::max()};
    std::size_t flushCount_{0};
    bool noProgressFailure_{false};
    bool flushFailure_{false};
    bool closed_{false};
};

class FakePlaybackService final : public IAudioPlaybackService {
public:
    explicit FakePlaybackService(std::optional<FrameCount> nextDuration)
        : nextDuration_(nextDuration)
    {
    }

    [[nodiscard]] Status prepare(const ResourceReference& source) override
    {
        if (failNextPrepare_) {
            failNextPrepare_ = false;
            return Status::failure(
                make_error(ErrorCode::ResourceNotFound, "Synthetic source is unavailable."));
        }
        if (!source.permissions().can_read()) {
            return Status::failure(
                make_error(ErrorCode::AccessDenied, "Playback requires read permission."));
        }
        source_ = source;
        state_ = PlaybackState::STOPPED;
        position_ = FrameIndex{0};
        duration_ = nextDuration_;
        loop_.reset();
        return Status::success();
    }

    [[nodiscard]] Status clear() override
    {
        source_.reset();
        state_ = PlaybackState::NO_SOURCE;
        position_ = FrameIndex{0};
        duration_.reset();
        loop_.reset();
        return Status::success();
    }

    [[nodiscard]] Status play() override
    {
        if (!source_ || (state_ != PlaybackState::STOPPED
                         && state_ != PlaybackState::PAUSED)) {
            return invalid_transition();
        }
        state_ = PlaybackState::PLAYING;
        return Status::success();
    }

    [[nodiscard]] Status pause() override
    {
        if (!source_ || state_ != PlaybackState::PLAYING) {
            return invalid_transition();
        }
        state_ = PlaybackState::PAUSED;
        return Status::success();
    }

    [[nodiscard]] Status stop() override
    {
        if (!source_) {
            return invalid_transition();
        }
        state_ = PlaybackState::STOPPED;
        position_ = FrameIndex{0};
        return Status::success();
    }

    [[nodiscard]] Status seek(FrameIndex position) override
    {
        if (!source_) {
            return invalid_transition();
        }
        if (position.value() < 0) {
            return Status::failure(
                make_error(ErrorCode::OutOfRange, "Playback position cannot be negative."));
        }
        if (duration_ && position.value() > duration_->value()) {
            return Status::failure(
                make_error(ErrorCode::OutOfRange, "Playback position exceeds duration."));
        }
        position_ = position;
        return Status::success();
    }

    [[nodiscard]] Status set_loop(std::optional<FrameRange> loop) override
    {
        if (!source_) {
            return invalid_transition();
        }
        if (loop) {
            if (loop->begin().value() < 0 || loop->end().value() <= loop->begin().value()) {
                return Status::failure(
                    make_error(ErrorCode::InvalidFrameRange, "Playback loop must be non-empty."));
            }
            if (duration_ && loop->end().value() > duration_->value()) {
                return Status::failure(
                    make_error(ErrorCode::OutOfRange, "Playback loop exceeds duration."));
            }
        }
        loop_ = loop;
        return Status::success();
    }

    [[nodiscard]] Result<PlaybackSnapshot> snapshot() const override
    {
        return Result<PlaybackSnapshot>::success(
            PlaybackSnapshot{state_, position_, duration_, loop_});
    }

    void fail_next_prepare() noexcept
    {
        failNextPrepare_ = true;
    }

private:
    [[nodiscard]] static Status invalid_transition()
    {
        return Status::failure(
            make_error(ErrorCode::InvalidState, "Playback command is invalid in this state."));
    }

    std::optional<ResourceReference> source_;
    PlaybackState state_{PlaybackState::NO_SOURCE};
    FrameIndex position_{0};
    std::optional<FrameCount> duration_;
    std::optional<FrameRange> loop_;
    std::optional<FrameCount> nextDuration_;
    bool failNextPrepare_{false};
};

[[nodiscard]] bool output_is_proven_distinct(
    const ResourceReference& source,
    const ResourceReference& output) noexcept
{
    // A mismatching opaque identity cannot prove that two provider locators do
    // not resolve to the same physical object.
    static_cast<void>(source);
    static_cast<void>(output);
    return false;
}

class PlatformContractsTest final : public QObject {
    Q_OBJECT

private slots:
    void resourceReferenceContract();
    void readerContract();
    void writerContract();
    void playbackContract();
    void errorTaxonomyContract();
};

void PlatformContractsTest::resourceReferenceContract()
{
    auto valid = ResourceReference::create(
        "local.fixture-1", "opaque:Alpha/../beta", true, false, "Synthetic display");
    QVERIFY(valid.value() != nullptr);
    QCOMPARE(valid.value()->provider_id(), std::string("local.fixture-1"));
    QCOMPARE(valid.value()->locator(), std::string("opaque:Alpha/../beta"));
    QVERIFY(valid.value()->permissions().can_read());
    QVERIFY(!valid.value()->permissions().can_write());
    QCOMPARE(valid.value()->display_name(), std::string("Synthetic display"));

    constexpr std::array<std::string_view, 5> invalidProviders{
        "", "Uppercase", "1leading", "has space", "has/slash"};
    for (const std::string_view provider : invalidProviders) {
        auto rejected = ResourceReference::create(
            std::string(provider), "synthetic", true, false);
        QVERIFY(rejected.error() != nullptr);
        QCOMPARE(rejected.error()->code(), ErrorCode::InvalidArgument);
    }
    auto emptyLocator = ResourceReference::create("fixture", "", true, false);
    QVERIFY(emptyLocator.error() != nullptr);
    auto emptyPermissions = ResourceReference::create("fixture", "synthetic", false, false);
    QVERIFY(emptyPermissions.error() != nullptr);

    const auto original = make_reference(true, false, "same", "first");
    const auto changedMetadata = make_reference(false, true, "same", "second");
    const auto changedLocator = make_reference(true, true, "different", "first");
    const auto changedProviderResult = ResourceReference::create(
        "other.provider", "same", true, false, "first");
    QVERIFY(changedProviderResult.value() != nullptr);
    QVERIFY(original.same_resource_identity(changedMetadata));
    QVERIFY(!original.same_resource_identity(changedLocator));
    QVERIFY(!original.same_resource_identity(*changedProviderResult.value()));
    QVERIFY(!output_is_proven_distinct(original, changedLocator));
}

void PlatformContractsTest::readerContract()
{
    const std::vector<std::byte> bytes{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}};
    FakeReader reader{make_reference(true, false), bytes, reader_capabilities()};

    std::array<std::byte, 0> empty{};
    auto emptyRead = reader.read(empty);
    QVERIFY(emptyRead.value() != nullptr);
    QCOMPARE(*emptyRead.value(), std::size_t{0});
    QCOMPARE(*reader.position_bytes().value(), std::uint64_t{0});

    std::array<std::byte, 7> destination{};
    destination.fill(std::byte{99});
    auto fullRead = reader.read(destination);
    QVERIFY(fullRead.value() != nullptr);
    QCOMPARE(*fullRead.value(), std::size_t{5});
    QCOMPARE(*reader.position_bytes().value(), std::uint64_t{5});
    QVERIFY(std::equal(bytes.begin(), bytes.end(), destination.begin()));
    QCOMPARE(destination[5], std::byte{99});
    QCOMPARE(destination[6], std::byte{99});
    auto eofRead = reader.read(destination);
    QVERIFY(eofRead.value() != nullptr);
    QCOMPARE(*eofRead.value(), std::size_t{0});

    QVERIFY(reader.seek_bytes(2));
    QVERIFY(reader.seek_bytes(2));
    QCOMPARE(*reader.position_bytes().value(), std::uint64_t{2});
    auto failedSeek = reader.seek_bytes(6);
    QVERIFY(failedSeek.error() != nullptr);
    QCOMPARE(failedSeek.error()->code(), ErrorCode::OutOfRange);
    QCOMPARE(*reader.position_bytes().value(), std::uint64_t{2});

    FakeReader partial{make_reference(true, false), bytes, reader_capabilities()};
    partial.limit_transfer(2);
    auto shortRead = partial.read(destination);
    QVERIFY(shortRead.value() != nullptr);
    QCOMPARE(*shortRead.value(), std::size_t{2});
    QVERIFY(partial.seek_bytes(0));
    partial.fail_after_next_partial_transfer(2);
    auto beforeFailure = partial.read(destination);
    QVERIFY(beforeFailure.value() != nullptr);
    QCOMPARE(*beforeFailure.value(), std::size_t{2});
    auto failure = partial.read(destination);
    QVERIFY(failure.error() != nullptr);
    QCOMPARE(failure.error()->code(), ErrorCode::IoFailure);
    QCOMPARE(*partial.position_bytes().value(), std::uint64_t{2});

    FakeReader sequential{
        make_reference(true, false), bytes, reader_capabilities(false, false)};
    auto unknownSize = sequential.size_bytes();
    QVERIFY(unknownSize.error() != nullptr);
    QCOMPARE(unknownSize.error()->code(), ErrorCode::UnsupportedOperation);
    auto unsupportedSeek = sequential.seek_bytes(0);
    QVERIFY(unsupportedSeek.error() != nullptr);
    QCOMPARE(unsupportedSeek.error()->code(), ErrorCode::UnsupportedOperation);
    QVERIFY(sequential.read(destination));

    FakeReader denied{make_reference(false, true), bytes, reader_capabilities()};
    auto deniedRead = denied.read(destination);
    QVERIFY(deniedRead.error() != nullptr);
    QCOMPARE(deniedRead.error()->code(), ErrorCode::AccessDenied);

    QVERIFY(reader.close());
    QVERIFY(reader.close());
    QVERIFY(reader.read(destination).error() != nullptr);
    QVERIFY(reader.seek_bytes(0).error() != nullptr);
    QVERIFY(reader.size_bytes().error() != nullptr);
    QVERIFY(reader.position_bytes().error() != nullptr);
}

void PlatformContractsTest::writerContract()
{
    FakeWriter writer{make_reference(false, true), writer_capabilities()};
    const std::array<std::byte, 5> bytes{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}};
    const std::span<const std::byte> empty{};
    auto emptyWrite = writer.write(empty);
    QVERIFY(emptyWrite.value() != nullptr);
    QCOMPARE(*emptyWrite.value(), std::size_t{0});
    QCOMPARE(*writer.position_bytes().value(), std::uint64_t{0});

    FakeWriter fullWriter{make_reference(false, true), writer_capabilities()};
    auto fullWrite = fullWriter.write(bytes);
    QVERIFY(fullWrite.value() != nullptr);
    QCOMPARE(*fullWrite.value(), bytes.size());
    QCOMPARE(*fullWriter.position_bytes().value(), std::uint64_t{5});
    QVERIFY(fullWriter.content() == std::vector<std::byte>(bytes.begin(), bytes.end()));

    writer.limit_transfer(3);
    auto partialWrite = writer.write(bytes);
    QVERIFY(partialWrite.value() != nullptr);
    QCOMPARE(*partialWrite.value(), std::size_t{3});
    QCOMPARE(*writer.position_bytes().value(), std::uint64_t{3});
    QCOMPARE(writer.content().size(), std::size_t{3});

    writer.fail_next_without_progress();
    auto noProgress = writer.write(bytes);
    QVERIFY(noProgress.error() != nullptr);
    QCOMPARE(noProgress.error()->code(), ErrorCode::IoFailure);
    QCOMPARE(*writer.position_bytes().value(), std::uint64_t{3});

    QVERIFY(writer.seek_bytes(1));
    const std::array<std::byte, 1> replacement{std::byte{9}};
    QVERIFY(writer.write(replacement));
    QCOMPARE(writer.content()[1], std::byte{9});
    const auto positionBeforeFailedSeek = *writer.position_bytes().value();
    QVERIFY(writer.seek_bytes(99).error() != nullptr);
    QCOMPARE(*writer.position_bytes().value(), positionBeforeFailedSeek);

    QVERIFY(writer.resize_bytes(8));
    QCOMPARE(writer.content().size(), std::size_t{8});
    QVERIFY(writer.seek_bytes(7));
    QVERIFY(writer.resize_bytes(2));
    QCOMPARE(writer.content().size(), std::size_t{2});
    QCOMPARE(*writer.position_bytes().value(), std::uint64_t{2});

    QVERIFY(writer.flush());
    QCOMPARE(writer.flush_count(), std::size_t{1});
    writer.set_flush_failure(true);
    auto flushFailure = writer.flush();
    QVERIFY(flushFailure.error() != nullptr);
    QCOMPARE(flushFailure.error()->code(), ErrorCode::IoFailure);
    QCOMPARE(writer.flush_count(), std::size_t{1});

    FakeWriter unsupported{
        make_reference(false, true), writer_capabilities(false, false, false)};
    QVERIFY(unsupported.seek_bytes(0).error() != nullptr);
    QVERIFY(unsupported.resize_bytes(1).error() != nullptr);
    QVERIFY(unsupported.flush().error() != nullptr);
    QCOMPARE(*unsupported.position_bytes().value(), std::uint64_t{0});
    QCOMPARE(unsupported.content().size(), std::size_t{0});

    FakeWriter denied{make_reference(true, false), writer_capabilities()};
    auto deniedWrite = denied.write(bytes);
    QVERIFY(deniedWrite.error() != nullptr);
    QCOMPARE(deniedWrite.error()->code(), ErrorCode::AccessDenied);

    QVERIFY(writer.close());
    QVERIFY(writer.close());
    QVERIFY(writer.write(bytes).error() != nullptr);
    QVERIFY(writer.seek_bytes(0).error() != nullptr);
    QVERIFY(writer.resize_bytes(0).error() != nullptr);
    QVERIFY(writer.flush().error() != nullptr);
    QVERIFY(writer.position_bytes().error() != nullptr);
}

void PlatformContractsTest::playbackContract()
{
    auto durationResult = FrameCount::create(100);
    QVERIFY(durationResult.value() != nullptr);
    FakePlaybackService service{*durationResult.value()};

    auto initial = service.snapshot();
    QVERIFY(initial.value() != nullptr);
    QCOMPARE(initial.value()->state, PlaybackState::NO_SOURCE);
    QCOMPARE(initial.value()->position, FrameIndex{0});
    QVERIFY(!initial.value()->duration);
    QVERIFY(!initial.value()->loop);
    QVERIFY(service.play().error() != nullptr);
    QVERIFY(service.pause().error() != nullptr);
    QVERIFY(service.stop().error() != nullptr);
    QVERIFY(service.seek(FrameIndex{0}).error() != nullptr);
    QVERIFY(service.set_loop(std::nullopt).error() != nullptr);
    QVERIFY(service.clear());
    QVERIFY(service.clear());

    const auto source = make_reference(true, false);
    QVERIFY(service.prepare(source));
    auto prepared = service.snapshot();
    QVERIFY(prepared.value() != nullptr);
    QCOMPARE(prepared.value()->state, PlaybackState::STOPPED);
    QCOMPARE(prepared.value()->position, FrameIndex{0});
    QVERIFY(prepared.value()->duration);
    QCOMPARE(prepared.value()->duration->value(), std::int64_t{100});

    QVERIFY(service.play());
    QVERIFY(service.pause());
    QVERIFY(service.play());
    QVERIFY(service.stop());
    QVERIFY(service.stop());
    QCOMPARE(service.snapshot().value()->position, FrameIndex{0});

    QVERIFY(service.seek(FrameIndex{40}));
    auto beforeInvalidSeek = *service.snapshot().value();
    auto negativeSeek = service.seek(FrameIndex{-1});
    QVERIFY(negativeSeek.error() != nullptr);
    QCOMPARE(*service.snapshot().value(), beforeInvalidSeek);
    auto beyondDuration = service.seek(FrameIndex{101});
    QVERIFY(beyondDuration.error() != nullptr);
    QCOMPARE(*service.snapshot().value(), beforeInvalidSeek);

    auto validLoopResult = FrameRange::create(FrameIndex{10}, FrameIndex{30});
    QVERIFY(validLoopResult.value() != nullptr);
    QVERIFY(service.set_loop(*validLoopResult.value()));
    QVERIFY(service.snapshot().value()->loop);
    auto emptyLoopResult = FrameRange::create(FrameIndex{20}, FrameIndex{20});
    QVERIFY(emptyLoopResult.value() != nullptr);
    const auto beforeInvalidLoop = *service.snapshot().value();
    QVERIFY(service.set_loop(*emptyLoopResult.value()).error() != nullptr);
    QCOMPARE(*service.snapshot().value(), beforeInvalidLoop);
    auto negativeLoopResult = FrameRange::create(FrameIndex{-1}, FrameIndex{10});
    QVERIFY(negativeLoopResult.value() != nullptr);
    QVERIFY(service.set_loop(*negativeLoopResult.value()).error() != nullptr);
    auto beyondLoopResult = FrameRange::create(FrameIndex{90}, FrameIndex{101});
    QVERIFY(beyondLoopResult.value() != nullptr);
    QVERIFY(service.set_loop(*beyondLoopResult.value()).error() != nullptr);
    QCOMPARE(*service.snapshot().value(), beforeInvalidLoop);
    QVERIFY(service.set_loop(std::nullopt));
    QVERIFY(!service.snapshot().value()->loop);

    const auto beforePrepareFailure = *service.snapshot().value();
    service.fail_next_prepare();
    auto prepareFailure = service.prepare(source);
    QVERIFY(prepareFailure.error() != nullptr);
    QCOMPARE(prepareFailure.error()->code(), ErrorCode::ResourceNotFound);
    QCOMPARE(*service.snapshot().value(), beforePrepareFailure);
    auto deniedPrepare = service.prepare(make_reference(false, true));
    QVERIFY(deniedPrepare.error() != nullptr);
    QCOMPARE(deniedPrepare.error()->code(), ErrorCode::AccessDenied);
    QCOMPARE(*service.snapshot().value(), beforePrepareFailure);

    QVERIFY(service.clear());
    auto cleared = service.snapshot();
    QCOMPARE(cleared.value()->state, PlaybackState::NO_SOURCE);
    QVERIFY(!cleared.value()->duration);
    QVERIFY(!cleared.value()->loop);
}

void PlatformContractsTest::errorTaxonomyContract()
{
    constexpr std::array<std::pair<ErrorCode, std::string_view>, 5> additions{{
        {ErrorCode::ResourceNotFound, "resource_not_found"},
        {ErrorCode::AccessDenied, "access_denied"},
        {ErrorCode::IoFailure, "io_failure"},
        {ErrorCode::UnsupportedOperation, "unsupported_operation"},
        {ErrorCode::InvalidState, "invalid_state"},
    }};
    for (const auto& [code, token] : additions) {
        QCOMPARE(error_code_token(code), token);
        auto parsed = parse_error_code(token);
        QVERIFY(parsed.value() != nullptr);
        QCOMPARE(*parsed.value(), code);
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::PlatformContractsTest)

#include "test_platform_contracts.moc"
