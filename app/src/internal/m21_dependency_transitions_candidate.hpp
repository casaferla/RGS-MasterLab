#pragma once
// M21 isolated PRIVATE dependency-event simulation. Not a canonical Phase-6
// scheduler, not a production signal or real AnalysisScope identity owner.
// No analyzer DSP or mutations to the mastering chain occur here.
#include "m21_metrics_comparison_candidate.hpp"
#include <array>
#include <cstdint>
#include <limits>
namespace rgsml::app::m21_private {
enum class PrivateChange : std::uint8_t {
  preparation_realization_replaced,
  source_realization_replaced,
  mastering_chain_changed,
  active_gold_replaced,
  gold_analysis_scope_replaced,
  audition_loop_region_only,
  audition_level_match_only,
  display_only_selection
};
class PrivateDependencyTransitions {
public:
  bool begin(Column column){
    const auto i=static_cast<std::size_t>(column);
    if(i>=3 || generations_[i]==(std::numeric_limits<std::uint64_t>::max)())
      return false;
    const auto next=generations_[i]+1;
    if(!guard_.expect(column,next))return false;
    generations_[i]=next;
    return true;
  }
  // An event invalidates only its truly data-dependent columns.
  // SOURCE/PREPARED changes also obsolete the processed render, but MUST
  // NEVER stale the independently acquired external GOLD selection.
  bool apply(PrivateChange change) {
    std::array<bool,3> dependent{};
    switch(change) {
      case PrivateChange::preparation_realization_replaced:
      case PrivateChange::source_realization_replaced:
        dependent={true,false,true};break;
      case PrivateChange::mastering_chain_changed:
        dependent={false,false,true};break;
      case PrivateChange::active_gold_replaced:
      case PrivateChange::gold_analysis_scope_replaced:
        dependent={false,true,false};break;
      case PrivateChange::audition_loop_region_only:
      case PrivateChange::audition_level_match_only:
      case PrivateChange::display_only_selection:
        dependent={false,false,false};break;
    }
    for(std::size_t i=0;i<3;++i)
      if(dependent[i] &&
         generations_[i]==(std::numeric_limits<std::uint64_t>::max)())
        return false; // No partial event processing on epoch overflow.
    for(std::size_t i=0;i<3;++i)if(dependent[i])
      if(!begin(static_cast<Column>(i)))return false;
    return true;
  }
  bool publish(const Snapshot& from_producer){
    return guard_.publish(from_producer); // no retagging or fake production
  }
  std::uint64_t expected(Column column) const {
    const auto i=static_cast<std::size_t>(column);
    return i<3?generations_[i]:0;
  }
  Comparison view() const {return guard_.view();}
private:
  std::array<std::uint64_t,3> generations_{};
  PrivatePublicationGuard guard_{};
};
}
