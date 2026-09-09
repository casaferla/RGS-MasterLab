#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>

#include <type_traits>

static_assert(std::is_move_constructible_v<rgsml::audio::AudioBuffer>);
static_assert(std::is_move_constructible_v<rgsml::audio::WavReader>);

int main()
{
    return rgsml::audio::kWavDecoderContractVersion.empty() ? 1 : 0;
}
