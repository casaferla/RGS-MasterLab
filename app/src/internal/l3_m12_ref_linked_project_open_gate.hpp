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
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <cstdint>
namespace rgsml::app::l3_automatch::m12_frozen {
enum class ProjectOpenLinkedSafety : std::uint8_t {
    ordinary_project_without_ref_linked,
    blocked_until_product_ref_linked_replay_is_installed
};
[[nodiscard]] inline ProjectOpenLinkedSafety
assess_current_product_ref_linked_open_safety(
    const project::ProjectDocument& doc)
{
    // A malformed/foreign segment must never evade a gate that only
    // searches for the conventional MASTER + REF_LINKED pair. Likewise
    // a controller-owned node inside MANUAL must not be replayed as
    // an ordinary user module by a legacy Product decoder.
    const bool has_linked=std::any_of(
        doc.chains.begin(),doc.chains.end(),
        [](const project::Chain& chain){
            return chain.segment=="REF_LINKED"||
                std::any_of(chain.modules.begin(),chain.modules.end(),
                    [](const project::Module& node){
                        return node.linkState=="REF_LINKED"||
                               node.owner=="REFERENCE_MATCH"||
                               node.provenance=="REFERENCE_MATCH";
                    });
        });
    // The segment can be absent from a corrupt or interrupted project
    // while authoritative materialization metadata remains. Never
    // silently turn such a project into a MANUAL-only audible session.
    const auto metadata=QJsonDocument::fromJson(
        QByteArray::fromStdString(doc.referenceMatch.canonical_utf8()));
    const bool unsafe_metadata=!metadata.isObject()||
        metadata.object().contains(QStringLiteral("materialization"));
    return has_linked||unsafe_metadata
        ?ProjectOpenLinkedSafety::
            blocked_until_product_ref_linked_replay_is_installed
        :ProjectOpenLinkedSafety::ordinary_project_without_ref_linked;
}
} // namespace rgsml::app::l3_automatch::m12_frozen
