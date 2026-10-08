#pragma once

#include <rgsml/core/frame_time.hpp>
#include <rgsml/core/realization_identity.hpp>
#include <rgsml/core/resource_reference.hpp>
#include <rgsml/core/result.hpp>

#include <optional>

namespace rgsml::core {

enum class PlaybackState {
    NO_SOURCE,
    STOPPED,
    PLAYING,
    PAUSED,
};

struct PlaybackSnapshot final {
    PlaybackState state{PlaybackState::NO_SOURCE};
    FrameIndex position{0};
    std::optional<FrameCount> duration;
    std::optional<FrameRange> loop;
    AudibleRealizationState audibleRealization;
    std::uint64_t traversalSerial{0};
    std::uint64_t seekSerial{0};
    std::uint64_t loopWrapCount{0};

    [[nodiscard]] bool operator==(const PlaybackSnapshot&) const = default;
};

// The service is an inward-facing, serialized control-plane port. prepare()
// requires a read-enabled reference and, on success, establishes STOPPED at
// frame zero with no loop. Failed commands preserve the previous snapshot.
// play(), pause(), stop(), seek(), and set_loop() require a prepared source;
// stop() and clear() are idempotent in their valid domains. Loops are absolute,
// non-negative, non-empty half-open ranges and must fit a known duration.
// Device selection, decoding, buffering, threading and platform state machines
// belong to future adapter layers.
class IAudioPlaybackService {
public:
    virtual ~IAudioPlaybackService() noexcept = default;

    [[nodiscard]] virtual Status prepare(const ResourceReference& source) = 0;
    [[nodiscard]] virtual Status clear() = 0;
    [[nodiscard]] virtual Status play() = 0;
    [[nodiscard]] virtual Status pause() = 0;
    [[nodiscard]] virtual Status stop() = 0;
    [[nodiscard]] virtual Status seek(FrameIndex position) = 0;
    [[nodiscard]] virtual Status set_loop(std::optional<FrameRange> loop) = 0;
    [[nodiscard]] virtual Result<PlaybackSnapshot> snapshot() const = 0;
};

}  // namespace rgsml::core
