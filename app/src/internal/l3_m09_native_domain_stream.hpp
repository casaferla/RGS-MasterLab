#pragma once
#include "l3_m09_native_covered_proxy.hpp"

namespace rgsml::app::l3_automatch::m09_frozen {
using DomainFamilyLists=std::array<std::vector<AbstractFamilyChoice>,4>;
[[nodiscard]] inline std::optional<DomainFamilyLists>
enumerate_native_domain_families(const NativeFamilySeedBank& bank,bool stereo)
{
    if(bank.tonal.size()>6U||bank.spectral.size()>3U||bank.side.size()>3U||
       (!stereo&&!bank.side.empty()))return {};
    DomainFamilyLists result;
    for(std::size_t t=0;t<=bank.tonal.size();++t) {
        AbstractFamilyChoice c;c.tonal_eq_prefix=static_cast<std::uint8_t>(t);
        result[0].push_back(c);
    }
    for(std::size_t d=0;d<=bank.spectral.size();++d)
        for(std::uint8_t m=0;m<(stereo?3U:2U);++m)
            for(std::uint8_t u=0;u<4U;++u) {
                AbstractFamilyChoice c;c.spectral_dynamic_eq_prefix=static_cast<std::uint8_t>(d);
                c.macro=static_cast<MacroMode>(m);c.micro=static_cast<MicroMode>(u);
                result[1].push_back(c);
            }
    for(std::size_t s=0;s<=bank.side.size();++s)
        for(std::size_t m=0;m<(stereo?kLowBassAlternatives.size()+1U:1U);++m) {
            AbstractFamilyChoice c;c.stereo_side_eq_prefix=static_cast<std::uint8_t>(s);
            c.stereo_mode_index=static_cast<std::uint8_t>(m);result[2].push_back(c);
        }
    for(std::uint8_t l=0;l<4U;++l) {
        AbstractFamilyChoice c;c.loudness=static_cast<LoudnessMode>(l);
        result[3].push_back(c);
    }
    return result;
}
[[nodiscard]] inline bool same_abstract_family(const AbstractFamilyChoice& a,
    const AbstractFamilyChoice& b) noexcept
{
    return a.tonal_eq_prefix==b.tonal_eq_prefix&&
        a.spectral_dynamic_eq_prefix==b.spectral_dynamic_eq_prefix&&a.macro==b.macro&&
        a.micro==b.micro&&a.stereo_side_eq_prefix==b.stereo_side_eq_prefix&&
        a.stereo_mode_index==b.stereo_mode_index&&a.loudness==b.loudness;
}
[[nodiscard]] inline std::optional<AbstractFamilyChoice> combine_domain_families(
    const std::array<AbstractFamilyChoice,4>& domains)
{
    AbstractFamilyChoice combined;
    for(std::size_t i=0;i<4U;++i) {
        AbstractFamilyChoice expected;
        const auto& c=domains[i];
        if(i==0U)expected.tonal_eq_prefix=c.tonal_eq_prefix;
        if(i==1U) {
            expected.spectral_dynamic_eq_prefix=c.spectral_dynamic_eq_prefix;
            expected.macro=c.macro;expected.micro=c.micro;
        }
        if(i==2U) {
            expected.stereo_side_eq_prefix=c.stereo_side_eq_prefix;
            expected.stereo_mode_index=c.stereo_mode_index;
        }
        if(i==3U)expected.loudness=c.loudness;
        if(!same_abstract_family(c,expected))return {};
    }
    combined.tonal_eq_prefix=domains[0].tonal_eq_prefix;
    combined.spectral_dynamic_eq_prefix=domains[1].spectral_dynamic_eq_prefix;
    combined.macro=domains[1].macro;combined.micro=domains[1].micro;
    combined.stereo_side_eq_prefix=domains[2].stereo_side_eq_prefix;
    combined.stereo_mode_index=domains[2].stereo_mode_index;
    combined.loudness=domains[3].loudness;
    return combined;
}
enum class NativeDomainDisposition:std::uint8_t {
    legal,not_applicable,unresolved
};
struct NativeDomainEntry final {
    AbstractFamilyChoice family;
    NativeDomainDisposition disposition{NativeDomainDisposition::unresolved};
    std::optional<m10_frozen::ConsumerInput> legal;
    std::string exact_reason;
};
struct NativeDomainSelection final {
    bool ready{};
    std::array<std::vector<NativeDomainEntry>,4> ledger;
    std::array<m10_frozen::DomainConsumerResult,4> numeric;
    DomainFamilyLists retained;
};
// Availability is intentionally NOT a legality filter. A legal unavailable
// entry with a bound proxy participates in numeric top8 unchanged. Missing
// authority blocks the full corrective domain, never a partial ranking.
template<class ResolveDomainEntry>
[[nodiscard]] inline NativeDomainSelection select_native_domain_families(
    const DomainFamilyLists& canonical,const std::array<bool,4>& corrective,
    ResolveDomainEntry&& resolve)
{
    NativeDomainSelection out;
    for(std::size_t d=0;d<4U;++d) {
        if(canonical[d].empty()||!same_abstract_family(canonical[d].front(),{}))return out;
        if(!corrective[d]) {
            for(const auto& c:canonical[d])
                out.ledger[d].push_back({c,NativeDomainDisposition::not_applicable,{},
                    "domain-not-corrective"});
            out.retained[d].push_back({});
            continue;
        }
        std::vector<m10_frozen::ConsumerInput> legal;
        std::vector<std::size_t> legal_to_original;
        bool unresolved=false;
        for(const auto& c:canonical[d]) {
            auto e=std::invoke(resolve,d,c);
            if(!same_abstract_family(e.family,c)||e.exact_reason.empty()||
               (e.disposition==NativeDomainDisposition::legal)!=e.legal.has_value())
                return out;
            if(e.disposition==NativeDomainDisposition::unresolved)unresolved=true;
            if(e.legal) {
                legal_to_original.push_back(out.ledger[d].size());
                legal.push_back(*e.legal);
            }
            out.ledger[d].push_back(std::move(e));
        }
        if(unresolved)return out;
        out.numeric[d]=m10_frozen::select_domain_top8(legal);
        if(out.numeric[d].status!=m10_frozen::ConsumerStatus::ready)return out;
        // Preserve canonical structural order for the Cartesian traversal,
        // separate from numeric rank order held by the consumer.
        auto indices=out.numeric[d].numeric_selected_indices;
        std::sort(indices.begin(),indices.end());
        for(auto i:indices) {
            if(i>=legal_to_original.size())return out;
            out.retained[d].push_back(out.ledger[d][legal_to_original[i]].family);
        }
    }
    out.ready=true;
    return out;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
