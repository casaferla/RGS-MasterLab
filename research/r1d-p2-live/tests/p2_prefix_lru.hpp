#pragma once
// RGS R1d-P2 research-only bounded exact-prefix cache PROTOTYPE.
// No Product registration, no persistence, no shared mutable audio, no
// full-chain metadata/telemetry substitution, no streaming callback.
// This is only a pre-L2 feasibility probe with immutable owned stage PCM.
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/audio/audio_buffer_view.hpp>
#include <rgsml/core/sha256.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::research::r1d_p2::perf {

struct PrefixKey final {
    rgsml::core::Sha256Digest sourcePcmSha256{};
    std::uint64_t sourceRevision{0};
    std::int64_t originFrame{0};
    std::int64_t frameCount{0};
    int sampleRateHz{0};
    rgsml::audio::ChannelLayout channelLayout{
        rgsml::audio::ChannelLayout::STEREO_LR};
    rgsml::audio::FrameDomainId domain{
        rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE};
    std::uint64_t prefixRevision{0};
    std::uint32_t exactTailContractVersion{0};
    bool programmeAtEos{true};
    std::array<std::string,4> orderedTypes{};
    std::array<std::string,4> algorithms{};
    std::array<std::string,4> parameterSchemas{};
    std::vector<rgsml::dsp::ModuleExecutionBinding> bindings{};

    friend bool operator==(const PrefixKey&,const PrefixKey&)=default;
};

class BoundedPrefixLru final {
public:
    struct Entry final {
        PrefixKey key;
        std::vector<double> planarPcm;
        std::size_t bytes{0};
    };

    explicit BoundedPrefixLru(std::size_t maxBytes)
        :maxBytes_(maxBytes) {}

    [[nodiscard]] const Entry* lookup(const PrefixKey& key) {
        const auto it=std::find_if(entries_.begin(),entries_.end(),
            [&](const Entry& value){return value.key==key;});
        if(it==entries_.end()) {
            ++misses_;
            return nullptr;
        }
        entries_.splice(entries_.begin(),entries_,it);
        ++hits_;
        return &entries_.front();
    }

    [[nodiscard]] bool store(PrefixKey key,std::vector<double> pcm) {
        // Count all owned stage PCM bytes; no unbounded growth/eviction
        // of buffers still in use. Caller must not retain raw entry pointers
        // over further store()/clear() operations.
        if(pcm.size()>maxBytes_/sizeof(double)) {
            ++refusals_;
            return false;
        }
        for(auto it=entries_.begin();it!=entries_.end();++it) {
            if(it->key==key) {
                bytesUsed_-=it->bytes;
                entries_.erase(it);
                break;
            }
        }
        const auto bytes=pcm.size()*sizeof(double);
        while(bytesUsed_>maxBytes_-bytes && !entries_.empty()) {
            bytesUsed_-=entries_.back().bytes;
            entries_.pop_back();
            ++evictions_;
        }
        bytesUsed_+=bytes;
        entries_.push_front(Entry{std::move(key),std::move(pcm),bytes});
        return true;
    }

    void clear() noexcept {entries_.clear();bytesUsed_=0;}
    [[nodiscard]] std::size_t hits() const noexcept {return hits_;}
    [[nodiscard]] std::size_t misses() const noexcept {return misses_;}
    [[nodiscard]] std::size_t evictions() const noexcept {return evictions_;}
    [[nodiscard]] std::size_t refusals() const noexcept {return refusals_;}
    [[nodiscard]] std::size_t bytesUsed() const noexcept {return bytesUsed_;}
    [[nodiscard]] std::size_t entries() const noexcept {return entries_.size();}
    [[nodiscard]] std::size_t capacity() const noexcept {return maxBytes_;}
private:
    std::size_t maxBytes_{0};
    std::size_t bytesUsed_{0};
    std::size_t hits_{0};
    std::size_t misses_{0};
    std::size_t evictions_{0};
    std::size_t refusals_{0};
    std::list<Entry> entries_{};
};

} // namespace rgsml::research::r1d_p2::perf
