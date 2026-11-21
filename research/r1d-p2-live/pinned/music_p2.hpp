#pragma once
#include "stream.hpp"

namespace research {

// A fixed-lookahead linked envelope. The safety stage still verifies actual PCM.
class PredictiveMusic {
public:
    PredictiveMusic(int fs, int channels, double drive_db, double ceiling)
        : channels_(channels), delay_(size_t(std::lround(fs * .0015))),
          drive_(std::pow(10., drive_db / 20.)),
          release_(std::exp(-1.0 / (.100 * fs))), target_(ceiling * .985),
          hold_frames_(fs / 20) {
        gains_.fill(1.);
    }

    size_t delay() const noexcept { return delay_; }

    bool push(Frame input, Frame& output) noexcept {
        Frame driven{input[0] * drive_, channels_ == 2 ? input[1] * drive_ : 0.};
        meter_.peak = 0.;
        meter_.push(driven, channels_);
        // Include sample peaks for envelope behavior at the stream boundaries.
        const double peak = std::max({meter_.peak, std::abs(driven[0]), std::abs(driven[1])});
        const double required = peak > target_ ? target_ / peak : 1.;
        if (required <= gain_ && required < 1.) {
            gain_ = required;
            hold_ = hold_frames_;
        } else if (hold_ > 0) {
            --hold_;
        } else {
            gain_ = std::min(required, 1. - (1. - gain_) * release_);
        }
        const size_t width = delay_ + 1;
        // The exact-unity case preserves bytes of a drive-zero signal.
        const double old = gains_[index_];
        nonunity_ -= old != 1.;
        nonunity_ += gain_ != 1.;
        add(gain_ - old);
        gains_[index_] = gain_;
        delay_line_[index_] = driven;
        index_ = (index_ + 1) % width;
        if (filled_ < width) ++filled_;
        if (filled_ <= delay_) return false;
        const size_t oldest = index_;
        const double smooth = nonunity_ ? std::clamp(1. + delta_ / width, 0., 1.) : 1.;
        if (!nonunity_) delta_ = correction_ = 0.;
        output = delay_line_[oldest];
        if (smooth != 1.) for (int c=0;c<channels_;++c) output[c] *= smooth;
        return true;
    }

private:
    void add(double value) noexcept {
        const double adjusted = value - correction_;
        const double sum = delta_ + adjusted;
        correction_ = (sum - delta_) - adjusted;
        delta_ = sum;
    }

    static constexpr size_t slots = 289;
    int channels_;
    size_t delay_, index_=0, filled_=0, nonunity_=0;
    double drive_, release_, target_, gain_=1., delta_=0., correction_=0.;
    int hold_frames_, hold_=0;
    Meter meter_{};
    std::array<Frame, slots> delay_line_{};
    std::array<double, slots> gains_{};
};

} // namespace research
