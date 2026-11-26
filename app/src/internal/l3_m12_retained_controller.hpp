#pragma once
// M12 P02: deleting only a plan never retires its durable controller.
// This record carries identity and a revision floor, not DSP or safety.
#include <rgsml/core/uuid.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <cstdint>
#include <optional>
#include <string>
namespace rgsml::app::l3_automatch::m12_frozen {
struct M12RetainedController final {
    core::Uuid controller_id,linked_chain_id;
    std::uint64_t last_removed_revision{};
};
[[nodiscard]] inline std::optional<M12RetainedController>
decode_m12_retained_controller(const QJsonObject& root)
{
    const auto v=root.value(QStringLiteral("retainedController"));
    if(!v.isObject())return {};
    const auto obj=v.toObject();
    if(obj.size()!=4||obj.value(QStringLiteral("typeId")).toString()!=
        QStringLiteral("rgsml.reference-match.global"))return {};
    const auto c=core::Uuid::parse(obj.value(QStringLiteral("referenceMatchControllerId")).toString().toStdString());
    const auto l=core::Uuid::parse(obj.value(QStringLiteral("linkedChainId")).toString().toStdString());
    const auto revision=obj.value(QStringLiteral("lastRemovedRevision")).toString();
    bool ok=false;const auto n=revision.toULongLong(&ok);
    if(!c||!l||c.value()->is_nil()||l.value()->is_nil()||*c.value()==*l.value()||
        !ok||n==0U||revision!=QString::number(n))return {};
    return M12RetainedController{*c.value(),*l.value(),static_cast<std::uint64_t>(n)};
}
[[nodiscard]] inline QJsonObject encode_m12_retained_controller(const M12RetainedController& c)
{
    return {{"typeId",QStringLiteral("rgsml.reference-match.global")},
        {"referenceMatchControllerId",QString::fromStdString(c.controller_id.to_string())},
        {"linkedChainId",QString::fromStdString(c.linked_chain_id.to_string())},
        {"lastRemovedRevision",QString::fromStdString(std::to_string(c.last_removed_revision))}};
}
} // namespace rgsml::app::l3_automatch::m12_frozen
