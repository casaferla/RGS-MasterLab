#pragma once

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/playback_sample_rate_adapter.hpp>
#include <rgsml/core/result.hpp>

#include <functional>
#include <string_view>

namespace rgsml::audio {

// Candidate-only full-resource materializer; NEVER a playback publisher.
// This is a dedicated offline PCM client of the existing frozen 192-tap
// kernel. The numerical playback implementation is NOT modified.
// Equal rates and the frozen 44.1<->48 kernels preserve existing oracles.
// Other six-rate pairs use a separate EXPERIMENTAL rational FIR candidate:
// fidelity, performance, cancellation and actual audio are not qualified.
// A zero-side-effect caller-owned retirement check, polled at output-block
// boundaries. On cancellation no partial output owner escapes the function.
using FullResourceOutputRateCancel=std::function<bool()>;

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
    const FullResourceOutputRateCancel& cancelled = {});

// Borrow-only monitoring entrypoint: identical offline SRC numerical path,
// but requires an immutable OUTPUT_RATE master domain instead of SOURCE.
// Does not allocate a full SOURCE scratch clone; the input is never changed.
// Separate from final-master rendering / M16/M17 certification.
[[nodiscard]] core::Result<FullResourceOutputRateCandidate>
materialize_output_master_monitoring_rate_candidate(
    AudioBufferView immutable_output_master_pcm,
    core::SampleRate required_device_rate,
    core::FrameCount maximum_output_block_frames,
    const FullResourceOutputRateCancel& cancelled = {});

} // namespace rgsml::audio
