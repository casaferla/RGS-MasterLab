#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/audio/wav_writer.hpp>

#include <type_traits>

static_assert(std::is_move_constructible_v<rgsml::audio::AudioBuffer>);
static_assert(std::is_move_constructible_v<rgsml::audio::WavReader>);
static_assert(std::is_final_v<rgsml::audio::WavWriter>);

int main()
{
    return rgsml::audio::kWavDecoderContractVersion.empty() ? 1 : 0;
}
