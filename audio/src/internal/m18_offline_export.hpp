#pragma once
// M18 isolated preproduction output seam. The caller supplies immutable
// final-MASTER binary64 frames; NO SOURCE, M17 worker, DAC, UI or project API.
// The existing Windows TransactionalAudioExporter remains authoritative.
// This prototype only proves frame/seed ownership through WAV publication.
#include "m18_transactional_file.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <system_error>

namespace rgsml::audio::internal {

class M18MasterFrameProvider {
public:
    virtual ~M18MasterFrameProvider() = default;
    // Implementer must fill ALL requested channel-interleaved binary64 samples.
    // This method does not mutate or advance the source/limiter/DEVICE owner.
    [[nodiscard]] virtual bool read_exact(std::uint64_t absoluteMasterFrame,
        std::uint32_t frames, std::uint32_t channels,
        std::span<double> destination) noexcept = 0;
};

class M18ExportStopSignal {
public:
    virtual ~M18ExportStopSignal() = default;
    [[nodiscard]] virtual bool cancelled() const noexcept = 0;
};

enum class M18OfflineExportStatus : std::uint8_t {
    OK, INVALID_PLAN, ADDRESS_OVERFLOW, TARGET_EXISTS, STAGING_FAILED,
    CANCELLED, SOURCE_FAILED, STREAM_FAILED, NO_RECEIPT,
    PUBLICATION_FAILED, PUBLISHED_CLEANUP_WARNING
};

struct M18OfflineExportResult {
    M18OfflineExportStatus status{M18OfflineExportStatus::INVALID_PLAN};
    M18StreamStatus stream_status{M18StreamStatus::BAD_SEQUENCE};
    M18CodecStatus codec_status{M18CodecStatus::OK};
    M18FileStatus file_status{M18FileStatus::STAGE_FAILED};
    std::optional<M18StreamReceipt> receipt{};
};

// The only frame origin accepted is the absolute OUTPUT (MASTER) frame.
// One allocation-free 4096-frame buffer, flat mono/stereo, explicit seed.
// The receipt is emitted ONLY after a successful no-overwrite publication.
// On any failure the RAII transaction owns and deletes its private staging.
[[nodiscard]] inline M18OfflineExportResult m18_export_offline_candidate(
    M18MasterFrameProvider& frames,
    const std::filesystem::path& destination,
    M18WavSpec spec,
    std::uint64_t firstAbsoluteMasterFrame,
    const M18ExportStopSignal* stop=nullptr) noexcept
{
    const auto plan=m18_plan_wav(spec);
    if(plan.status!=M18WavStatus::OK)
        return {M18OfflineExportStatus::INVALID_PLAN};
    if(spec.frame_count && spec.frame_count-1U >
       UINT64_MAX-firstAbsoluteMasterFrame)
        return {M18OfflineExportStatus::ADDRESS_OVERFLOW};
    std::error_code ec;
    if(std::filesystem::exists(destination,ec))
        return {M18OfflineExportStatus::TARGET_EXISTS};
    if(ec)return {M18OfflineExportStatus::STAGING_FAILED};
    auto stage=M18TransactionalFile::open(destination);
    if(!stage)return {M18OfflineExportStatus::STAGING_FAILED};
    M18StreamingWavWriter writer(*stage,spec,firstAbsoluteMasterFrame);
    auto st=writer.start();
    if(st!=M18StreamStatus::OK)
        return {M18OfflineExportStatus::STREAM_FAILED,st};
    std::array<double,M18StreamingWavWriter::kChunkFrames*2U> buffer{};
    std::uint64_t offset=0U;
    while(offset<spec.frame_count){
        if(stop && stop->cancelled())
            return {M18OfflineExportStatus::CANCELLED};
        const auto count=static_cast<std::uint32_t>(std::min(
            spec.frame_count-offset,
            static_cast<std::uint64_t>(M18StreamingWavWriter::kChunkFrames)));
        const auto block=std::span<double>{buffer}.first(
            static_cast<std::size_t>(count)*spec.channels);
        if(!frames.read_exact(firstAbsoluteMasterFrame+offset,
                              count,spec.channels,block))
            return {M18OfflineExportStatus::SOURCE_FAILED};
        st=writer.append(block);
        if(st!=M18StreamStatus::OK)
            return {M18OfflineExportStatus::STREAM_FAILED,st,
                    writer.codec_failure()};
        offset+=count;
    }
    if(stop && stop->cancelled())
        return {M18OfflineExportStatus::CANCELLED};
    auto finished=writer.finish();
    if(finished.status!=M18StreamStatus::OK || !finished.receipt)
        return {M18OfflineExportStatus::NO_RECEIPT,finished.status,
                writer.codec_failure()};
    const auto published=stage->publish(*finished.receipt);
    if(published!=M18FileStatus::OK){
        if(published==M18FileStatus::PUBLISHED_CLEANUP_WARNING)
            return {M18OfflineExportStatus::PUBLISHED_CLEANUP_WARNING,
                    M18StreamStatus::OK,M18CodecStatus::OK,
                    published,finished.receipt};
        return {M18OfflineExportStatus::PUBLICATION_FAILED,
                M18StreamStatus::OK,M18CodecStatus::OK,published};
    }
    return {M18OfflineExportStatus::OK,M18StreamStatus::OK,
            M18CodecStatus::OK,M18FileStatus::OK,finished.receipt};
}

} // namespace rgsml::audio::internal
