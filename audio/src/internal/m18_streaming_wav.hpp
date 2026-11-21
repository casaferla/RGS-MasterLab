#pragma once
// M18 parallel preproduction: bounded-RAM streaming PCM24/F32 RIFF/RF64.
// Uses the exact private M18 sample codec; DOES NOT alter M17 or the
// production F64 WavWriter. A caller owns the byte sink and publication.
// No atomic-publication or durability claim attaches to this encoder.
#include "m18_delivery_identity.hpp"
#include <rgsml/core/sha256.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace rgsml::audio::internal {

class M18ByteSink {
public:
    virtual ~M18ByteSink() = default;
    // Return true ONLY if all input bytes have been accepted in order.
    // On false, the caller MUST discard its privately staged candidate.
    [[nodiscard]] virtual bool write_all(
        std::span<const std::uint8_t> bytes) noexcept = 0;
};

enum class M18StreamStatus : std::uint8_t {
    OK, INVALID_PLAN, BAD_SEQUENCE, BAD_LAYOUT, ADDRESS_OVERFLOW,
    CODEC_REJECTED, SINK_FAILED, INCOMPLETE
};

struct M18StreamReceipt {
    M18WavPlan plan;
    M18DeliveryIdentity identity;
    std::uint64_t frames{};
    std::string payload_sha256;
    std::string full_file_sha256;
};

struct M18StreamFinish {
    M18StreamStatus status{M18StreamStatus::BAD_SEQUENCE};
    std::optional<M18StreamReceipt> receipt{};
};

class M18StreamingWavWriter final {
public:
    static constexpr std::size_t kChunkFrames = 4096U;

    explicit M18StreamingWavWriter(
        M18ByteSink& sink, M18WavSpec spec,
        std::uint64_t firstAbsoluteFrame=0U) noexcept
        : sink_(sink), origin_(firstAbsoluteFrame)
    {
        const auto p=m18_plan_wav(spec);
        const auto identity=m18_make_delivery_identity(
            spec,firstAbsoluteFrame);
        if(p.status!=M18WavStatus::OK || !identity){
            status_=M18StreamStatus::INVALID_PLAN;
            return;
        }
        plan_=p.plan;
        identity_=*identity;
        status_=M18StreamStatus::OK;
    }

    [[nodiscard]] M18StreamStatus start() noexcept
    {
        if(status_!=M18StreamStatus::OK)return status_;
        if(started_||finished_)return reject(M18StreamStatus::BAD_SEQUENCE);
        try {
            const auto header=m18_wav_header(*plan_);
            if(header.size()!=plan_->header_bytes)
                return reject(M18StreamStatus::INVALID_PLAN);
            if(!sink_.write_all(header))
                return reject(M18StreamStatus::SINK_FAILED);
            file_sha_.update(std::as_bytes(std::span{header}));
            started_=true;
            return M18StreamStatus::OK;
        } catch(...) {
            return reject(M18StreamStatus::SINK_FAILED);
        }
    }

    // Whole passed frame span is immutable and must be frame-major stereo LR.
    // Internally only 4096 frames are buffered and fully checked per chunk;
    // any later error makes this PRIVATE candidate unpublishable. It must
    // be discarded by the caller. This is NOT an all-or-nothing socket sink.
    [[nodiscard]] M18StreamStatus append(
        std::span<const double> interleaved) noexcept
    {
        if(status_!=M18StreamStatus::OK)return status_;
        if(!started_||finished_)
            return reject(M18StreamStatus::BAD_SEQUENCE);
        const auto channels=plan_->spec.channels;
        if(interleaved.size()%channels!=0U)
            return reject(M18StreamStatus::BAD_LAYOUT);
        const auto added=interleaved.size()/channels;
        if(added>plan_->spec.frame_count-written_frames_)
            return reject(M18StreamStatus::BAD_LAYOUT);
        constexpr std::size_t kMaxBytes=kChunkFrames*2U*4U;
        std::array<std::uint8_t,kMaxBytes> buffer{};
        std::size_t offset=0U;
        while(offset<added){
            const auto frames=std::min(kChunkFrames,added-offset);
            const auto samples=frames*channels;
            auto src=interleaved.subspan(offset*channels,samples);
            auto bytes=std::span{buffer.data(),frames*plan_->block_align};
            const auto absoluteFrame=origin_+written_frames_;
            const auto result=plan_->spec.encoding==M18WavEncoding::PCM24
                ?m18_pcm24_block(src,channels,absoluteFrame,
                    plan_->spec.dither_seed,bytes)
                :m18_float32_block(src,channels,absoluteFrame,bytes);
            if(result!=M18CodecStatus::OK){
                codec_failure_=result;
                return reject(M18StreamStatus::CODEC_REJECTED);
            }
            if(!sink_.write_all(bytes))
                return reject(M18StreamStatus::SINK_FAILED);
            const auto byteView=std::as_bytes(bytes);
            file_sha_.update(byteView);
            payload_sha_.update(byteView);
            offset+=frames;
            written_frames_+=static_cast<std::uint64_t>(frames);
        }
        return M18StreamStatus::OK;
    }

    // The sink must not publish before this exact frame-count proof.
    [[nodiscard]] M18StreamFinish finish() noexcept
    {
        if(status_!=M18StreamStatus::OK)return {status_,{}};
        if(!started_||finished_)
            return {reject(M18StreamStatus::BAD_SEQUENCE),{}};
        if(written_frames_!=plan_->spec.frame_count)
            return {reject(M18StreamStatus::INCOMPLETE),{}};
        try {
            // RIFF/RF64 requires a single ZERO pad byte after an odd-sized
            // data payload. The data chunk length and PCM payload SHA
            // exclude the pad; only full-file size/SHA include it.
            if((plan_->data_bytes&UINT64_C(1))!=0U){
                const std::array<std::uint8_t,1> pad{{0U}};
                if(!sink_.write_all(pad))
                    return {reject(M18StreamStatus::SINK_FAILED),{}};
                file_sha_.update(std::as_bytes(std::span{pad}));
            }
            finished_=true;
            return {M18StreamStatus::OK,M18StreamReceipt{
                *plan_,*identity_,written_frames_,
                core::sha256_hex(payload_sha_.finalize()),
                core::sha256_hex(file_sha_.finalize())}};
        } catch(...) {
            return {reject(M18StreamStatus::SINK_FAILED),{}};
        }
    }

    [[nodiscard]] M18StreamStatus status() const noexcept
    {
        return status_;
    }
    [[nodiscard]] M18CodecStatus codec_failure() const noexcept
    {
        return codec_failure_;
    }
    [[nodiscard]] std::uint64_t frames_written() const noexcept
    {
        return written_frames_;
    }
private:
    [[nodiscard]] M18StreamStatus reject(M18StreamStatus s) noexcept
    {
        status_=s;
        return s;
    }
    M18ByteSink& sink_;
    std::uint64_t origin_;
    std::optional<M18WavPlan> plan_;
    std::optional<M18DeliveryIdentity> identity_;
    core::Sha256 file_sha_;
    core::Sha256 payload_sha_;
    std::uint64_t written_frames_{};
    bool started_{false};
    bool finished_{false};
    M18StreamStatus status_{M18StreamStatus::INVALID_PLAN};
    M18CodecStatus codec_failure_{M18CodecStatus::OK};
};

} // namespace rgsml::audio::internal
