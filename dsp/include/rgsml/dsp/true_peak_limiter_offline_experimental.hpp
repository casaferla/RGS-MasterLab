#pragma once
// Isolated, opt-in offline experimental M17 renderer: this never replaces
// the registered M17 DSP or changes frozen controller/checkpoint semantics.
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/dsp/true_peak_limiter_module.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <cstdint>
#include <functional>
#include <vector>
namespace rgsml::dsp {
struct M17ExperimentalOfflineResult final {
    audio::AudioBuffer pcm;
    std::vector<TruePeakLimiterAppliedGainFrame> gain_frames;
    std::int64_t delay_frames{0};
};
using M17ExperimentalOfflineCancel=std::function<bool()>;
using M17ExperimentalOfflineProgress=std::function<void(std::int64_t,std::int64_t)>;
[[nodiscard]] core::Result<M17ExperimentalOfflineResult>
render_m17_experimental_offline(audio::AudioBufferView,
    TruePeakLimiterParameters,
    const M17ExperimentalOfflineCancel& cancelled={},
    const M17ExperimentalOfflineProgress& progress={});
}
