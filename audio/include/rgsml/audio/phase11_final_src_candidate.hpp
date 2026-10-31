#pragma once

// Offline Phase-11 FINAL SRC candidate integration seam (NOT RELEASED).
// Never called by product M17 until genuine 14-kernel MPFR/Ziv/DTFT Q1-Q6
// approval and immutable release SHA list exist. This API performs exact
// format/hash checks, full-resource strict scalar convolution and cancellation
// but cannot independently confer a release certificate or audible authority.
#include <rgsml/audio/full_resource_output_rate_candidate.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace rgsml::audio {
[[nodiscard]] core::Result<FullResourceOutputRateCandidate>
materialize_phase11_pinned_final_src_candidate(
    AudioBufferView immutable_full_source,
    core::SampleRate target_master_rate,
    core::FrameCount maximum_block_frames,
    std::span<const std::uint8_t> candidate_rgsmsr1_bytes,
    std::string_view externally_locked_sha256,
    const FullResourceOutputRateCancel& cancelled = {});

// Distinct, explicit OFFLINE test of the SOURCE-AUTHENTIC Q0 PIN (not
// self-declared hash). This mathematical-subgate wrapper remains NONRELEASE:
// production admission, manifest release and full M17 FQ are independent.
[[nodiscard]] core::Result<FullResourceOutputRateCandidate>
materialize_phase11_scoped_q0_pinned_offline_candidate(
    AudioBufferView full_source_pcm,core::SampleRate output_rate,
    core::FrameCount maximum_output_block_frames,
    std::span<const std::uint8_t> original_private_fir_asset,
    const FullResourceOutputRateCancel& cancelled = {});

} // namespace rgsml::audio
