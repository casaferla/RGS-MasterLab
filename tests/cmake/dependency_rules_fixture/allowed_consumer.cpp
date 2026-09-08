#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/resource_io.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace {

using namespace rgsml::core;

class Reader final : public IResourceReader {
public:
    explicit Reader(ResourceReference reference)
        : reference_(std::move(reference))
    {
    }
    const ResourceReference& reference() const noexcept override { return reference_; }
    ResourceCapabilities capabilities() const noexcept override
    {
        return ResourceCapabilities::create(false, false, false, false);
    }
    Result<std::uint64_t> size_bytes() const override
    {
        return Result<std::uint64_t>::failure(
            Error{ErrorCode::UnsupportedOperation});
    }
    Result<std::uint64_t> position_bytes() const override
    {
        return Result<std::uint64_t>::success(0);
    }
    Result<std::size_t> read(std::span<std::byte>) override
    {
        return Result<std::size_t>::success(0);
    }
    Status seek_bytes(std::uint64_t) override
    {
        return Status::failure(Error{ErrorCode::UnsupportedOperation});
    }
    Status close() override { return Status::success(); }

private:
    ResourceReference reference_;
};

class Writer final : public IResourceWriter {
public:
    explicit Writer(ResourceReference reference)
        : reference_(std::move(reference))
    {
    }
    const ResourceReference& reference() const noexcept override { return reference_; }
    ResourceCapabilities capabilities() const noexcept override
    {
        return ResourceCapabilities::create(false, false, false, false);
    }
    Result<std::uint64_t> position_bytes() const override
    {
        return Result<std::uint64_t>::success(0);
    }
    Result<std::size_t> write(std::span<const std::byte>) override
    {
        return Result<std::size_t>::success(0);
    }
    Status seek_bytes(std::uint64_t) override
    {
        return Status::failure(Error{ErrorCode::UnsupportedOperation});
    }
    Status resize_bytes(std::uint64_t) override
    {
        return Status::failure(Error{ErrorCode::UnsupportedOperation});
    }
    Status flush() override
    {
        return Status::failure(Error{ErrorCode::UnsupportedOperation});
    }
    Status close() override { return Status::success(); }

private:
    ResourceReference reference_;
};

class Playback final : public IAudioPlaybackService {
public:
    Status prepare(const ResourceReference&) override { return Status::success(); }
    Status clear() override { return Status::success(); }
    Status play() override { return Status::success(); }
    Status pause() override { return Status::success(); }
    Status stop() override { return Status::success(); }
    Status seek(FrameIndex) override { return Status::success(); }
    Status set_loop(std::optional<FrameRange>) override { return Status::success(); }
    Result<PlaybackSnapshot> snapshot() const override
    {
        return Result<PlaybackSnapshot>::success(
            PlaybackSnapshot{PlaybackState::NO_SOURCE, FrameIndex{0}, std::nullopt, std::nullopt});
    }
};

}  // namespace

int main()
{
    auto reference = ResourceReference::create(
        "fixture", "synthetic", true, true);
    if (!reference) {
        return 1;
    }
    Reader reader{*reference.value()};
    Writer writer{*reference.value()};
    Playback playback;
    return reader.reference().same_resource_identity(writer.reference())
            && playback.snapshot()
        ? 0
        : 1;
}
