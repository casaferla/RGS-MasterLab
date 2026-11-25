#pragma once
#include "l3_m09_native_automatic_seed_bank.hpp"
#include "l3_m10_original_definition_authority.hpp"

namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeFixedSeedGeometry final {
    std::vector<m05m06_real::T01ConfiguredBand> tonal;
    std::vector<m05m06_real::T01ConfiguredBand> side;
    int side_denominator{};
    double common_low{},common_high{};
};
[[nodiscard]] inline std::optional<std::vector<m05m06_real::T01ConfiguredBand>>
match_exact_native_stereo_grid(std::span<const m05m06_real::NativeFrameBand> bands,
    std::span<const int> indices,std::uint32_t rate,int denominator)
{
    if(bands.empty()||bands.size()!=indices.size())return {};
    const auto geometry=analysis::native_tonal_fractional_grid(rate,denominator);
    std::vector<m05m06_real::T01ConfiguredBand> out;
    for(std::size_t i=0;i<bands.size();++i) {
        if(bands[i].exact_canonical_index!=indices[i]||
           (i&&indices[i]<=indices[i-1U]))return {};
        const auto it=std::find_if(geometry.begin(),geometry.end(),[&](const auto& band){
            return band.band_index==indices[i]&&band.low_hz==bands[i].low_hz&&
                band.high_hz==bands[i].high_hz;
        });
        if(it==geometry.end())return {};
        out.push_back({it->band_index,it->low_hz,it->center_hz,it->high_hz});
    }
    return out;
}
// Both grid identity and selected resolution must be evidenced by the
// same immutable Fixed source policy, not inferred from the count.
[[nodiscard]] inline std::optional<NativeFixedSeedGeometry>
derive_native_fixed_seed_geometry(const m11_frozen::NativeFixedPopulation& fixed,
    const OriginalPreparedReferenceNativeFacts& originals,
    const ProductNativeOnePassSpec& spec,std::uint32_t rate)
{
    if(!fixed.original_definition_authority||
       fixed.exact_definition_authority_content.empty()||
       m10_frozen::exact_original_native_definition_content(*fixed.original_definition_authority)!=
           fixed.exact_definition_authority_content||
       originals.prepared.original_m19_m20.sample_rate_hz!=static_cast<std::int64_t>(rate))
        return {};
    const auto& definitions=*fixed.original_definition_authority;
    const auto common=originals.prepared.exact_profile_common_bandwidth;
    if(common.low_hz!=originals.reference.exact_profile_common_bandwidth.low_hz||
       common.high_hz!=originals.reference.exact_profile_common_bandwidth.high_hz||
       common.low_hz!=definitions.t02_t06.exact_common.low_hz||
       common.high_hz!=definitions.t02_t06.exact_common.high_hz)return {};
    NativeFixedSeedGeometry out;
    out.common_low=common.low_hz;out.common_high=common.high_hz;
    out.tonal=definitions.t01.authoritative_primary_grid;
    if(originals.prepared.original_m19_m20.channels==2U) {
        const auto primary=match_exact_native_stereo_grid(spec.stereo_bands,
            definitions.s03_s06.exact_canonical_band_indices,rate,12);
        const auto coarse=match_exact_native_stereo_grid(spec.stereo_bands,
            definitions.s03_s06.exact_canonical_band_indices,rate,3);
        if(primary.has_value()==coarse.has_value())return {};
        out.side=primary?*primary:*coarse;
        out.side_denominator=primary?12:3;
    }
    return out;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
