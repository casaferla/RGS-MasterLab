#pragma once
#include "l3_m09_native_domain_stream.hpp"
#include "l3_m09_native_eq_dictionary.hpp"
#include <functional>
#include <map>
#include <set>

namespace rgsml::app::l3_automatch::m09_frozen {
// One worker-local allocation table for the whole accepted generation.
// UUIDs are issued by the existing Product allocator, not by hashing
// floating point frequency or deriving persistent IDs from topology.
class NativeOwnerAllocations final {
public:
    using AllocateUuid=std::function<core::Uuid()>;
    explicit NativeOwnerAllocations(AllocateUuid allocate,
        std::span<const core::Uuid> protected_ids={})
        :allocate_(std::move(allocate)),used_(protected_ids.begin(),protected_ids.end()) {}
    [[nodiscard]] std::optional<core::Uuid> band(IntentRole role,const NativeEqDictionaryIdentity& atom) {
        if(role!=IntentRole::tonal_eq&&role!=IntentRole::stereo_side_eq)return {};
        if(!std::isfinite(atom.q_or_slope))return {};
        ExactBytes key;
        using namespace m10_frozen::p2_cache_detail;
        append_exact_field(key,"rgsml.l3.eq-dictionary-band/1");
        append_exact_field(key,std::to_string(static_cast<unsigned>(role)));
        append_exact_field(key,std::to_string(atom.canonical_index));
        append_exact_field(key,std::to_string(static_cast<unsigned>(atom.filter)));
        append_exact_field(key,std::to_string(std::bit_cast<std::uint64_t>(atom.q_or_slope)));
        return bind_band(role,std::move(key));
    }
    [[nodiscard]] std::optional<core::Uuid> dynamic_band(std::size_t rank) {
        if(rank>=3U)return {};
        ExactBytes key;
        using namespace m10_frozen::p2_cache_detail;
        append_exact_field(key,"rgsml.l3.dynamic-cluster-rank/1");
        append_exact_field(key,std::to_string(rank));
        return bind_band(IntentRole::spectral_dynamic_eq,std::move(key));
    }
    [[nodiscard]] std::optional<ExactBytes> band_identity(IntentRole role,const core::Uuid& id) const {
        const auto found=band_keys_.find(id);
        if(found==band_keys_.end()||found->second.first!=role)return {};
        return found->second.second;
    }
    [[nodiscard]] static ExactBytes role_key(IntentRole role) {
        ExactBytes out;
        using namespace m10_frozen::p2_cache_detail;
        append_exact_field(out,"rgsml.l3.semantic-role/1");
        append_exact_field(out,std::to_string(static_cast<unsigned>(role)));
        return out;
    }
    [[nodiscard]] std::optional<NativeRoleAllocation> role(IntentRole role) {
        if(!native_artistic_role_type(role))return {};
        if(const auto found=roles_.find(role);found!=roles_.end())return found->second;
        const auto instance=next(),semantic=next();
        if(!instance||!semantic)return {};
        auto typed=dsp::ModuleInstanceId::from_uuid(*instance);
        if(!typed)return {};
        NativeRoleAllocation allocation{role,*typed.value(),*semantic};
        roles_.emplace(role,allocation);
        return allocation;
    }
    [[nodiscard]] bool failed()const noexcept{return failed_;}
private:
    [[nodiscard]] std::optional<core::Uuid> next() {
        if(failed_||!allocate_)return {};
        const auto id=allocate_();
        if(id.is_nil()||!used_.insert(id).second){failed_=true;return {};}
        return id;
    }
    [[nodiscard]] std::optional<core::Uuid> bind_band(IntentRole role,ExactBytes key) {
        if(failed_)return {};
        if(const auto found=bands_.find(key);found!=bands_.end())return found->second;
        auto id=next();if(!id)return {};
        bands_.emplace(key,*id);
        band_keys_.emplace(*id,std::pair{role,std::move(key)});
        return id;
    }
    AllocateUuid allocate_;
    std::set<core::Uuid> used_;
    std::map<ExactBytes,core::Uuid> bands_;
    std::map<core::Uuid,std::pair<IntentRole,ExactBytes>> band_keys_;
    std::map<IntentRole,NativeRoleAllocation> roles_;
    bool failed_{};
};
// Canonical topology carries discrete intent and selected evidence keys,
// never the allocation table's persistence UUIDs or rounded frequencies.
// The independently accepted generation/dependency tuple owns the full
// source-grid, Amount and registry revisions.
[[nodiscard]] inline std::optional<ExactBytes>
native_owner_family_topology(const NativeFamilySeedBank& bank,
    const AbstractFamilyChoice& family,const NativeOwnerAllocations& allocations)
{
    ExactBytes out;
    using namespace m10_frozen::p2_cache_detail;
    append_exact_field(out,kCandidateSchema);
    append_exact_field(out,kEnumerationPolicy);
    for(auto value:{family.tonal_eq_prefix,family.spectral_dynamic_eq_prefix,
        static_cast<std::uint8_t>(family.macro),static_cast<std::uint8_t>(family.micro),
        family.stereo_side_eq_prefix,family.stereo_mode_index,
        static_cast<std::uint8_t>(family.loudness)})
        append_exact_field(out,std::to_string(value));
    const auto roles=ordered_abstract_roles(family);
    append_exact_field(out,std::to_string(roles.size()));
    for(const auto role:roles) {
        const auto type=native_artistic_role_type(role);
        const auto payload=native_family_role_payload(bank,family,role);
        if(!type||!payload)return {};
        append_exact_byte_field(out,NativeOwnerAllocations::role_key(role));
        append_exact_field(out,*type);
        if(const auto* eq=std::get_if<dsp::ParametricEqParameters>(&*payload)) {
            const auto& grid=role==IntentRole::tonal_eq?bank.exact_tonal_grid:bank.exact_side_grid;
            append_exact_field(out,std::to_string(grid.size()));
            for(const auto& cell:grid)append_exact_field(out,std::to_string(cell.canonical_index));
            for(const auto& band:eq->bands()) {
                const auto key=allocations.band_identity(role,band.band_id());
                if(!key)return {};
                append_exact_byte_field(out,*key);
            }
        } else if(const auto* eq=std::get_if<dsp::DynamicEqParameters>(&*payload)) {
            const auto& prefix=bank.spectral[family.spectral_dynamic_eq_prefix-1U];
            if(prefix.ranked_clusters.size()!=eq->bands().size())return {};
            for(std::size_t i=0;i<eq->bands().size();++i) {
                const auto key=allocations.band_identity(role,eq->bands()[i].spec().band_id);
                if(!key)return {};
                append_exact_byte_field(out,*key);
                append_exact_field(out,std::to_string(prefix.ranked_clusters[i].first));
                append_exact_field(out,std::to_string(prefix.ranked_clusters[i].count));
            }
        }
    }
    return out;
}
// Owner-attested exact dependency bytes come from the current accepted
// input snapshot. The seed bank provides only actual analytical roles.
// Unsupported conditional composition is explicitly unresolved, not NONE.
[[nodiscard]] inline NativeFamilyBindingAuthority
resolve_native_owner_family(const NativeFamilySeedBank& bank,
    const AbstractFamilyChoice& family,NativeOwnerAllocations& allocations,
    const dsp::ModuleRegistry& registry,const ExactBytes& generation,
    const ExactBytes& exact_dependencies,bool fixed_peak_exceeds_ceiling)
{
    NativeFamilyBindingAuthority out;
    out.exact_reason="missing-current-generation-dependencies";
    if(generation.empty()||exact_dependencies.empty()||allocations.failed())return out;
    const auto conditional=[&](const char* reason){
        out.disposition=NativeDomainDisposition::not_applicable;
        out.exact_reason=reason;
        return out;
    };
    if(family.loudness==LoudnessMode::gain_with_terminal_limiter) {
        if(!bank.gain||!bank.gain->terminal_branch_requested)
            return conditional("terminal-branch-condition-not-met");
        out.exact_reason="conditional-terminal-composition-unresolved";return out;
    }
    if(family.loudness==LoudnessMode::terminal_limiter_only) {
        if(!fixed_peak_exceeds_ceiling)return conditional("fixed-peak-within-independent-ceiling");
        out.exact_reason="conditional-terminal-composition-unresolved";return out;
    }
    if(family.stereo_side_eq_prefix&&family.stereo_mode_index) {
        if(family.stereo_mode_index>bank.stereo.size()||
           !bank.stereo[family.stereo_mode_index-1U])
            return conditional("no-native-stereo-seed");
        out.exact_reason="conditional-post-side-insertion-baseline-unresolved";return out;
    }
    for(const auto role:ordered_abstract_roles(family)) {
        if(!native_family_role_payload(bank,family,role))
            return conditional("no-current-eligible-native-seed");
        const auto type=native_artistic_role_type(role);
        if(!type||!registry.has_factory(*type)) {
            out.exact_reason="native-factory-unavailable";return out;
        }
    }
    const auto topology=native_owner_family_topology(bank,family,allocations);
    if(!topology){out.exact_reason="native-topology-evidence-unavailable";return out;}
    for(const auto role:ordered_abstract_roles(family)) {
        auto allocated=allocations.role(role);
        if(!allocated){out.exact_reason="owner-role-allocation-failed";return out;}
        out.allocations.push_back(*allocated);
        out.role_keys.push_back({role,NativeOwnerAllocations::role_key(role)});
    }
    out.identity={*topology,{},exact_dependencies,generation,{"native-owner-seed-bank"}};
    out.available_currently=true;
    out.disposition=NativeDomainDisposition::legal;
    out.exact_reason="native-artistic-seed-and-owner-bound";
    return out;
}
} // namespace rgsml::app::l3_automatch::m09_frozen
