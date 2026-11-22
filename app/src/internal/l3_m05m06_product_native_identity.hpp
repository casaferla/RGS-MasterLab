#pragma once
// L3 M05/M06 canonical, exact Product SourceIdentity for an immutable
// SOURCE, independent REFERENCE, or one physical M10 candidate.
// NOT a substituted Phase6 feature digest and NOT an M07 score.
// Length-delimited provider/locator/ticket bytes prevent ambiguity;
// epoch + independent generations prevent stale same-path selection.
#include "l3_product_offline_trial_job.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace rgsml::app::l3_automatch {
namespace native_owner_detail {
inline void append_exact_utf8(std::string& out,std::string_view value) {
    out+=std::to_string(value.size());
    out+=':';
    out.append(value.data(),value.size());
}
inline void append_exact_integer(std::string& out,std::uint64_t value) {
    out+=std::to_string(value);
    out+=';';
}
} // namespace native_owner_detail

enum class OriginalProductRole:std::uint8_t {
    prepared,reference
};
[[nodiscard]] inline m05m06_real::SourceIdentity
native_product_original_identity(
    const OfflineProductOwnerStamp& owner,OriginalProductRole role)
{
    if(!owner.populated())return {};
    const bool is_source=role==OriginalProductRole::prepared;
    const std::string_view prefix=is_source?
        "rgsml.l3.native.prepared/1;":
        "rgsml.l3.native.reference/1;";
    std::string identity{prefix};
    native_owner_detail::append_exact_integer(
        identity,owner.m18_owner_epoch);
    native_owner_detail::append_exact_integer(
        identity,is_source?
            owner.source_generation:owner.reference_selection_revision);
    native_owner_detail::append_exact_utf8(
        identity,is_source?owner.source_provider_id:
                           owner.reference_provider_id);
    native_owner_detail::append_exact_utf8(
        identity,is_source?owner.source_locator:
                           owner.reference_locator);
    return {std::move(identity),
            is_source?owner.source_generation:
                      owner.reference_selection_revision,true};
}
[[nodiscard]] inline m05m06_real::SourceIdentity
native_product_candidate_identity(
    const OfflineProductOwnerStamp& owner,
    std::string_view exact_m10_candidate_ticket)
{
    if(!owner.populated()||exact_m10_candidate_ticket.empty())
        return {};
    std::string identity{"rgsml.l3.native.candidate/1;"};
    native_owner_detail::append_exact_integer(
        identity,owner.m18_owner_epoch);
    native_owner_detail::append_exact_integer(
        identity,owner.source_generation);
    native_owner_detail::append_exact_integer(
        identity,owner.reference_selection_revision);
    native_owner_detail::append_exact_utf8(
        identity,owner.source_provider_id);
    native_owner_detail::append_exact_utf8(
        identity,owner.source_locator);
    native_owner_detail::append_exact_utf8(
        identity,owner.reference_provider_id);
    native_owner_detail::append_exact_utf8(
        identity,owner.reference_locator);
    native_owner_detail::append_exact_utf8(
        identity,exact_m10_candidate_ticket);
    return {std::move(identity),owner.source_generation,true};
}
[[nodiscard]] inline bool authentic_product_source_identity(
    const m05m06_real::SourceIdentity& found,
    const OfflineProductOwnerStamp& owner,
    OriginalProductRole role)
{
    const auto exact=native_product_original_identity(owner,role);
    return found.full_track_owner_confirmed&&
           found.generation==exact.generation&&
           !exact.resource_identity.empty()&&
           found.resource_identity==exact.resource_identity;
}
[[nodiscard]] inline bool authentic_product_candidate_identity(
    const m05m06_real::SourceIdentity& found,
    const OfflineProductOwnerStamp& owner,
    std::string_view exact_ticket)
{
    const auto exact=native_product_candidate_identity(owner,exact_ticket);
    return found.full_track_owner_confirmed&&
           found.generation==exact.generation&&
           !exact.resource_identity.empty()&&
           found.resource_identity==exact.resource_identity;
}
} // namespace rgsml::app::l3_automatch
