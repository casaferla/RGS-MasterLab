#pragma once
// L3-M06 D10-D12 EXACT CANONICAL PER-FRAME STFT ENERGY REDUCER.
// Consumes authenticated full active-frame E[t,b] matrix produced at
// Phase6 native RTE(fs*4096/48000), periodic Hann, hop RTE(N/4);
// does NOT synthesize E from time-AVERAGED M20 PSD or a UI spectrum.
// D10 relative spread P90-P10, D11 absolute spread P90-P10,
// D12 Pearson(A,G) then endpoint-preserving Fisher Z.
#include "l3_m06_real_short_term_shape.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
inline constexpr double kSpectralBandMinPower=1e-12;
inline constexpr std::size_t kSpectralMinimumValidFrames=20U;
inline constexpr double kSpectralMinNativeCoverage=0.50;
enum class SpectralNativeReason:std::uint8_t {
    ready,missing_exact_stft_or_grid,invalid_energy_or_geometry,
    insufficient_band_frame_support,undefined_temporal_variance,
    invalid_arithmetic
};
struct SpectralBandD10D12 final {
    std::array<std::optional<double>,3> d10_d11_d12_z{};
    std::array<std::uint64_t,3> valid_native_counts{};
    std::array<double,3> native_support{};
    std::array<SpectralNativeReason,3> reason{
        SpectralNativeReason::missing_exact_stft_or_grid,
        SpectralNativeReason::missing_exact_stft_or_grid,
        SpectralNativeReason::missing_exact_stft_or_grid};
    std::optional<double> d12_raw_rho{};
    std::string exact_frame_population_identity{};
};
struct SpectralD10D12Native final {
    std::vector<SpectralBandD10D12> canonical_band_results;
    std::uint64_t candidate_active_frames{};
    SpectralNativeReason source_reason{
        SpectralNativeReason::missing_exact_stft_or_grid};
    [[nodiscard]] constexpr bool authorizes_m10() const noexcept {
        return false;
    }
};
[[nodiscard]] inline SpectralD10D12Native
reduce_native_spectral_d10_d12(
    const std::vector<std::vector<std::optional<double>>>& active_e_tb,
    std::size_t canonical_configured_band_count,
    bool exact_current_stft_frame_energy_and_grid_attested,
    std::uint64_t active_duration_frames,
    std::uint64_t sample_rate_hz,
    std::span<const std::uint64_t> exact_selected_ordinals={})
{
    SpectralD10D12Native out;
    if(!exact_current_stft_frame_energy_and_grid_attested||
       active_e_tb.empty()||canonical_configured_band_count==0U||
       sample_rate_hz==0U)
        return out;
    const std::size_t n=active_e_tb.size();
    const bool has_ordinals=!exact_selected_ordinals.empty();
    if(has_ordinals&&(exact_selected_ordinals.size()!=n||
       std::adjacent_find(exact_selected_ordinals.begin(),
           exact_selected_ordinals.end(),[](auto a,auto b){return a>=b;})!=
           exact_selected_ordinals.end())) {
        out.source_reason=SpectralNativeReason::invalid_energy_or_geometry;
        return out;
    }
    NativeMembershipIdentity selected{"D10-D12-selected-STFT/1"};
    selected.append(sample_rate_hz);
    selected.append(active_duration_frames);
    selected.append(canonical_configured_band_count);
    for(auto ordinal:exact_selected_ordinals)selected.append(ordinal);
    const auto selected_identity=has_ordinals?selected.finish():std::string{};
    std::vector<NativeMembershipIdentity> accepted;
    if(has_ordinals) {
        accepted.reserve(canonical_configured_band_count);
        for(std::size_t k=0;k<canonical_configured_band_count;++k) {
            accepted.emplace_back("D10-D12-valid-band-STFT/1");
            accepted.back().append(k);
        }
    }
    out.candidate_active_frames=n;
    out.canonical_band_results.resize(canonical_configured_band_count);
    if(active_duration_frames<2U*sample_rate_hz) {
        for(auto& band:out.canonical_band_results)
            band.reason.fill(
                SpectralNativeReason::insufficient_band_frame_support);
        out.source_reason=
            SpectralNativeReason::insufficient_band_frame_support;
        return out;
    }
    std::vector<std::vector<double>> abs_values(canonical_configured_band_count);
    std::vector<std::vector<double>> rel_values(canonical_configured_band_count);
    std::vector<std::vector<double>> common_values(canonical_configured_band_count);
    std::vector<std::vector<double>> chronological_A(canonical_configured_band_count);
    for(std::size_t frame_index=0;frame_index<n;++frame_index) {
        const auto& frame=active_e_tb[frame_index];
        if(frame.size()!=canonical_configured_band_count) {
            out.source_reason=SpectralNativeReason::invalid_energy_or_geometry;
            return out;
        }
        double B=0.0;
        bool complete=true;
        for(const auto& energy:frame) {
            if(!energy){complete=false;continue;}
            if(!std::isfinite(*energy)||*energy<0.0) {
                out.source_reason=SpectralNativeReason::invalid_energy_or_geometry;
                return out;
            }
            B+=*energy;
        }
        // A missing exact common grid member invalidates broadband B
        // for this frame; never shrink the comparison bandwidth.
        if(!complete)continue;
        if(!std::isfinite(B)) {
            out.source_reason=SpectralNativeReason::invalid_energy_or_geometry;
            return out;
        }
        if(B<kSpectralBandMinPower)continue;
        const double G=10.0*std::log10(B);
        const double floor=std::max(kSpectralBandMinPower,
                                     kSpectralBandMinPower*B);
        if(!std::isfinite(G)||!std::isfinite(floor)) {
            out.source_reason=SpectralNativeReason::invalid_arithmetic;
            return out;
        }
        for(std::size_t k=0;k<canonical_configured_band_count;++k) {
            const double E=*frame[k];
            if(E<floor)continue;
            const double A=10.0*std::log10(E);
            const double R=A-G;
            if(!std::isfinite(A)||!std::isfinite(R)) {
                out.source_reason=SpectralNativeReason::invalid_arithmetic;
                return out;
            }
            if(has_ordinals)accepted[k].append(exact_selected_ordinals[frame_index]);
            abs_values[k].push_back(A);
            rel_values[k].push_back(R);
            common_values[k].push_back(G);
            chronological_A[k].push_back(A);
        }
    }
    constexpr double kMinVariance=1e-12;
    for(std::size_t k=0;k<canonical_configured_band_count;++k) {
        auto& band=out.canonical_band_results[k];
        const auto count=abs_values[k].size();
        const double native_support=
            static_cast<double>(count)/static_cast<double>(n);
        if(!std::isfinite(native_support)||native_support<0.0||
           native_support>1.0) {
            out.source_reason=SpectralNativeReason::invalid_arithmetic;
            return out;
        }
        if(has_ordinals)band.exact_frame_population_identity=
            selected_identity+":"+accepted[k].finish();
        band.valid_native_counts.fill(count);
        band.native_support.fill(native_support);
        if(count<kSpectralMinimumValidFrames||
           native_support<kSpectralMinNativeCoverage) {
            band.reason.fill(
                SpectralNativeReason::insufficient_band_frame_support);
            continue;
        }
        auto& A=abs_values[k];
        auto& R=rel_values[k];
        const auto& G=common_values[k];
        std::sort(A.begin(),A.end());
        std::sort(R.begin(),R.end());
        const auto a10=type7_percentile(A,0.10);
        const auto a90=type7_percentile(A,0.90);
        const auto r10=type7_percentile(R,0.10);
        const auto r90=type7_percentile(R,0.90);
        if(a10&&a90&&r10&&r90) {
            band.d10_d11_d12_z[0]=*r90-*r10;
            band.d10_d11_d12_z[1]=*a90-*a10;
            band.reason[0]=SpectralNativeReason::ready;
            band.reason[1]=SpectralNativeReason::ready;
        }else{
            band.reason[0]=SpectralNativeReason::invalid_arithmetic;
            band.reason[1]=SpectralNativeReason::invalid_arithmetic;
        }
        // Chronological pairs, never the independently sorted P10/P90
        // sequences: compute population moments in centered coordinates.
        const auto& timeA=chronological_A[k];
        const double positive_n=static_cast<double>(count);
        const double meanA=std::accumulate(
            timeA.begin(),timeA.end(),0.0)/positive_n;
        const double meanG=std::accumulate(
            G.begin(),G.end(),0.0)/positive_n;
        double varA=0.0,varG=0.0,cov=0.0;
        for(std::size_t t=0;t<count;++t) {
            const double a=timeA[t]-meanA;
            const double g=G[t]-meanG;
            varA+=a*a;
            varG+=g*g;
            cov+=a*g;
        }
        varA/=positive_n;varG/=positive_n;cov/=positive_n;
        if(!std::isfinite(varA)||!std::isfinite(varG)||
           !std::isfinite(cov)) {
            band.reason[2]=SpectralNativeReason::invalid_arithmetic;
            continue;
        }
        if(varA<kMinVariance||varG<kMinVariance) {
            band.reason[2]=
                SpectralNativeReason::undefined_temporal_variance;
            continue;
        }
        const double rho=cov/std::sqrt(varA*varG);
        if(!std::isfinite(rho)||rho< -1.0-1e-12||rho>1.0+1e-12) {
            band.reason[2]=SpectralNativeReason::invalid_arithmetic;
            continue;
        }
        const double bounded=std::clamp(rho,-1.0,1.0);
        const double endpoint=std::clamp(bounded,
                                        -1.0+1e-6,1.0-1e-6);
        const double z=std::atanh(endpoint);
        if(!std::isfinite(z)) {
            band.reason[2]=SpectralNativeReason::invalid_arithmetic;
            continue;
        }
        band.d12_raw_rho=bounded;
        band.d10_d11_d12_z[2]=z;
        band.reason[2]=SpectralNativeReason::ready;
    }
    out.source_reason=SpectralNativeReason::ready;
    return out;
}
} // namespace rgsml::app::l3_automatch::m05m06_real
