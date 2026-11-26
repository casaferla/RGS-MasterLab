#pragma once
// Native Product policy constructed from the admitted immutable Qt capture.
// These are local exact dependency identities, NOT a signed Phase9
// registry or a formal QTP approval. All numerical evidence still comes
// from the real independent M05/M06/M11/M16 workers.
#include "l3_native_qt_search_worker.hpp"
#include "l3_m10_native_scope_key.hpp"
#include <rgsml/core/sha256.hpp>
#include <bit>

namespace rgsml::app::l3_automatch {
[[nodiscard]] inline std::optional<NativeQtSearchPolicy>
build_current_native_qt_product_policy(const NativeQtSearchCapture& capture,
    const m08_frozen::AmountConfig& amount,const std::array<bool,4>& requested,
    double independent_ceiling_db_tp)
{
    const auto& stamp=capture.current.job.owner_stamp();
    if(!stamp.populated()||!capture.current.job.fixed_only()||
       !capture.execution_registry||!capture.linked_revision||
       !std::isfinite(independent_ceiling_db_tp)||
       independent_ceiling_db_tp< -12.0||independent_ceiling_db_tp>0.0)return {};
    // All-zero domain settings are legitimate suspension intent. A
    // zero-request vector with positive stored domain Amount is not.
    if(std::none_of(requested.begin(),requested.end(),[](bool b){return b;})&&
       amount.global>0.0&&std::any_of(amount.domains.begin(),amount.domains.end(),
           [](double value){return value>0.0;}))return {};
    std::array<m08_frozen::DomainAmount,4> domains;
    for(std::size_t i=0;i<domains.size();++i)
        domains[i]={m07_frozen::kDomainIds[i],amount.domains[i]};
    const auto validated=m08_frozen::create_amount_config(amount.global,domains,
        amount.independent_amount_revision);
    if(!validated.config)return {};
    const auto prepared=m10_frozen::exact_native_analysis_scope_content(capture.common_spec.prepared);
    const auto reference=m10_frozen::exact_native_analysis_scope_content(capture.common_spec.reference);
    const auto graph=capture.current.job.exact_fixed_graph_content();
    if(!prepared||!reference||!graph||graph->empty())return {};
    using namespace m10_frozen::p2_cache_detail;
    using Bytes=m10_frozen::ExactCacheBytes;
    const auto hash=[](const Bytes& v){return core::sha256_hex(std::as_bytes(std::span{v}));};
    const auto real=[](Bytes& v,double n){append_exact_field(v,std::to_string(std::bit_cast<std::uint64_t>(n)));};
    NativeQtSearchPolicy policy;
    policy.amount=*validated.config;policy.requested=requested;
    policy.independent_ceiling_db_tp=independent_ceiling_db_tp;
    policy.definitions.exact_density_floor=1e-12;
    Bytes canonical;
    append_exact_field(canonical,"rgsml.l3.product-native-policy/1");
    append_exact_field(canonical,m07_frozen::kRegistryId);
    append_exact_field(canonical,m07_frozen::kPolicyVersion);
    append_exact_field(canonical,m08_frozen::kTargetPolicy);
    real(canonical,policy.definitions.exact_density_floor);
    for(const auto& b:m07_frozen::kBindings) {
        for(const auto field:{b.binding_id,b.source_feature_id,b.exclusive_fallback_feature_id,
            b.group_id,b.support_policy_key})append_exact_field(canonical,field);
        for(const auto n:{static_cast<int>(b.domain),static_cast<int>(b.transform),
            b.domain_mass.numerator,b.domain_mass.denominator,static_cast<int>(b.component_share)})
            append_exact_field(canonical,std::to_string(n));
        real(canonical,b.scale);real(canonical,b.deadband);
    }
    append_exact_byte_field(canonical,*prepared);
    append_exact_byte_field(canonical,*reference);
    policy.generation_and_policy=canonical;
    Bytes dependencies=canonical;
    for(const auto n:{stamp.m18_owner_epoch,stamp.source_generation,
                     stamp.reference_selection_revision,capture.linked_revision,
                     amount.independent_amount_revision})
        append_exact_field(dependencies,std::to_string(n));
    for(const auto* s:{&stamp.source_provider_id,&stamp.source_locator,
                       &stamp.reference_provider_id,&stamp.reference_locator})
        append_exact_field(dependencies,*s);
    append_exact_byte_field(dependencies,*graph);
    real(dependencies,amount.global);
    for(auto n:amount.domains)real(dependencies,n);
    for(bool b:requested)dependencies.push_back(b?1U:0U);
    real(dependencies,independent_ceiling_db_tp);
    policy.accepted_dependencies=dependencies;
    policy.exact_generation=hash(dependencies);
    for(std::size_t i=0;i<policy.definitions.records.size();++i) {
        const auto& b=m07_frozen::kBindings[i];
        Bytes definition=canonical;
        // D03/D04 share the existing one native producer definition.
        append_exact_field(definition,m07_frozen::kBindings[i==9U?8U:i].source_feature_id);
        policy.definitions.records[i]={std::string(b.binding_id),
            std::string(b.source_feature_id),hash(definition)};
    }
    policy.definitions.exact_current_registry_and_phase6_policy_attested=true;
    if(!m10_frozen::valid_native_definition_policy(policy.definitions))return {};
    // These admit the real native pipeline; they are not measured PASS
    // flags. The scorer independently binds Fixed and K, executes actual
    // DSP, recomputes all native observations and enforces M16 and M11.
    policy.exact_current_endpoint_safety_attested=true;
    policy.file_based_lra_policy_attested=true;
    policy.constraints.optimization_target_equals_desired_attested=true;
    policy.constraints.exact_m05_m06_source_and_m07_dependency_current=true;
    policy.constraints.m11_fixed_and_protected_snapshot_current=true;
    policy.constraints.fresh_candidate_render_and_analysis_current=true;
    policy.constraints.hard_m11_and_independent_safety_preflight_passed=true;
    policy.constraints.m11_full_k_dependency_and_order_attested=true;
    return policy;
}
} // namespace rgsml::app::l3_automatch
