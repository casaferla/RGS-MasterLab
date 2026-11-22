#pragma once
// L3-M12 Project Open safety gate for the current Product. REF_LINKED
// native DSP decoding is implemented, but Product audio adoption
// (including its real REF-before-terminal-P2 execution position) is
// NOT yet installed in ProjectSessionViewModel. It is unsafe to open
// a calculated .rgsml, silently restore MANUAL only, and claim its
// REF_LINKED sound is playable. Fail BEFORE any SOURCE/Gold/chain
// mutation. Remove this refusal only alongside the real atomic
// restore+preview integration; this is not an M12 receipt.
#include <rgsml/project/project_document.hpp>
#include <algorithm>
#include <cstdint>
namespace rgsml::app::l3_automatch::m12_frozen {
enum class ProjectOpenLinkedSafety : std::uint8_t {
    ordinary_project_without_ref_linked,
    blocked_until_product_ref_linked_replay_is_installed
};
[[nodiscard]] inline ProjectOpenLinkedSafety
assess_current_product_ref_linked_open_safety(
    const project::ProjectDocument& doc) noexcept
{
    const bool has_linked=std::any_of(
        doc.chains.begin(),doc.chains.end(),
        [](const project::Chain& chain){
            return chain.stage=="MASTER"&&
                   chain.segment=="REF_LINKED";
        });
    return has_linked
        ?ProjectOpenLinkedSafety::
            blocked_until_product_ref_linked_replay_is_installed
        :ProjectOpenLinkedSafety::ordinary_project_without_ref_linked;
}
} // namespace rgsml::app::l3_automatch::m12_frozen
