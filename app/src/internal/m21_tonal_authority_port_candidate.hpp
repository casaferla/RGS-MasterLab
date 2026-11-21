#pragma once
// L1-M21 PRIVATE. Reuse M20 profile topology, never derive shapeDb from fractions.
// Actual authoritative shapeDb and pair-specific common support MUST be supplied
// by a future qualified canonical producer; this file does not establish them.
#include "m21_metrics_comparison_candidate.hpp"
#include "internal/m20_normalized_profile_candidate.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
namespace rgsml::app::m21_private {
namespace m20=rgsml::analysis::m20_private;
struct ExternalShapeBand {
  int actual_m20_band_index{};
  std::uint32_t externally_certified_band_id{};
  Cell externally_computed_shape_db{};
};
struct ExternalCanonicalShapeDisclosure {
  bool producer_shape_authority_present{false}; // private test-wire only
  std::string exact_resource_realization_id{};
  std::string common_comparison_support_id{}; // pair-scoped, not timeline sync
  std::vector<ExternalShapeBand> bands{};
};
enum class TonalPortProblem : std::uint8_t {
  none, no_external_shape_authority, invalid_m20_profile,
  wrong_resource_or_rate, missing_common_bandwidth, mismatched_band_list,
  duplicate_band_identity, false_numeric_support, invalid_shape_value
};
struct TonalPortResult {
  std::optional<Snapshot> snapshot{};
  TonalPortProblem problem{TonalPortProblem::no_external_shape_authority};
};
inline TonalPortResult attach_external_canonical_shape_to_actual_m20(
    const Snapshot& source,
    const m20::NormalizedProfileCandidate& normalized_m20,
    const ExternalCanonicalShapeDisclosure& certificate)
{
  const auto fail=[](TonalPortProblem problem) {
    return TonalPortResult{{},problem};
  };
  if(!certificate.producer_shape_authority_present ||
     certificate.common_comparison_support_id.empty() ||
     certificate.exact_resource_realization_id.empty())
    return fail(TonalPortProblem::no_external_shape_authority);
  if(source.lifecycle!=Lifecycle::current || source.resource_identity.empty() ||
     source.sample_rate_hz==0 ||
     certificate.exact_resource_realization_id!=source.resource_identity ||
     source.profile_denominator!=0 || !source.tonal_profile.empty())
    return fail(TonalPortProblem::wrong_resource_or_rate);
  if(normalized_m20.diagnostic!=m20::ProfileDiagnostic::valid ||
     (normalized_m20.denominator!=12 && normalized_m20.denominator!=3))
    return fail(TonalPortProblem::invalid_m20_profile);
  if(normalized_m20.sample_rate!=source.sample_rate_hz)
    return fail(TonalPortProblem::wrong_resource_or_rate);
  if(!normalized_m20.common_comparison_bandwidth)
    return fail(TonalPortProblem::missing_common_bandwidth);
  if(normalized_m20.bands.size()!=certificate.bands.size() ||
     normalized_m20.bands.empty())
    return fail(TonalPortProblem::mismatched_band_list);
  auto out=source; // fully transactional: never mutate caller on bad evidence
  out.profile_denominator=static_cast<std::uint8_t>(normalized_m20.denominator);
  out.common_comparison_support_id=certificate.common_comparison_support_id;
  std::set<int> grid_indices;
  std::set<std::uint32_t> certified_ids;
  out.tonal_profile.reserve(normalized_m20.bands.size());
  for(std::size_t i=0;i<normalized_m20.bands.size();++i) {
    const auto& band=normalized_m20.bands[i];
    const auto& disclosed=certificate.bands[i];
    if(disclosed.actual_m20_band_index!=band.geometry.index)
      return fail(TonalPortProblem::mismatched_band_list);
    if(!grid_indices.insert(band.geometry.index).second ||
       !certified_ids.insert(disclosed.externally_certified_band_id).second)
      return fail(TonalPortProblem::duplicate_band_identity);
    const bool in_common=band.fully_in_common_comparison &&
                          band.fully_in_measurement;
    const bool supported=band.supplied==m20::SuppliedBandEligibility::eligible &&
                          band.fraction && std::isfinite(*band.fraction) &&
                          *band.fraction>=0.0 && *band.fraction<=1.0;
    if(!in_common || !supported) {
      if(disclosed.externally_computed_shape_db.unrounded ||
         disclosed.externally_computed_shape_db.quality==Quality::present ||
         disclosed.externally_computed_shape_db.quality==Quality::low_confidence)
        return fail(TonalPortProblem::false_numeric_support);
      const bool low=band.supplied==m20::SuppliedBandEligibility::low_support;
      out.tonal_profile.push_back({disclosed.externally_certified_band_id,false,
        {{},!in_common?Quality::incompatible:Quality::missing,
         !in_common?"Outside common-band support":
         low?"LOW_SUPPORT: no valid shape value":"Missing band support"}});
      continue;
    }
    const auto& c=disclosed.externally_computed_shape_db;
    if((c.quality==Quality::present || c.quality==Quality::low_confidence) &&
       (!c.unrounded || !std::isfinite(*c.unrounded)))
      return fail(TonalPortProblem::invalid_shape_value);
    if(c.unrounded && (!std::isfinite(*c.unrounded) ||
       (c.quality!=Quality::present && c.quality!=Quality::low_confidence)))
      return fail(TonalPortProblem::invalid_shape_value);
    out.tonal_profile.push_back({disclosed.externally_certified_band_id,true,c});
  }
  return {std::move(out),TonalPortProblem::none};
}
} // namespace rgsml::app::m21_private
