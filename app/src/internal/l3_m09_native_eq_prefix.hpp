#pragma once
#include "l3_m09_native_eq_atom.hpp"
#include "l3_m09_frozen_gaussian_grid.hpp"
#include <set>
namespace rgsml::app::l3_automatch::m09_frozen {
struct NativeEqPrefix final {
    dsp::ParametricEqParameters payload;
    std::vector<NativeEqAtomFit> greedy_order;
    std::vector<double> exact_active_frequencies_hz;
};
// The dictionary must contain the exact legal canonical atoms and their
// allocated band IDs. The full active population stays in the fit.
[[nodiscard]] inline std::vector<NativeEqPrefix> fit_native_eq_prefixes(
    std::span<const NativeEqFitCell> active,
    std::span<const NativeEqAtom> dictionary,core::SampleRate rate,
    bool stereo_side,double deadband)
{
    std::vector<NativeEqPrefix> prefixes;
    if(active.empty()||dictionary.empty()||!std::isfinite(deadband)||deadband<0.0)return prefixes;
    std::set<core::Uuid> ids;
    for(const auto& a:dictionary)
        if(a.band_id.is_nil()||!ids.insert(a.band_id).second)return {};
    std::vector<NativeEqFitCell> residual(active.begin(),active.end());
    std::vector<NativeEqAtomFit> selected;
    std::set<core::Uuid> used;
    const std::size_t cap=stereo_side?3U:6U;
    for(std::size_t step=0;step<cap;++step) {
        if(std::all_of(residual.begin(),residual.end(),[&](const auto& c){
            return std::abs(c.residual_db)<=deadband;}))break;
        std::optional<NativeEqAtomFit> best;
        for(const auto& atom:dictionary) {
            if(used.contains(atom.band_id))continue;
            auto fit=fit_native_eq_atom(atom,residual,rate,stereo_side);
            if(!fit||!usable_greedy_atom(fit->fit))continue;
            if(!best||atom_precedes_for_selection(fit->fit,best->fit))best=std::move(fit);
        }
        if(!best)break;
        const auto& response=best->fit.atom.exact_canonical_response_at_limited_gain;
        for(std::size_t i=0;i<residual.size();++i)residual[i].residual_db-=response[i];
        used.insert(best->payload.band_id());
        selected.push_back(std::move(*best));
        auto stored=selected;
        std::sort(stored.begin(),stored.end(),[](const auto& a,const auto& b){
            const auto& x=a.fit.atom;const auto& y=b.fit.atom;
            if(x.canonical_frequency_hz!=y.canonical_frequency_hz)
                return x.canonical_frequency_hz<y.canonical_frequency_hz;
            if(x.filter!=y.filter)return x.filter<y.filter;
            return x.q_or_slope<y.q_or_slope;
        });
        std::vector<dsp::EqBandParameters> bands;
        for(const auto& item:stored)bands.push_back(item.payload);
        auto payload=dsp::ParametricEqParameters::create(std::move(bands));
        if(!payload)return {};
        std::vector<double> frequencies;
        for(const auto& cell:active)frequencies.push_back(cell.frequency_hz);
        prefixes.push_back({std::move(*payload.value()),selected,std::move(frequencies)});
    }
    return prefixes;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
