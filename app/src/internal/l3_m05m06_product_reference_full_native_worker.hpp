#pragma once
// L3 M05/M06 independently-owned REFERENCE: capture the genuine Qt
// GoldSelection owner on the Qt thread, then decode the EXACT entire
// WAV and compute the ORIGINAL full native M19/M20/P04 feature
// families on a worker. No M21 nine-metric proxy or re-rendering a
// SOURCE variant as REFERENCE. Completion remains provisional until
// the caller rechecks GoldCapture on the Qt owner thread.
#include "l3_m05m06_owned_full_source_native_facts.hpp"
#include "l3_m05m06_product_native_identity.hpp"
#include "m21_product_owner_analysis.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/result.hpp>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace rgsml::app::l3_automatch {
struct ProductReferenceNativeFullResult final {
    std::shared_ptr<const audio::AudioBuffer> exact_independent_pcm;
    OriginalOwnedNativeSourceFacts original_full_m05_m06;
    [[nodiscard]] constexpr bool authorizes_product_m10_or_m12()
        const noexcept{return false;}
};
[[nodiscard]] inline core::Result<ProductReferenceNativeFullResult>
measure_current_gold_full_native_worker(
    const internal::M21ProductOwnerAnalysis::GoldCapture& captured,
    const OfflineProductOwnerStamp& exact_product_owner,
    ProductNativeOnePassSpec native_spec,
    m05m06_real::CommonTonalBandwidth exact_phase6_common_bandwidth,
    double exact_phase6_tonal_density_floor,
    bool exact_phase6_lra_file_based_trailing_attested,
    const std::atomic<bool>* cancellation=nullptr)
{
    using R=core::Result<ProductReferenceNativeFullResult>;
    const auto fail=[](const char* msg){
        return R::failure(core::Error{core::ErrorCode::InvalidState,msg});
    };
    if(!captured.gold.permissions().can_read()||
       !captured.owner.epoch||!captured.owner.identity.valid()||
       !captured.selection_revision||
       !exact_product_owner.populated()||
       captured.owner.epoch!=exact_product_owner.m18_owner_epoch||
       captured.owner.identity.source_generation!=
           exact_product_owner.source_generation||
       captured.selection_revision!=
           exact_product_owner.reference_selection_revision||
       captured.gold.provider_id()!=
           exact_product_owner.reference_provider_id||
       captured.gold.locator()!=exact_product_owner.reference_locator||
       captured.full_frames<=0||
       (captured.channels!=1U&&captured.channels!=2U)||
       (captured.sample_rate_hz!=44100U&&captured.sample_rate_hz!=48000U&&
        captured.sample_rate_hz!=88200U&&captured.sample_rate_hz!=96000U&&
        captured.sample_rate_hz!=176400U&&captured.sample_rate_hz!=192000U)||
       (cancellation&&cancellation->load(std::memory_order_acquire)))
        return fail("L3_REFERENCE_NATIVE_MISSING_QT_CAPTURE_OR_OWNER");
    if(!bind_product_native_fft_metadata(native_spec,captured.sample_rate_hz))
        return fail("L3_REFERENCE_NATIVE_FFT_METADATA_INVALID");
    const auto bandwidth=analysis::native_tonal_measurement_bandwidth(captured.sample_rate_hz);
    if(!bandwidth||!m05m06_real::native_common_contains(
           {bandwidth->low_hz,bandwidth->high_hz},exact_phase6_common_bandwidth))
        return fail("L3_REFERENCE_NATIVE_BANDWIDTH_INVALID");
    // Fail closed on a geometrically impossible allocation. No
    // hardcoded 128-MiB cap: five minutes at 192k stereo float64
    // legitimately requires ~879 MiB for one exact full PCM.
    if(static_cast<std::uint64_t>(captured.full_frames)>
        static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max())/
            (captured.channels*sizeof(double)))
        return fail("L3_REFERENCE_NATIVE_GEOMETRY_OVERFLOW");
    auto endpoint=platform::windows::WindowsResourceReader::
        open_read_only(captured.gold);
    if(!endpoint)return R::failure(*endpoint.error());
    std::unique_ptr<core::IResourceReader> reader=
        std::move(*endpoint.value());
    auto opened=audio::WavReader::open(std::move(reader));
    if(!opened)return R::failure(*opened.error());
    auto& wav=**opened.value();
    const auto& format=wav.info().audio_format();
    if(wav.info().frame_count().value()!=captured.full_frames||
       format.sample_rate().value()!=
           static_cast<std::int64_t>(captured.sample_rate_hz)||
       format.channel_count()!=captured.channels)
        return fail("L3_REFERENCE_NATIVE_WAV_CHANGED_SINCE_CAPTURE");
    auto full=audio::AudioBuffer::create(
        format,audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},wav.info().frame_count());
    if(!full)return R::failure(*full.error());
    const auto read=wav.read_frames(
        core::FrameIndex{0},full.value()->mutable_view());
    if(!read)return R::failure(*read.error());
    if(*read.value()!=wav.info().frame_count())
        return fail("L3_REFERENCE_NATIVE_WAV_SHORT_READ");
    const auto closed=wav.close();
    if(!closed)return R::failure(*closed.error());
    if(cancellation&&cancellation->load(std::memory_order_acquire))
        return fail("L3_REFERENCE_NATIVE_CANCELLED_AFTER_READ");
    auto original=observe_original_owned_full_source_once(
        *full.value(),native_product_original_identity(
            exact_product_owner,OriginalProductRole::reference),
        std::move(native_spec),exact_phase6_common_bandwidth,
        exact_phase6_tonal_density_floor,
        exact_phase6_lra_file_based_trailing_attested,cancellation);
    if(!original)return R::failure(*original.error());
    try {
        auto owned=std::make_shared<const audio::AudioBuffer>(
            std::move(*full.value()));
        return R::success(ProductReferenceNativeFullResult{
            std::move(owned),std::move(*original.value())});
    }catch(const std::bad_alloc&) {
        return fail("L3_REFERENCE_NATIVE_OUT_OF_MEMORY");
    }
}
} // namespace rgsml::app::l3_automatch
