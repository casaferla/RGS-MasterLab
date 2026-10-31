#include <rgsml/audio/external_playback_fir_assets.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/sha256.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::audio {
namespace {
struct FrozenPlaybackIdentity final {
    std::int64_t source;
    std::int64_t target;
    const char* filename;
    std::size_t words;
    const char* sha256;
};
constexpr std::array<FrozenPlaybackIdentity,2> kFrozen{{
    {44100,48000,"playback_src_kernel_44100_48000.bin",30721,
     "ec8c4d554eb4ede15f58eead1fede923e74c4692c57832bda716e3fc826926b4"},
    {48000,44100,"playback_src_kernel_48000_44100.bin",28225,
     "37cfda8e885e2d00646ec60d02bdf5b7dcb85348fca416e065bab71a824efb22"}
}};
struct AssetState final {
    std::mutex mutex;
    std::filesystem::path root;
    std::array<std::shared_ptr<const std::vector<std::uint64_t>>,2> cache{};
};
AssetState& state() {
    static AssetState s;
    return s;
}
[[nodiscard]] core::Result<ExternalPlaybackFir> refused(
    core::ErrorCode code,const char* why)
{
    return core::Result<ExternalPlaybackFir>::failure(
        core::Error{code,why,{{"category","PLAYBACK_FIR_EXTERNAL_ASSET_REJECTED"}}});
}
} // namespace

void configure_external_playback_fir_root(std::filesystem::path root)
{
    auto& s=state();
    std::lock_guard guard{s.mutex};
    s.cache={}; // existing PlaybackSampleRateAdapter owns its immutable copy
    s.root=std::move(root);
}

core::Result<ExternalPlaybackFir> load_external_playback_fir(
    std::int64_t source_hz,std::int64_t device_hz)
{
    using Result=core::Result<ExternalPlaybackFir>;
    std::size_t selected=kFrozen.size();
    for(std::size_t i=0;i<kFrozen.size();++i)
        if(kFrozen[i].source==source_hz&&kFrozen[i].target==device_hz) {
            selected=i;
            break;
        }
    if(selected==kFrozen.size())
        return refused(core::ErrorCode::UnsupportedOperation,
            "PLAYBACK_RATE_PAIR_UNSUPPORTED_V1");

    auto& s=state();
    // This lock is taken during control-path adapter construction only.
    // No audio callback may load, allocate, hash or access a disk path.
    std::lock_guard guard{s.mutex};
    const auto& id=kFrozen[selected];
    if(s.cache[selected])
        return Result::success(ExternalPlaybackFir{
            s.cache[selected],id.sha256});

    std::filesystem::path root=s.root;
    // Only test/authorized process configuration may supply a root;
    // no network discovery, user home search or system-directory fallback.
    if(root.empty()) {
        if(const auto* explicitRoot=std::getenv("RGSML_PLAYBACK_FIR_ROOT");
           explicitRoot!=nullptr&&*explicitRoot!='\0')
            root=std::filesystem::path{explicitRoot};
    }
    if(root.empty())
        return refused(core::ErrorCode::InvalidState,
            "PLAYBACK_FIR_RESOURCE_ROOT_NOT_CONFIGURED");
    const auto path=root/id.filename;
    try {
        if(std::filesystem::is_symlink(root)
           ||std::filesystem::is_symlink(path)
           ||!std::filesystem::is_directory(root)
           ||!std::filesystem::is_regular_file(path))
            return refused(core::ErrorCode::InvalidState,
                "PLAYBACK_FIR_RESOURCE_MISSING_OR_UNSAFE");
        if(std::filesystem::file_size(path)!=id.words*8U)
            return refused(core::ErrorCode::InvalidState,
                "PLAYBACK_FIR_RESOURCE_SIZE_MISMATCH");
        std::vector<std::uint8_t> raw(id.words*8U);
        std::ifstream input(path,std::ios::binary);
        if(!input)
            return refused(core::ErrorCode::IoFailure,
                "PLAYBACK_FIR_RESOURCE_READ_REFUSED");
        input.read(reinterpret_cast<char*>(raw.data()),
                   static_cast<std::streamsize>(raw.size()));
        if(input.gcount()!=static_cast<std::streamsize>(raw.size())
           ||input.bad())
            return refused(core::ErrorCode::IoFailure,
                "PLAYBACK_FIR_RESOURCE_TRUNCATED");
        const auto bytes=std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(raw.data()),raw.size()};
        if(core::sha256_hex(bytes)!=id.sha256)
            return refused(core::ErrorCode::InvalidState,
                "PLAYBACK_FIR_TRUSTED_SHA256_MISMATCH");
        std::vector<std::uint64_t> bits(id.words);
        for(std::size_t i=0;i<id.words;++i) {
            std::uint64_t word=0;
            for(std::size_t byte=0;byte<8;++byte)
                word|=static_cast<std::uint64_t>(raw[8*i+byte])
                    <<(8*byte);
            if(!std::isfinite(std::bit_cast<double>(word)))
                return refused(core::ErrorCode::InvalidState,
                    "PLAYBACK_FIR_NONFINITE_BINARY64");
            bits[i]=word;
        }
        // This is only an immutable-coefficient source identity check.
        // The frozen polyphase numerical implementation is not replaced.
        if(bits.empty() ||bits.size()!=id.words)
            return refused(core::ErrorCode::InvalidState,
                "PLAYBACK_FIR_INVALID_GEOMETRY");
        auto owned=std::make_shared<const std::vector<std::uint64_t>>(
            std::move(bits));
        s.cache[selected]=owned;
        return Result::success(ExternalPlaybackFir{std::move(owned),id.sha256});
    } catch(const std::exception&) {
        return refused(core::ErrorCode::IoFailure,
            "PLAYBACK_FIR_RESOURCE_LOAD_FAILED_CLOSED");
    }
}
} // namespace rgsml::audio
