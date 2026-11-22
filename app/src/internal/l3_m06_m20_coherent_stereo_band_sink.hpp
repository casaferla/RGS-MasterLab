#pragma once
// L3-M06 exact native M20 L/R + signed real cross-PSD source.
// S03/S04 are per-common-band groups. S05/S06 are cumulative
// [20,80),[20,120),[20,200) with separately attested width gate.
// One streaming pass, bounded per-band accumulator; no second FFT,
// no coherence substitution, no fabricated mono Side endpoint.
#include "l3_m06_m20_native_frame_energy_sink.hpp"
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rgsml::app::l3_automatch::m05m06_real {
inline constexpr double kStereoNativePowerFloor=1e-12;
struct StereoCoherentScalar final {
    std::optional<double> transformed_z;
    std::optional<double> raw_ratio_or_rho;
    bool actual_zero_side_endpoint{};
    std::uint64_t valid_frames{};
    double native_support{};
};
struct StereoCanonicalBand final {
    int canonical_index{};
    StereoCoherentScalar width,correlation;
};
struct StereoCoherentNativeResult final {
    std::vector<StereoCanonicalBand> multiband;
    std::array<StereoCoherentScalar,3> low_end_width;
    std::array<StereoCoherentScalar,3> low_end_correlation;
    std::uint64_t native_selected_frames{};
    bool numerical_source_valid{};
    bool low_end_gate_current_and_identical{};
    [[nodiscard]] constexpr bool authorizes_m10_or_stereo_safety()
        const noexcept{return false;}
};
class AuthenticM20CoherentStereoSink final:
    public analysis::TonalStereoStereoFrameSink {
public:
    AuthenticM20CoherentStereoSink(
        std::uint32_t sample_rate_hz,
        std::uint64_t canonical_m20_fft_size,
        std::vector<NativeFrameBand> exact_full_common_bands,
        bool exact_current_phase6_band_geometry_attested,
        bool exact_current_low_end_width_gate_equals_selected_frames)
        :fs_(sample_rate_hz),nfft_(canonical_m20_fft_size),
         low_gate_current_(exact_current_low_end_width_gate_equals_selected_frames)
    {
        constexpr std::array<std::uint32_t,6> accepted_rates{{
            44100,48000,88200,96000,176400,192000}};
        if(!exact_current_phase6_band_geometry_attested||
           std::find(accepted_rates.begin(),accepted_rates.end(),fs_)==
               accepted_rates.end()||
           nfft_<2||(nfft_&(nfft_-1U))!=0U||
           exact_full_common_bands.empty()) {
            valid_=false;return;
        }
        const double max_valid=std::min(20000.0,0.45*fs_);
        for(std::size_t i=0;i<exact_full_common_bands.size();++i) {
            const auto& band=exact_full_common_bands[i];
            if(!std::isfinite(band.low_hz)||!std::isfinite(band.high_hz)||
               band.low_hz<20.0||band.low_hz>=band.high_hz||
               band.high_hz>max_valid||
               (i>0U&&
                (band.exact_canonical_index<=
                   exact_full_common_bands[i-1U].exact_canonical_index||
                 band.low_hz<exact_full_common_bands[i-1U].high_hz))) {
                valid_=false;return;
            }
            bands_.push_back({
                band.exact_canonical_index,
                weighted_cells(band.low_hz,band.high_hz),{}});
        }
        full_band_=weighted_cells(20.0,max_valid);
        for(std::size_t i=0;i<3U;++i)
            low_bands_[i]=weighted_cells(20.0,
                static_cast<double>(kLowEndFc[i]));
    }
    [[nodiscard]] bool on_selected_stereo_frame(
        std::uint64_t ordinal,std::uint32_t sample_rate_hz,
        std::uint64_t fft_size,std::span<const double> l,
        std::span<const double> r,std::span<const double> cross) override
    {
        if(!valid_||fs_!=sample_rate_hz||fft_size!=nfft_||
           l.size()!=nfft_/2U+1U||r.size()!=l.size()||
           cross.size()!=l.size()||
           (last_ordinal_&&ordinal<=*last_ordinal_)) {
            valid_=false;return false;
        }
        for(std::size_t k=0;k<l.size();++k) {
            if(!std::isfinite(l[k])||!std::isfinite(r[k])||
               !std::isfinite(cross[k])||l[k]<0.0||r[k]<0.0||
               std::abs(cross[k])>
                 std::sqrt(l[k]*r[k])+1e-12) {
                valid_=false;return false;
            }
        }
        const auto full=integrate(full_band_,l,r,cross);
        if(!full) {valid_=false;return false;}
        for(auto& band:bands_) {
            const auto energy=integrate(band.cells,l,r,cross);
            if(!energy) {valid_=false;return false;}
            band.observed.accept(*energy,*full);
        }
        if(low_gate_current_) {
            for(std::size_t i=0;i<3U;++i) {
                const auto e=integrate(low_bands_[i],l,r,cross);
                if(!e) {valid_=false;return false;}
                low_[i].accept(*e,*full);
            }
        }
        last_ordinal_=ordinal;
        ++frames_;
        return true;
    }
    [[nodiscard]] StereoCoherentNativeResult finalize(
        std::uint64_t independently_authenticated_active_pcm_frames,
        bool current_scope_and_phase6_reference_grade_attested,
        std::span<const bool> per_band_exact_zero_side_attested={},
        const std::array<bool,3>& low_end_zero_side_attested={{false,false,false}})
        const
    {
        StereoCoherentNativeResult out;
        out.native_selected_frames=frames_;
        out.low_end_gate_current_and_identical=low_gate_current_;
        out.numerical_source_valid=valid_&&frames_>0U;
        if(!out.numerical_source_valid||fs_==0U)return out;
        if(!per_band_exact_zero_side_attested.empty()&&
           per_band_exact_zero_side_attested.size()!=bands_.size()) {
            out.numerical_source_valid=false;
            return out;
        }
        const bool active=current_scope_and_phase6_reference_grade_attested&&
            independently_authenticated_active_pcm_frames>=fs_;
        out.multiband.reserve(bands_.size());
        for(std::size_t i=0;i<bands_.size();++i) {
            const bool mono=per_band_exact_zero_side_attested.empty()?
                false:per_band_exact_zero_side_attested[i];
            out.multiband.push_back({
                bands_[i].index,
                bands_[i].observed.width(frames_,active,mono,true),
                bands_[i].observed.correlation(frames_,active,true)});
        }
        if(low_gate_current_) {
            for(std::size_t i=0;i<3U;++i) {
                out.low_end_width[i]=low_[i].width(
                    frames_,active,low_end_zero_side_attested[i],false);
                out.low_end_correlation[i]=low_[i].correlation(
                    frames_,active,false);
            }
        }
        return out;
    }
    [[nodiscard]] bool valid() const noexcept{return valid_;}
    [[nodiscard]] constexpr bool authorizes_full_stereo_gate()
        const noexcept{return false;}
private:
    struct Triple final {double l{},r{},c{};};
    struct Cell final {std::size_t bin{};double overlap_hz{};};
    struct Acc final {
        double width_mid{},width_side{},corr_l{},corr_r{},corr_c{};
        std::uint64_t width_valid{},corr_valid{};
        static constexpr double kRelativeFloor=1e-12;
        void accept(const Triple& band,const Triple& full) noexcept {
            const double EL=band.l,ER=band.r,C=band.c;
            const double T=EL+ER;
            const double Em=(T+2.0*C)/4.0;
            const double Es=(T-2.0*C)/4.0;
            const double fl=std::max(kStereoNativePowerFloor,
                                    kRelativeFloor*full.l);
            const double fr=std::max(kStereoNativePowerFloor,
                                    kRelativeFloor*full.r);
            const double fsum=std::max(kStereoNativePowerFloor,
                                      kRelativeFloor*(full.l+full.r));
            if(std::isfinite(Em)&&std::isfinite(Es)&&Em>0.0&&
               Es>=0.0&&T>=fsum) {
                width_mid+=Em;width_side+=Es;++width_valid;
            }
            if(EL>0.0&&ER>0.0&&EL>=fl&&ER>=fr&&
               std::isfinite(C)&&std::isfinite(EL)&&std::isfinite(ER)) {
                corr_l+=EL;corr_r+=ER;corr_c+=C;++corr_valid;
            }
        }
        [[nodiscard]] StereoCoherentScalar width(
            std::uint64_t total,bool external_grade,bool mono_attested,
            bool per_band) const
        {
            StereoCoherentScalar out;
            out.valid_frames=width_valid;
            if(total)out.native_support=
                static_cast<double>(width_valid)/static_cast<double>(total);
            const bool grade=external_grade&&width_valid>0U&&
                (!per_band||(width_valid>=20U&&out.native_support>=0.50));
            if(!grade||!std::isfinite(width_mid)||
               !std::isfinite(width_side)||width_mid<=0.0||
               width_side<0.0)return out;
            if(width_side==0.0) {
                if(!mono_attested)return out;
                out.raw_ratio_or_rho=0.0;
                out.transformed_z=-24.0;
                out.actual_zero_side_endpoint=true;
                return out;
            }
            const double ratio=std::sqrt(width_side/width_mid);
            if(!std::isfinite(ratio)||ratio<std::exp2(-24.0))
                return out;
            out.raw_ratio_or_rho=ratio;
            out.transformed_z=std::log2(ratio);
            return out;
        }
        [[nodiscard]] StereoCoherentScalar correlation(
            std::uint64_t total,bool external_grade,bool per_band) const
        {
            StereoCoherentScalar out;
            out.valid_frames=corr_valid;
            if(total)out.native_support=
                static_cast<double>(corr_valid)/static_cast<double>(total);
            const bool grade=external_grade&&corr_valid>0U&&
                (!per_band||(corr_valid>=20U&&out.native_support>=0.50));
            if(!grade||corr_l<=0.0||corr_r<=0.0)return out;
            const double divisor=std::sqrt(corr_l*corr_r);
            if(!std::isfinite(divisor)||divisor<=0.0)
                return out;
            // Explicit guarded positive divisor remains visible under
            // aggressive MSVC fast release analysis (C4723).
            const volatile double checked_positive_divisor=divisor;
            const double r=corr_c/checked_positive_divisor;
            if(!std::isfinite(r)||r< -1.0-1e-10||r>1.0+1e-10)
                return out;
            const double bounded=std::clamp(r,-1.0,1.0);
            out.raw_ratio_or_rho=bounded;
            out.transformed_z=std::atanh(
                std::clamp(bounded,-1.0+1e-6,1.0-1e-6));
            return out;
        }
    };
    struct BandAccumulator final {
        int index{};
        std::vector<Cell> cells;
        Acc observed;
    };
    [[nodiscard]] std::vector<Cell> weighted_cells(
        double lo,double hi) const {
        std::vector<Cell> cells;
        const double half=static_cast<double>(fs_)/2.0;
        const double step=static_cast<double>(fs_)/
                          static_cast<double>(nfft_);
        for(std::size_t k=0;k<nfft_/2U+1U;++k) {
            const double a=std::max(0.0,(static_cast<double>(k)-0.5)*step);
            const double b=std::min(half,
                (static_cast<double>(k)+0.5)*step);
            const double overlap=std::max(0.0,
                std::min(b,hi)-std::max(a,lo));
            if(overlap>0.0)cells.push_back({k,overlap});
        }
        return cells;
    }
    [[nodiscard]] std::optional<Triple> integrate(
        const std::vector<Cell>& cells,std::span<const double> l,
        std::span<const double> r,std::span<const double> c) const {
        Triple out;
        for(const auto& cell:cells) {
            out.l+=l[cell.bin]*cell.overlap_hz;
            out.r+=r[cell.bin]*cell.overlap_hz;
            out.c+=c[cell.bin]*cell.overlap_hz;
        }
        if(!std::isfinite(out.l)||!std::isfinite(out.r)||
           !std::isfinite(out.c)||out.l<0.0||out.r<0.0)
            return std::nullopt;
        return out;
    }
    static constexpr std::array<unsigned,3> kLowEndFc{{80,120,200}};
    std::uint32_t fs_{};
    std::uint64_t nfft_{};
    bool low_gate_current_{},valid_{true};
    std::uint64_t frames_{};
    std::optional<std::uint64_t> last_ordinal_;
    std::vector<BandAccumulator> bands_;
    std::vector<Cell> full_band_;
    std::array<std::vector<Cell>,3> low_bands_;
    std::array<Acc,3> low_;
};
} // namespace rgsml::app::l3_automatch::m05m06_real
