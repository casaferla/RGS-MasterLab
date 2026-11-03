// Private application-side original Q0 FIR disk loader. Not a public audio
// DSP header: filesystem and file IO are restricted to this offline boundary.
#pragma once
// Private, on-demand loading of ORIGINAL 14 FIR bytes from an explicitly
// provisioned OFFLINE directory. This is neither a public asset installer
// nor a runtime/admission token. No search paths, network, user supplied
// SHA, generated fallback kernel or implicit release flag are permitted.
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_geometry.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>
#include <rgsml/audio/internal/phase11_rgsmsr1_candidate_asset.hpp>
#include <rgsml/core/result.hpp>
#include <rgsml/core/error.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::audio::internal {
[[nodiscard]] inline core::Result<std::vector<std::uint8_t>>
load_phase11_original_pinned_private_fir(
    const std::filesystem::path& explicitRoot,
    std::uint32_t L,std::uint32_t M)
{
    using Result=core::Result<std::vector<std::uint8_t>>;
    const auto reject=[](const char* why)->Result {
        return Result::failure(core::Error{
            core::ErrorCode::InvalidArgument,why,
            {{"category","PHASE11_PRIVATE_ORIGINAL_FIR_NOT_ADMITTED"}}});
    };
    const auto* pin=phase11_frozen_original_fir_pin(L,M);
    const auto* shape=phase11_frozen_shape(L,M);
    if(!pin||!shape||explicitRoot.empty())
        return reject("Original 14-FIR Q0 pin, shape or explicit root missing");
    const auto name=std::string{"RGSMSR1_L"}+std::to_string(L)
        +"_M"+std::to_string(M)+".bin";
    const auto path=explicitRoot/name;
    try {
        // Do not follow symlinks, alternate search roots or directories.
        if(std::filesystem::is_symlink(explicitRoot)
           ||std::filesystem::is_symlink(path)
           ||!std::filesystem::is_directory(explicitRoot)
           ||!std::filesystem::is_regular_file(path))
            return reject("Private RGSMSR1 file is missing or unsafe");
        const auto expected=std::uintmax_t{52}
            +std::uintmax_t{8}*static_cast<std::uintmax_t>(shape->N);
        if(std::filesystem::file_size(path)!=expected)
            return reject("Original private FIR byte length differs from Q0");
        std::ifstream input(path,std::ios::binary);
        if(!input)return reject("Original private FIR cannot be opened");
        std::vector<std::uint8_t> data{
            std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
        if(data.size()!=expected||input.bad())
            return reject("Original private FIR read incomplete");
        // This parser verifies the independently pinned TRUE source SHA
        // and binary64 finite + bit-symmetry + canonical 52-byte geometry.
        auto checked=parse_phase11_rgsmsr1_pinned_candidate_bits(
            data,L,M,shape->T,pin->sha256);
        if(!checked)return Result::failure(*checked.error());
        return Result::success(std::move(data));
    } catch(const std::filesystem::filesystem_error&) {
        return reject("Private original FIR path cannot be verified");
    } catch(const std::exception&) {
        return reject("Original private FIR load failed closed");
    }
}

[[nodiscard]] inline bool all_14_phase11_original_pins_present(
    const std::filesystem::path& explicitRoot)
{
    for(const auto& pin:kPhase11FrozenOriginalFirPins)
        if(!load_phase11_original_pinned_private_fir(
            explicitRoot,pin.L,pin.M))
            return false;
    return true; // INTEGRITY ONLY, not permission to publish M17 SRC
}
} // namespace rgsml::audio::internal
