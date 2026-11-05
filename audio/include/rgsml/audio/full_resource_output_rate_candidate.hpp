#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/core/result.hpp>

#include <cstdint>
#include <functional>
#include <string_view>

namespace rgsml::audio {

// Candidate-only full-resource materializer; NEVER a playback publisher.
// Equal rates remain a bit-exact copy. Two historic 44.1<->48 PCM oracles
// remain unchanged. The PRODUCT MASTER->DEVICE monitoring entrypoint uses
// the exact verified 16-FIR assets and shared Playback polyphase path for
// all 30 nonidentity pairs. Legacy SOURCE-domain exploratory candidate
// tests retain their separate NONRELEASE rational prototype only in that
// explicitly distinct source entrypoint. Device Q1-Q6/physical still open.
// A zero-side-effect caller-owned retirement check, polled at output-block
// boundaries. On cancellation no partial output owner escapes the function.
using FullResourceOutputRateCancel=std::function<bool()>;
// Observer-only completed/total output frames. No change to audio numerics.
using FullResourceOutputRateProgress=std::function<void(std::int64_t,std::int64_t)>;

struct FullResourceOutputRateCandidate final {
    AudioBuffer pcm;
    core::SampleRate source_rate;
    core::SampleRate output_rate;
    core::FrameCount source_frames;
    bool used_src{false};
    std::string_view kernel_sha256;
};

// Requires entire zero-origin PCM at SOURCE_PROCESSING_RATE and finite samples.
// Equal rates: separate bit-exact OUTPUT_RATE allocation.
// 44.1<->48: complete-resource bounded FIR blocks and frozen mapping.
// Refuses unsupported pairs, partial windows, short reads, wrong domain,
// nonfinite samples and invalid block geometry before returning any PCM.
[[nodiscard]] core::Result<FullResourceOutputRateCandidate>
materialize_full_resource_output_rate_candidate(
    AudioBufferView full_source_pcm,
    core::SampleRate required_output_rate,
    core::FrameCount maximum_output_block_frames,
    const FullResourceOutputRateCancel& cancelled = {},
    const FullResourceOutputRateProgress& progress = {});

// Borrow-only monitoring entrypoint: identical offline SRC numerical path,
// but requires an immutable OUTPUT_RATE master domain instead of SOURCE.
// Does not allocate a full SOURCE scratch clone; the input is never changed.
// Separate from final-master rendering / M16/M17 certification.
[[nodiscard]] core::Result<FullResourceOutputRateCandidate>
materialize_output_master_monitoring_rate_candidate(
    AudioBufferView immutable_output_master_pcm,
    core::SampleRate required_device_rate,
    core::FrameCount maximum_output_block_frames,
    const FullResourceOutputRateCancel& cancelled = {},
    const FullResourceOutputRateProgress& progress = {});

} // namespace rgsml::audio
