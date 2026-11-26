#pragma once
// Render-local observational adapter; never part of search iteration.
// Transfers the EXACT already-applied C00 Attack/Sustain gains into the
// 20-ms full-track collector without a duplicate envelope implementation.
#include "transient_activity_collector.hpp"
#include <rgsml/dsp/linked_dsp_activity_observation.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace rgsml::render::linked_activity {

class TransientC00ActivitySink final
    : public rgsml::dsp::internal::ITransientC00ActivitySink {
public:
    explicit TransientC00ActivitySink(
        TransientActivityCollector& collector) noexcept
        : collector_(collector) {}

    void on_successful_block(
        std::uint64_t first_absolute_frame,
        std::span<const rgsml::dsp::internal::TransientC00AppliedGains> gains)
        noexcept override
    {
        ++observed_blocks_;
        for (std::size_t i = 0; i < gains.size(); ++i) {
            if (i > std::numeric_limits<std::uint64_t>::max()
                        - first_absolute_frame
                || !collector_.push(first_absolute_frame + i,
                                    gains[i].attack_db,
                                    gains[i].sustain_db)) {
                observation_failed_ = true;
                return; // Audio has already been committed by C00.
            }
            ++observed_frames_;
            last_attack_db_ = gains[i].attack_db;
            last_sustain_db_ = gains[i].sustain_db;
        }
    }

    [[nodiscard]] std::size_t observed_blocks() const noexcept {
        return observed_blocks_;
    }
    [[nodiscard]] std::uint64_t observed_frames() const noexcept {
        return observed_frames_;
    }
    [[nodiscard]] bool observation_failed() const noexcept {
        return observation_failed_
            || collector_.status() != ObservationStatus::OK;
    }
    [[nodiscard]] double last_attack_db() const noexcept {
        return last_attack_db_;
    }
    [[nodiscard]] double last_sustain_db() const noexcept {
        return last_sustain_db_;
    }

private:
    TransientActivityCollector& collector_;
    std::size_t observed_blocks_{};
    std::uint64_t observed_frames_{};
    bool observation_failed_{false};
    double last_attack_db_{};
    double last_sustain_db_{};
};

} // namespace rgsml::render::linked_activity
