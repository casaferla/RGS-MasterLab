#pragma once
// L1-M18 isolated read bridge: exact immutable M17 OUTPUT_RATE binary64.
// This is NOT a WAV export authority or evidence of current UI selection.
#include "m18_offline_export.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <utility>

namespace rgsml::audio::internal {

class M18OwnedMasterFrameProvider final : public M18MasterFrameProvider {
public:
    // Caller MUST already prove the exact M17/M16 verified owner is current.
    // Holding memory does not confer later export authority or user consent.
    [[nodiscard]] static std::shared_ptr<M18OwnedMasterFrameProvider> capture(
        std::shared_ptr<const audio::AudioBuffer> immutableMaster,
        core::SampleRate selectedMasterRate)
    {
        if (!immutableMaster || !supported_rate(selectedMasterRate.value()))
            return {};
        const auto view=immutableMaster->view();
        const auto frameCount=view.frame_count().value();
        const auto count=view.format().channel_count();
        if (view.timebase().frame_domain_id()!=audio::FrameDomainId::OUTPUT_RATE
            ||view.timebase().sample_rate()!=selectedMasterRate
            ||view.format().sample_rate()!=selectedMasterRate
            ||view.absolute_start_frame()!=core::FrameIndex{0}
            ||frameCount<=0 ||(count!=1U && count!=2U))
            return {};
        const auto frames=static_cast<std::uint64_t>(frameCount);
        if (frames>std::numeric_limits<std::size_t>::max())
            return {};
        std::array<std::span<const double>,2> planes{};
        for (std::size_t channel=0;channel<count;++channel) {
            const auto p=view.channel(channel);
            if (!p ||p.value()==nullptr
                ||p.value()->size()!=static_cast<std::size_t>(frames)
                ||p.value()->data()==nullptr)
                return {};
            planes[channel]=*p.value();
        }
        return std::shared_ptr<M18OwnedMasterFrameProvider>(
            new M18OwnedMasterFrameProvider(
                std::move(immutableMaster),selectedMasterRate,frames,
                static_cast<std::uint32_t>(count),planes));
    }

    [[nodiscard]] bool read_exact(
        std::uint64_t absoluteMasterFrame,std::uint32_t frames,
        std::uint32_t channels,std::span<double> destination) noexcept override
    {
        if (frames==0U ||channels!=channels_
            ||static_cast<std::size_t>(frames)>
                  std::numeric_limits<std::size_t>::max()/channels
            ||destination.size()!=static_cast<std::size_t>(frames)*channels
            ||absoluteMasterFrame>=totalFrames_
            ||static_cast<std::uint64_t>(frames)>
                  totalFrames_-absoluteMasterFrame)
            return false;
        const auto first=static_cast<std::size_t>(absoluteMasterFrame);
        if (channels==1U) {
            // Byte copy preserves signed zero and subnormal bits.
            std::memcpy(destination.data(),planes_[0].data()+first,
                        static_cast<std::size_t>(frames)*sizeof(double));
        } else {
            for (std::size_t i=0;i<frames;++i) {
                std::memcpy(destination.data()+i*2U,
                            planes_[0].data()+first+i,sizeof(double));
                std::memcpy(destination.data()+i*2U+1U,
                            planes_[1].data()+first+i,sizeof(double));
            }
        }
        return true;
    }

    [[nodiscard]] std::uint64_t frame_count() const noexcept
    { return totalFrames_; }
    [[nodiscard]] std::uint32_t channel_count() const noexcept
    { return channels_; }
    [[nodiscard]] core::SampleRate master_rate() const noexcept
    { return selectedRate_; }

private:
    [[nodiscard]] static constexpr bool supported_rate(
        std::int64_t hz) noexcept
    {
        return hz==44100 ||hz==48000 ||hz==88200
            ||hz==96000 ||hz==176400 ||hz==192000;
    }

    M18OwnedMasterFrameProvider(
        std::shared_ptr<const audio::AudioBuffer> owner,
        core::SampleRate rate,std::uint64_t frames,std::uint32_t channels,
        std::array<std::span<const double>,2> planes) noexcept
        : owner_(std::move(owner)),selectedRate_(rate),
          totalFrames_(frames),channels_(channels),planes_(planes)
    {}

    // Alias-sharing the M17 stage pins exactly the same physical PCM.
    std::shared_ptr<const audio::AudioBuffer> owner_;
    core::SampleRate selectedRate_;
    std::uint64_t totalFrames_{};
    std::uint32_t channels_{};
    std::array<std::span<const double>,2> planes_{};
};

} // namespace rgsml::audio::internal
