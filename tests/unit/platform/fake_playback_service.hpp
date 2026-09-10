#pragma once

#include <rgsml/core/audio_playback_service.hpp>

#include <optional>

namespace rgsml::tests {

class FakePlaybackService final : public core::IAudioPlaybackService {
public:
    [[nodiscard]] core::Status prepare(
        const core::ResourceReference&) override
    {
        ++prepareCalls;
        if (failPrepare) {
            return core::Status::failure(core::Error{
                core::ErrorCode::UnsupportedOperation,
                "Injected playback prepare failure."});
        }
        state = core::PlaybackState::STOPPED;
        position = core::FrameIndex{0};
        duration = *core::FrameCount::create(preparedDurationFrames).value();
        loop.reset();
        return core::Status::success();
    }

    [[nodiscard]] core::Status clear() override
    {
        ++clearCalls;
        state = core::PlaybackState::NO_SOURCE;
        position = core::FrameIndex{0};
        duration.reset();
        loop.reset();
        return core::Status::success();
    }

    [[nodiscard]] core::Status play() override
    {
        ++playCalls;
        if (!duration) {
            return invalid_state();
        }
        state = core::PlaybackState::PLAYING;
        return core::Status::success();
    }

    [[nodiscard]] core::Status pause() override
    {
        ++pauseCalls;
        if (state != core::PlaybackState::PLAYING) {
            return invalid_state();
        }
        state = core::PlaybackState::PAUSED;
        return core::Status::success();
    }

    [[nodiscard]] core::Status stop() override
    {
        ++stopCalls;
        if (!duration) {
            return invalid_state();
        }
        state = core::PlaybackState::STOPPED;
        position = core::FrameIndex{0};
        return core::Status::success();
    }

    [[nodiscard]] core::Status seek(core::FrameIndex newPosition) override
    {
        if (!duration
            || newPosition.value() < 0
            || newPosition.value() > duration->value()) {
            return core::Status::failure(core::Error{
                core::ErrorCode::OutOfRange,
                "Injected playback seek failure."});
        }
        position = newPosition;
        return core::Status::success();
    }

    [[nodiscard]] core::Status set_loop(
        std::optional<core::FrameRange> newLoop) override
    {
        loop = newLoop;
        return core::Status::success();
    }

    [[nodiscard]] core::Result<core::PlaybackSnapshot> snapshot() const override
    {
        if (failSnapshot) {
            return core::Result<core::PlaybackSnapshot>::failure(core::Error{
                core::ErrorCode::IoFailure,
                "Injected playback snapshot failure."});
        }
        return core::Result<core::PlaybackSnapshot>::success(
            core::PlaybackSnapshot{state, position, duration, loop});
    }

    bool failPrepare{false};
    bool failSnapshot{false};
    std::int64_t preparedDurationFrames{48000};
    int prepareCalls{0};
    int clearCalls{0};
    int playCalls{0};
    int pauseCalls{0};
    int stopCalls{0};
    core::PlaybackState state{core::PlaybackState::NO_SOURCE};
    core::FrameIndex position{0};
    std::optional<core::FrameCount> duration;
    std::optional<core::FrameRange> loop;

private:
    [[nodiscard]] static core::Status invalid_state()
    {
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "Injected invalid playback state."});
    }
};

}  // namespace rgsml::tests
