#pragma once
// Independent test-only direct finite-resource convolution. It uses the
// protected M16 authority coefficients, NEVER the M17 copied coefficients or
// M17 detector's ring-buffer implementation. OUTPUT_RATE is supplied by test.
#include "internal/true_peak_itu4x_coefficients.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

namespace rgsml::tests::m17_oracle {
struct Event final {
    std::int64_t tick;
    double peak;
};
// Frozen binary64 midpoint tap literals independently copied from PO delta.
inline constexpr std::array<double, 16> kReferenceMidpoint{{
    -2.239776377948551e-06, 0.00017165472521812235,
    -0.0015146051755935781, 0.0071952023595316566,
    -0.024200410441139873, 0.066061419666173099,
    -0.16826720648957741, 0.62055618513176591,
    0.62055618513176591, -0.16826720648957741,
    0.066061419666173099, -0.024200410441139873,
    0.0071952023595316566, -0.0015146051755935781,
    0.00017165472521812235, -2.239776377948551e-06
}};

// O(N) storage, O(16N) direct convolution, independent of streaming state.
[[nodiscard]] inline std::vector<Event> direct_events(
    std::span<const double> left, std::span<const double> right = {})
{
    const auto count = static_cast<std::int64_t>(left.size());
    const auto qcount = 4*count + 59;
    std::array<std::vector<double>,2> anchors{
        std::vector<double>(static_cast<std::size_t>(qcount),0.0),
        std::vector<double>(static_cast<std::size_t>(qcount),0.0)};
    const std::array<std::span<const double>,2> pcm{left,right};
    const auto nChannels = right.empty() ? 1U : 2U;
    for (std::int64_t j=0; j<qcount; ++j) {
        const std::int64_t frame=j/4;
        const std::size_t phase=static_cast<std::size_t>(j%4);
        for (std::size_t ch=0; ch<nChannels; ++ch) {
            double value=+0.0;
            for (std::int64_t lag=0;lag<12;++lag) {
                const auto inputFrame=frame-lag;
                const double x=(inputFrame>=0 && inputFrame<count)
                    ? pcm[ch][static_cast<std::size_t>(inputFrame)] : +0.0;
                const double product=
                    rgsml::analysis::internal::kTruePeakItu4xPhaseMajor[
                        12*phase+static_cast<std::size_t>(lag)] * x;
                value += product;
            }
            anchors[ch][static_cast<std::size_t>(j)]=value;
        }
    }
    std::vector<Event> emitted;
    emitted.reserve(static_cast<std::size_t>(qcount*2));
    for (std::int64_t j=0;j<qcount;++j) {
        if (j>=8) {
            const auto idx=static_cast<std::size_t>(j-8);
            double peak=std::abs(anchors[0][idx]);
            if(nChannels==2)peak=std::max(peak,std::abs(anchors[1][idx]));
            emitted.push_back(Event{2*j-63,peak});
        }
        double peak=+0.0;
        for(std::size_t ch=0;ch<nChannels;++ch) {
            double v=+0.0;
            for(std::int64_t lag=0;lag<16;++lag) {
                const double a=j>=lag
                    ? anchors[ch][static_cast<std::size_t>(j-lag)] : +0.0;
                v += kReferenceMidpoint[static_cast<std::size_t>(lag)]*a;
            }
            peak=std::max(peak,std::abs(v));
        }
        emitted.push_back(Event{2*j-62,peak});
    }
    return emitted;
}

// Test-only controller replay derived from the frozen physical 8x event list.
// It neither calls TruePeakLimiterControllerState nor reads M17 internal state,
// its event buffer, its applied-gain witness, or production gain_for_frame.
// The entire finite resource (including signed prezero and EOS tail) is
// available to this offline oracle; missing events denote exact zero demand.
// Running on the SAME strict-FP backend permits binary64 replay assertions.
struct ReferenceFrameGain final {
    std::int64_t frame;
    double reduction_db;
    double gain;
};
[[nodiscard]] inline std::vector<ReferenceFrameGain> replay_control_from_events(
    std::span<const Event> events,std::int64_t sample_rate,
    double ceiling_db_tp,std::size_t programme_frames)
{
    const auto half=sample_rate/200;
    const auto remainder=sample_rate%200;
    const std::int64_t h=half+
        ((remainder>100 || (remainder==100 && (half%2)!=0))?1:0);
    const std::int64_t lookahead=8*h;
    const std::int64_t hold_length=sample_rate/25;
    const double threshold=std::pow(10.0,ceiling_db_tp/20.0);
    const double density_coeff=std::exp(
        -1.0/(0.100*static_cast<double>(8*sample_rate)));
    const auto demand=[threshold](double peak)->double {
        return peak>threshold ? 20.0*std::log10(peak/threshold):+0.0;
    };
    std::vector<ReferenceFrameGain> output;
    output.reserve(programme_frames);
    double density=+0.0;
    double previous_reduction=+0.0;
    std::int64_t hold=0;
    std::size_t cursor=0;
    const auto last_tick=8*static_cast<std::int64_t>(programme_frames-1U);
    for(std::int64_t tick=-62;tick<=last_tick;++tick) {
        while(cursor<events.size() && events[cursor].tick<tick)++cursor;
        const bool current_valid=cursor<events.size() &&
            events[cursor].tick==tick;
        const double current=current_valid?events[cursor].peak:+0.0;
        const double required=demand(current);
        double floor=required;
        const auto first=cursor+(current_valid?1U:0U);
        for(auto i=first;i<events.size() &&
            events[i].tick<=tick+lookahead;++i) {
            const double needed=demand(events[i].peak);
            if(needed>0.0) {
                const double distance=static_cast<double>(events[i].tick-tick);
                const double weight=0.5*(1.0+std::cos(
                    std::numbers::pi_v<double>*distance/
                    static_cast<double>(lookahead)));
                floor=std::max(floor,needed*weight);
            }
        }
        const double q=required>0.0?1.0:0.0;
        density=density_coeff*density+(1.0-density_coeff)*q;
        const double remaining=1.0-density;
        const double s=1.0-remaining*remaining*remaining*remaining;
        const double release_ms=20.0+980.0*s;
        const double release_coeff=std::exp(-1.0/(
            (release_ms/1000.0)*static_cast<double>(8*sample_rate)));
        double applied=+0.0;
        if(floor>=previous_reduction) {
            applied=floor;
            hold=hold_length;
        } else if(hold>0) {
            applied=std::max(floor,previous_reduction);
            --hold;
        } else {
            const double candidate=release_coeff*previous_reduction+
                (1.0-release_coeff)*floor;
            applied=std::max(floor,candidate);
        }
        previous_reduction=applied;
        if(tick>=0 && tick%8==0) {
            const double gain=applied==0.0?1.0:
                std::pow(10.0,-applied/20.0);
            output.push_back({tick/8,applied,gain});
        }
    }
    return output;
}
} // namespace rgsml::tests::m17_oracle
