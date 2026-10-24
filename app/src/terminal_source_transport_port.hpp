#pragma once

#include <rgsml/core/audio_playback_service.hpp>
#include <rgsml/core/result.hpp>

#include <memory>
#include <optional>

namespace rgsml::app {

// PRIVATE M17-only virtual control port: avoids linking terminal DSP/audio
// implementations into otherwise independent playback and UI executables.
// The owning gate issues a weak liveness lease which expires on detach.
// This port never certifies sink-consumed frames or publishes PROCESSED.
class TerminalSourceTransportPort {
public:
    struct DisplaySnapshot final {
        core::PlaybackState state;
        core::FrameIndex position;
        core::FrameCount duration;
        std::optional<core::FrameRange> loop;
    };

    virtual ~TerminalSourceTransportPort() = default;
    [[nodiscard]] virtual bool has_session() const noexcept = 0;
    [[nodiscard]] virtual bool owns_playback(
        const core::IAudioPlaybackService& service) const noexcept = 0;
    [[nodiscard]] virtual std::weak_ptr<const void>
    session_liveness() const noexcept = 0;
    [[nodiscard]] virtual core::Result<core::SampleRate>
    source_rate() const = 0;
    [[nodiscard]] virtual core::Result<DisplaySnapshot>
    source_display_snapshot() const = 0;
    [[nodiscard]] virtual core::Status seek_source(
        core::FrameIndex position) = 0;
    [[nodiscard]] virtual core::Status set_source_loop(
        std::optional<core::FrameRange> region) = 0;
};

} // namespace rgsml::app
