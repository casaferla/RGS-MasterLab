// Private offline FIR asset IO implementation; no realtime callback IO.
// Public audio API remains filename-system-neutral / portable UTF-16.
#include <rgsml/audio/external_playback_fir_assets.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/sha256.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_geometry.hpp>
#include <rgsml/audio/internal/phase11_rgsmsr1_candidate_asset.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numeric>
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
    std::array<std::shared_ptr<const std::vector<std::uint64_t>>,14> originalQ0Cache{};
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

void configure_external_playback_fir_root(std::u16string root)
{
    auto& s=state();
    std::lock_guard guard{s.mutex};
    s.cache={}; // existing PlaybackSampleRateAdapter owns its immutable copy
    s.originalQ0Cache={}; // changing roots cannot retain a formerly admitted FIR
    // Convert the portable UTF-16 API value to the native filesystem path
    // ONLY inside this .cpp (never in public DSP-facing headers).
    s.root=std::filesystem::path{std::move(root)};
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
    // All 30 directed nonidentity six-rate paths share one explicit
    // immutable 16-file catalog. The two historic Playback kernels retain
    // their bit-exact identity; other DEVICE paths use the *original* Q0
    // RGSMSR1 asset for the same reduced ratio, never generated coefficients.
    const bool historic=selected<kFrozen.size();
    std::size_t originalIndex=internal::kPhase11FrozenOriginalFirPins.size();
    const internal::Phase11FrozenOriginalFirPin* original=nullptr;
    const internal::Phase11FrozenShape* shape=nullptr;
    if(!historic && source_hz>0 && device_hz>0) {
        const auto g=std::gcd(source_hz,device_hz);
        const auto L=static_cast<std::uint32_t>(device_hz/g);
        const auto M=static_cast<std::uint32_t>(source_hz/g);
        original=internal::phase11_frozen_original_fir_pin(L,M);
        shape=internal::phase11_frozen_shape(L,M);
        for(std::size_t i=0;i<internal::kPhase11FrozenOriginalFirPins.size();++i)
            if(internal::kPhase11FrozenOriginalFirPins[i].L==L
               &&internal::kPhase11FrozenOriginalFirPins[i].M==M) {
                originalIndex=i;
                break;
            }
    }
    if(!historic &&(!original||!shape
       ||originalIndex>=internal::kPhase11FrozenOriginalFirPins.size()))
        return refused(core::ErrorCode::UnsupportedOperation,
            "PLAYBACK_ORIGINAL_Q0_RATE_PAIR_NOT_FROZEN");

    auto& s=state();
    // This lock is taken during control-path adapter construction only.
    // No audio callback may load, allocate, hash or access a disk path.
    std::lock_guard guard{s.mutex};
    const auto filename=historic
        ?std::string{kFrozen[selected].filename}
        :std::string{"RGSMSR1_L"}+std::to_string(original->L)
            +"_M"+std::to_string(original->M)+".bin";
    const auto expectedWords=historic?kFrozen[selected].words
                                  :static_cast<std::size_t>(shape->N);
    const std::string_view pinnedSha=historic?kFrozen[selected].sha256
                                            :original->sha256;
    auto& cached=historic?s.cache[selected]:s.originalQ0Cache[originalIndex];
    if(cached)
        return Result::success(ExternalPlaybackFir{cached,pinnedSha});

    std::filesystem::path root=s.root;
    // Only test/authorized process configuration may supply a root;
    // no network discovery, user home search or system-directory fallback.
    if(root.empty()) {
#if defined(_WIN32)
        // MSVC secure CRT: read the explicit test-only fallback without
        // disabling C4996 / warnings-as-errors for the audio library.
        char* explicitRoot=nullptr;
        std::size_t chars=0;
        const auto err=_dupenv_s(
            &explicitRoot,&chars,"RGSML_PLAYBACK_FIR_ROOT");
        if(err==0 && explicitRoot!=nullptr && chars>1)
            root=std::filesystem::path{explicitRoot};
        std::free(explicitRoot);
#else
        if(const auto* explicitRoot=std::getenv("RGSML_PLAYBACK_FIR_ROOT");
           explicitRoot!=nullptr&&*explicitRoot!='\0')
            root=std::filesystem::path{explicitRoot};
#endif
    }
    if(root.empty())
        return refused(core::ErrorCode::InvalidState,
            "PLAYBACK_FIR_RESOURCE_ROOT_NOT_CONFIGURED");
    const auto path=root/filename;
    try {
        if(std::filesystem::is_symlink(root)
           ||std::filesystem::is_symlink(path)
           ||!std::filesystem::is_directory(root)
           ||!std::filesystem::is_regular_file(path))
            return refused(core::ErrorCode::InvalidState,
                "PLAYBACK_FIR_RESOURCE_MISSING_OR_UNSAFE");
        if(std::filesystem::file_size(path)!=(historic?0U:52U)+expectedWords*8U)
            return refused(core::ErrorCode::InvalidState,
                "PLAYBACK_FIR_RESOURCE_SIZE_MISMATCH");
        std::vector<std::uint8_t> raw((historic?0U:52U)+expectedWords*8U);
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
        if(core::sha256_hex(bytes)!=pinnedSha)
            return refused(core::ErrorCode::InvalidState,
                "PLAYBACK_FIR_TRUSTED_SHA256_MISMATCH");
        std::vector<std::uint64_t> bits;
        if(historic) {
            bits.resize(expectedWords);
            for(std::size_t i=0;i<expectedWords;++i) {
                std::uint64_t word=0;
                for(std::size_t byte=0;byte<8;++byte)
                    word|=static_cast<std::uint64_t>(raw[8*i+byte])
                        <<(8*byte);
                if(!std::isfinite(std::bit_cast<double>(word)))
                    return refused(core::ErrorCode::InvalidState,
                        "PLAYBACK_FIR_NONFINITE_BINARY64");
                bits[i]=word;
            }
        } else {
            // Separate frozen SHA was checked above. Validate the whole
            // exact 52-byte RGSMSR1 header, count and symmetric binary64
            // coefficients before making an immutable DEVICE owner.
            auto verified=internal::parse_phase11_rgsmsr1_pinned_candidate_bits(
                raw,original->L,original->M,shape->T,pinnedSha);
            if(!verified)
                return Result::failure(*verified.error());
            bits=std::move(*verified.value());
        }
        if(bits.size()!=expectedWords)
            return refused(core::ErrorCode::InvalidState,
                "PLAYBACK_ORIGINAL_FIR_INVALID_GEOMETRY");
        auto owned=std::make_shared<const std::vector<std::uint64_t>>(
            std::move(bits));
        cached=owned;
        return Result::success(ExternalPlaybackFir{std::move(owned),pinnedSha});
    } catch(const std::exception&) {
        return refused(core::ErrorCode::IoFailure,
            "PLAYBACK_FIR_RESOURCE_LOAD_FAILED_CLOSED");
    }
}
} // namespace rgsml::audio
