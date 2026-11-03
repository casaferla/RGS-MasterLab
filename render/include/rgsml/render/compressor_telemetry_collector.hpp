#pragma once

#include <rgsml/audio/audio_format.hpp>
#include <rgsml/dsp/compressor_module.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/render/render_result.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rgsml::render {

class CompressorTelemetryCollector final : public rgsml::dsp::ICompressorTelemetrySink {
public:
    CompressorTelemetryCollector(
        std::int64_t start_frame,
        std::int64_t total_frames,
        std::uint32_t sample_rate_hz,
        rgsml::audio::ChannelLayout channel_layout,
        rgsml::dsp::CompressorChannelLink channel_link,
        rgsml::dsp::ModuleInstanceId instance_id,
        std::uint64_t chain_revision,
        std::size_t max_memory_bytes = 128U * 1024U * 1024U,
        std::optional<rgsml::core::RealizationId> realization_id = std::nullopt);

    ~CompressorTelemetryCollector() override = default;

    void push_frame_telemetry(
        std::int64_t absolute_frame,
        const rgsml::dsp::CompressorFrameTelemetry& frame) noexcept override;

    [[nodiscard]] CompressorTelemetrySidecar build_sidecar();

private:
    std::int64_t start_frame_{0};
    std::int64_t total_frames_{0};
    std::uint32_t sample_rate_hz_{44100};
    rgsml::audio::ChannelLayout channel_layout_{rgsml::audio::ChannelLayout::STEREO_LR};
    rgsml::dsp::CompressorChannelLink channel_link_{rgsml::dsp::CompressorChannelLink::LINKED_MAX};
    rgsml::dsp::ModuleInstanceId instance_id_;
    std::uint64_t chain_revision_{0};
    std::optional<rgsml::core::RealizationId> realization_id_{std::nullopt};
    std::size_t num_lanes_{1};

    std::int64_t expected_next_frame_{0};
    std::int64_t pushed_frame_count_{0};
    bool telemetry_failed_{false};

    std::vector<CompressorTelemetryLane> lanes_;
    std::vector<std::vector<double>> sums_;
    std::vector<std::vector<std::uint32_t>> counts_;
};

}  // namespace rgsml::render
