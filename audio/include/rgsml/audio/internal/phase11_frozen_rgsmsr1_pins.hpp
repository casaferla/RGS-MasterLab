#pragma once
// 2026-10-31 original 14 RGSMSR1 FIR identities from authentic private
// Q0 SHA256 94d3340667ab5c86b2db5f2d0e191f58b5d05863413312fb3545faed51798ce4.
// Scope: frozen source-byte/mathematical Q0, NOT product M17 admission,
// Q1–Q6, FQ, WAV export, or signed release. NO binary tap material included.
#include <array>
#include <cstdint>
namespace rgsml::audio::internal {
struct Phase11FrozenOriginalFirPin final {
    std::uint32_t L;
    std::uint32_t M;
    const char* sha256;
};
inline constexpr std::array<Phase11FrozenOriginalFirPin,14>
kPhase11FrozenOriginalFirPins{{

        {1,2,"b97776527687180bea6dbf86a8da877c0bdaa528d305abe4b7beaf806ecaf48e"},
        {1,4,"3c5db068904a059fae54ab3a96ae96ec3963fc31e649f5f4de9ecf5b6d17a947"},
        {2,1,"6fb38a7d62859283e4061a18d23f354a8308d5b22ff55623e8b1c9a245e8b00c"},
        {4,1,"f2563518411bc6525889eaf82c43b194f88ceadd59b0750071c613d1071de095"},
        {40,147,"7658f824ca67beab44a3dee35ccbdaae3aa0acdfc9257c327a9eaa7f3851928f"},
        {80,147,"42d55a83855e1f166819ce4c3ad6bd53293ddb76a51afd5b1733c520be7ec5f0"},
        {147,40,"fd57c100ca171a73a3bae4e61c4cc8faca64deff722b363375e4a6fae51f65ef"},
        {147,80,"38f23112566f59137383d9ba58a0e47a80c583b1232bbbf88315b2505ac02488"},
        {147,160,"697face6bce0cf299e80c0c28cae51bb49e13dbe435fb529fbf98cfb76fee991"},
        {147,320,"36e226baf02c67e6e9357c0c1d31c205b2f00ef89526512347487c27479d8936"},
        {147,640,"42f674e946c7aa816da52ef434f162caed917199ad752514ab771d3094860923"},
        {160,147,"6997f49435c08f031fd3df68405fbd4836f1918b3a6756aa6defb970241380cf"},
        {320,147,"4da7e0cbafc81837c159dab8139e605350f9c156d92b5f11bd02262b51872004"},
        {640,147,"5f5a3b1c9020f3404cb5b1f292396122c3d65368c61988809713ac94acfb08e1"},
    }};
[[nodiscard]] constexpr const Phase11FrozenOriginalFirPin*
phase11_frozen_original_fir_pin(std::uint32_t L,std::uint32_t M) noexcept
{
    for(const auto& row:kPhase11FrozenOriginalFirPins)
        if(row.L==L &&row.M==M)return &row;
    return nullptr;
}
} // namespace rgsml::audio::internal
