#pragma once

// Phase-11 frozen Q0 RGSMSR1 geometry transcribed from the independently
// rational/pi-interval audited phase11_math_preflight.py. This table is a
// TEST/qualification reference, never coefficient/response/release approval.
// Each of 30 nonidentity pairs reduces to one of these 14 L/M shapes.
#include <array>
#include <cstdint>

namespace rgsml::audio::internal {
struct Phase11FrozenShape final {
    std::uint32_t L;
    std::uint32_t M;
    std::uint32_t T;
    std::uint32_t N;
};
inline constexpr std::array<Phase11FrozenShape,14> kPhase11FrozenShapes{{
    {1,2,990,991},       {1,4,1980,1981},
    {2,1,512,1025},      {4,1,512,2049},
    {40,147,1818,72721},{80,147,910,72801},
    {147,40,512,75265}, {147,80,512,75265},
    {147,160,540,79381},{147,320,1078,158467},
    {147,640,2154,316639},{160,147,512,81921},
    {320,147,512,163841},{640,147,512,327681},
}};
[[nodiscard]] constexpr const Phase11FrozenShape*
phase11_frozen_shape(std::uint32_t L,std::uint32_t M) noexcept
{
    for(const auto& shape:kPhase11FrozenShapes)
        if(shape.L==L&&shape.M==M)
            return &shape;
    return nullptr;
}
} // namespace rgsml::audio::internal
