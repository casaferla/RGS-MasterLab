// M21 tonal preproduction: genuine M20 FFT/Hann/PSD and 1/12 or 1/3 profile
// topology; unrounded shapeDb provided EXTERNALLY by private fixtures, not M21.
#include "internal/m21_tonal_authority_port_candidate.hpp"
#include "internal/m20_scope_spectrum_candidate.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <vector>
using namespace rgsml::app::m21_private;
static m20::NormalizedProfileCandidate actual_m20(
    std::uint32_t fs,int denom,bool low_support)
{
  const auto g=m20::spectral_geometry(fs);assert(g);
  const auto count=static_cast<std::size_t>(g->window+2*g->hop);
  std::vector<double> pcm(count);
  for(std::size_t i=0;i<count;++i)
    pcm[i]=0.2*std::sin(2*std::numbers::pi*1377.0*i/fs);
  const auto sp=m20::scope_spectrum_candidate(
    m20::ScopeSpectrumInput{fs,22017,pcm,{},false,{}});
  assert(sp.diagnostic==m20::Diagnostic::valid);
  const auto geometry=m20::fractional_band_geometry(fs,denom);
  std::vector<std::uint8_t> eligible(geometry.size(),std::uint8_t{1});
  if(low_support)for(std::size_t i=0;i<eligible.size();++i)
    if(i%13==5)eligible[i]=2;
  const auto common=m20::common_bandwidth(fs,44100);
  assert(common);
  auto result=m20::make_profile_candidate(sp.average_psd,fs,g->fft,
    denom,eligible,common);
  assert(result.diagnostic==m20::ProfileDiagnostic::valid);
  return result;
}
static Snapshot make_source(std::uint32_t fs,Column column) {
  Snapshot s;s.column=column;s.lifecycle=Lifecycle::current;
  s.sample_rate_hz=fs;s.generation_token=1;s.scope_frames=fs;
  s.resource_identity=column==Column::gold?"EXTERNAL-GOLD":"PREPARED";
  s.analysis_scope_label=column==Column::gold?"Region":"Full Track";
  return s;
}
static ExternalCanonicalShapeDisclosure externally_supplied(
  const m20::NormalizedProfileCandidate& profile,
  const Snapshot& source,double offset)
{
  ExternalCanonicalShapeDisclosure d;
  d.producer_shape_authority_present=true;
  d.exact_resource_realization_id=source.resource_identity;
  d.common_comparison_support_id="FIXTURE-PAIR-IDENTITY";
  for(const auto& band:profile.bands) {
    const bool available=band.fully_in_common_comparison &&
      band.fully_in_measurement &&
      band.supplied==m20::SuppliedBandEligibility::eligible &&
      band.fraction && std::isfinite(*band.fraction);
    const auto id=static_cast<std::uint32_t>(band.geometry.index+128);
    d.bands.push_back({band.geometry.index,id,
      available?Cell{-16.0+offset+band.geometry.index*0.02,Quality::present,{}}:
                Cell{{},Quality::missing,"Unsupported fixture band"}});
  }
  return d;
}
int main() {
  for(std::uint32_t fs:{44100U,48000U,88200U,96000U,176400U,192000U}) {
    const auto prepProfile=actual_m20(fs,12,true);
    const auto goldProfile=actual_m20(44100,12,true);
    auto prepared=make_source(fs,Column::prepared);
    auto gold=make_source(44100,Column::gold);
    const auto prepCertificate=externally_supplied(prepProfile,prepared,0.0);
    const auto goldCertificate=externally_supplied(goldProfile,gold,2.0);
    const auto pp=attach_external_canonical_shape_to_actual_m20(
        prepared,prepProfile,prepCertificate);
    const auto gg=attach_external_canonical_shape_to_actual_m20(
        gold,goldProfile,goldCertificate);
    assert(pp.problem==TonalPortProblem::none && pp.snapshot);
    assert(gg.problem==TonalPortProblem::none && gg.snapshot);
    assert(pp.snapshot->profile_denominator==12);
    assert(gg.snapshot->profile_denominator==12);
    std::size_t valid=0,missing=0;
    for(const auto& band:gg.snapshot->tonal_profile) {
      const auto d=tonal_delta(&*pp.snapshot,&*gg.snapshot,band.band_id);
      if(d.quality==Quality::present) {
        assert(d.unrounded && std::abs(*d.unrounded-2.0)<1e-12);
        ++valid;
      } else {
        assert(!d.unrounded);
        ++missing;
      }
    }
    assert(valid>5 && missing>0);
    // Gold vs Prepared may use different sample rates; common support does
    // NOT imply sample/time alignment between independent Gold scopes.
    assert(gg.snapshot->analysis_scope_label=="Region");
    auto wrong=prepCertificate;
    wrong.common_comparison_support_id="UNMATCHED-COMPARISON-PAIR";
    const auto mismatch=attach_external_canonical_shape_to_actual_m20(
      prepared,prepProfile,wrong);
    assert(mismatch.snapshot);
    assert(tonal_delta(&*mismatch.snapshot,&*gg.snapshot,
       gg.snapshot->tonal_profile[gg.snapshot->tonal_profile.size()/2].band_id)
       .quality==Quality::incompatible);
    wrong=prepCertificate;wrong.producer_shape_authority_present=false;
    assert(attach_external_canonical_shape_to_actual_m20(
      prepared,prepProfile,wrong).problem==TonalPortProblem::no_external_shape_authority);
    wrong=prepCertificate;wrong.exact_resource_realization_id="OTHER";
    assert(attach_external_canonical_shape_to_actual_m20(
      prepared,prepProfile,wrong).problem==TonalPortProblem::wrong_resource_or_rate);
    wrong=prepCertificate;wrong.bands[0].actual_m20_band_index=12345;
    assert(attach_external_canonical_shape_to_actual_m20(
      prepared,prepProfile,wrong).problem==TonalPortProblem::mismatched_band_list);
    wrong=prepCertificate;
    wrong.bands[1].externally_certified_band_id=
      wrong.bands[0].externally_certified_band_id;
    assert(attach_external_canonical_shape_to_actual_m20(
      prepared,prepProfile,wrong).problem==TonalPortProblem::duplicate_band_identity);
    wrong=prepCertificate;
    for(std::size_t i=0;i<prepProfile.bands.size();++i)
      if(!prepProfile.bands[i].fully_in_common_comparison ||
         prepProfile.bands[i].supplied==m20::SuppliedBandEligibility::low_support) {
        wrong.bands[i].externally_computed_shape_db={0.0,Quality::present,{}};
        break;
      }
    assert(attach_external_canonical_shape_to_actual_m20(
      prepared,prepProfile,wrong).problem==TonalPortProblem::false_numeric_support);
    wrong=prepCertificate;
    for(std::size_t i=0;i<prepProfile.bands.size();++i)
      if(prepProfile.bands[i].fraction &&
         prepProfile.bands[i].fully_in_common_comparison) {
        wrong.bands[i].externally_computed_shape_db.unrounded=
          std::numeric_limits<double>::quiet_NaN();
        break;
      }
    assert(attach_external_canonical_shape_to_actual_m20(
      prepared,prepProfile,wrong).problem==TonalPortProblem::invalid_shape_value);
    auto noCommon=prepProfile;noCommon.common_comparison_bandwidth.reset();
    assert(attach_external_canonical_shape_to_actual_m20(
      prepared,noCommon,prepCertificate).problem==TonalPortProblem::missing_common_bandwidth);
    const auto fallback=actual_m20(fs,3,true);
    const auto three=attach_external_canonical_shape_to_actual_m20(
      prepared,fallback,externally_supplied(fallback,prepared,0.0));
    assert(three.snapshot && three.snapshot->profile_denominator==3);
    assert(tonal_delta(&*three.snapshot,&*gg.snapshot,
      gg.snapshot->tonal_profile[gg.snapshot->tonal_profile.size()/2].band_id)
      .quality==Quality::incompatible);
  }
  std::cout<<"M21_SIX_RATE_REAL_M20_PROFILE_EXTERNAL_SHAPE_PASS_NOT_S\n";
}
