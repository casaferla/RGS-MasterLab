#ifndef RGSML_TESTS_ORACLES_LIVE_SPECTRUM_ORACLE_HPP
#define RGSML_TESTS_ORACLES_LIVE_SPECTRUM_ORACLE_HPP

#include <cmath>
#include <numbers>
#include <vector>

namespace rgsml::tests::oracle {

struct GeneratedSignal final {
    std::uint32_t sample_rate_hz{44100};
    std::size_t channel_count{2};
    std::vector<float> pcm_interleaved_f32;
    std::vector<std::int16_t> pcm_interleaved_i16;
};

inline GeneratedSignal generate_sine_wave(
    double freqHz,
    double amplitudeDb,
    double durationSec,
    std::uint32_t sampleRateHz = 44100,
    bool stereo = true,
    double phaseOffsetR = 0.0)
{
    GeneratedSignal sig;
    sig.sample_rate_hz = sampleRateHz;
    sig.channel_count = stereo ? 2 : 1;

    const std::size_t totalFrames = static_cast<std::size_t>(durationSec * sampleRateHz);
    const double amp = std::pow(10.0, amplitudeDb / 20.0);

    sig.pcm_interleaved_f32.resize(totalFrames * sig.channel_count);
    sig.pcm_interleaved_i16.resize(totalFrames * sig.channel_count);

    for (std::size_t f = 0; f < totalFrames; ++f) {
        const double t = static_cast<double>(f) / sampleRateHz;
        const double sampleL = amp * std::sin(2.0 * std::numbers::pi * freqHz * t);
        const double sampleR = amp * std::sin(2.0 * std::numbers::pi * freqHz * t + phaseOffsetR);

        sig.pcm_interleaved_f32[f * sig.channel_count + 0] = static_cast<float>(sampleL);
        sig.pcm_interleaved_i16[f * sig.channel_count + 0] = static_cast<std::int16_t>(std::round(sampleL * 32767.0));

        if (stereo) {
            sig.pcm_interleaved_f32[f * sig.channel_count + 1] = static_cast<float>(sampleR);
            sig.pcm_interleaved_i16[f * sig.channel_count + 1] = static_cast<std::int16_t>(std::round(sampleR * 32767.0));
        }
    }

    return sig;
}

inline GeneratedSignal generate_silence(
    double durationSec,
    std::uint32_t sampleRateHz = 44100,
    bool stereo = true)
{
    GeneratedSignal sig;
    sig.sample_rate_hz = sampleRateHz;
    sig.channel_count = stereo ? 2 : 1;
    const std::size_t totalFrames = static_cast<std::size_t>(durationSec * sampleRateHz);
    sig.pcm_interleaved_f32.resize(totalFrames * sig.channel_count, 0.0f);
    sig.pcm_interleaved_i16.resize(totalFrames * sig.channel_count, 0);
    return sig;
}

}  // namespace rgsml::tests::oracle

#endif  // RGSML_TESTS_ORACLES_LIVE_SPECTRUM_ORACLE_HPP
