#ifndef RGSML_M20_PRIVATE_NORMALIZED_PROFILE_CANDIDATE_HPP
#define RGSML_M20_PRIVATE_NORMALIZED_PROFILE_CANDIDATE_HPP
// M20 PRIVATE preproduction candidate. No new FeatureDefinitionRegistry, statuses or UI maths.
// Canonical per-band LOW_SUPPORT thresholds and result identities are NOT reproduced here.
#include "m20_tonal_candidate.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace rgsml::analysis::m20_private {

// INPUT LABELS only, from a future authenticated canonical feature support source.
// Values are private test-wire tokens, NOT canonical FeatureStatus numeric IDs.
enum class SuppliedBandEligibility : std::uint8_t {
  missing = 0,
  eligible = 1,
  low_support = 2
};
enum class ProfileDiagnostic {
  valid,
  invalid_input,
  no_eligible_power
};
struct ProfileBandCandidate {
  Band geometry;
  SuppliedBandEligibility supplied = SuppliedBandEligibility::missing;
  bool fully_in_measurement = false;
  bool fully_in_common_comparison = false;
  std::optional<double> measured_energy;
  std::optional<double> fraction;
};
struct NormalizedProfileCandidate {
  ProfileDiagnostic diagnostic = ProfileDiagnostic::invalid_input;
  std::uint32_t sample_rate = 0;
  int denominator = 0;  // 12 canonical primary, 3 explicitly selected substitute
  Bandwidth measurement_bandwidth{};
  std::optional<Bandwidth> common_comparison_bandwidth;
  std::optional<double> eligible_power_sum;
  std::vector<ProfileBandCandidate> bands;
};

[[nodiscard]] inline NormalizedProfileCandidate make_profile_candidate(
    std::span<const double> canonical_mean_psd,
    std::uint32_t fs,
    std::uint64_t fft,
    int denominator,
    std::span<const std::uint8_t> supplied_eligibility,
    std::optional<Bandwidth> explicit_common_bandwidth = std::nullopt,
    bool use_all_common_band_power = false)
{
  NormalizedProfileCandidate out;
  out.sample_rate = fs;
  out.denominator = denominator;
  const auto sg = spectral_geometry(fs);
  if (!sg || sg->fft != fft || (denominator != 12 && denominator != 3))
    return out;
  if (canonical_mean_psd.size() != static_cast<std::size_t>(fft/2 + 1))
    return out;
  for (double psd : canonical_mean_psd)
    if (!(psd >= 0.0) || !std::isfinite(psd)) return out;

  out.measurement_bandwidth = available_bandwidth(fs);
  if (explicit_common_bandwidth) {
    const auto b = *explicit_common_bandwidth;
    if (!std::isfinite(b.low) || !std::isfinite(b.high) ||
        b.low < out.measurement_bandwidth.low ||
        b.high > out.measurement_bandwidth.high || !(b.high > b.low))
      return out;
    out.common_comparison_bandwidth = b;
  }
  const auto grid = fractional_band_geometry(fs, denominator);
  if (supplied_eligibility.size() != grid.size())
    return out;

  if(use_all_common_band_power&&!explicit_common_bandwidth)return out;
  double sum = 0.0;
  double common_sum=0.0;
  out.bands.reserve(grid.size());
  for (std::size_t i=0; i<grid.size(); ++i) {
    const auto token = supplied_eligibility[i];
    if (token > static_cast<std::uint8_t>(SuppliedBandEligibility::low_support))
      return out;
    ProfileBandCandidate band;
    band.geometry = grid[i];
    band.supplied = static_cast<SuppliedBandEligibility>(token);
    band.fully_in_measurement = grid[i].fully_supported;
    band.fully_in_common_comparison =
      band.fully_in_measurement &&
      (!explicit_common_bandwidth ||
       (grid[i].low >= explicit_common_bandwidth->low &&
        grid[i].high <= explicit_common_bandwidth->high));
    const bool eligible=band.supplied == SuppliedBandEligibility::eligible &&
        (!explicit_common_bandwidth || band.fully_in_common_comparison);
    if (eligible || (use_all_common_band_power&&band.fully_in_common_comparison)) {
      const double low = explicit_common_bandwidth ?
        std::max(grid[i].low,explicit_common_bandwidth->low) :
        grid[i].covered_low;
      const double high = explicit_common_bandwidth ?
        std::min(grid[i].high,explicit_common_bandwidth->high) :
        grid[i].covered_high;
      if (high > low) {
        const auto e = band_energy(canonical_mean_psd,fs,fft,low,high);
        if (!e) return out;
        if(use_all_common_band_power) {
          common_sum+=*e;
          if(!std::isfinite(common_sum))return out;
        }
        if(eligible) {
          band.measured_energy = *e;
          sum += *e;
          if (!std::isfinite(sum)) return out;
        }
      }
    }
    out.bands.push_back(band);
  }
  if (sum <= 0.0) {
    out.diagnostic = ProfileDiagnostic::no_eligible_power;
    return out; // Distinguish zero genuine measured power from missing frequency.
  }
  out.eligible_power_sum = sum;
  for (auto& band : out.bands) {
    if (band.measured_energy) {
      const double divisor=use_all_common_band_power?common_sum:sum;
      if(!(divisor>0.0))return out;
      const double fraction=*band.measured_energy/divisor;
      // Phase6 comparison floor: below -120 dB of the full common power
      // is missing, not a clamped number or a renormalized population.
      if(use_all_common_band_power&&fraction<1e-12)continue;
      band.fraction = fraction;
      if (!std::isfinite(*band.fraction)) {
        out.diagnostic = ProfileDiagnostic::invalid_input;
        out.eligible_power_sum.reset();
        for (auto& clear : out.bands) clear.fraction.reset();
        return out;
      }
    }
  }
  out.diagnostic = ProfileDiagnostic::valid;
  return out;
}

} // namespace rgsml::analysis::m20_private
#endif // RGSML_M20_PRIVATE_NORMALIZED_PROFILE_CANDIDATE_HPP
