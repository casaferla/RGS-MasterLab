#pragma once
#include "l3_m09_abstract_family_enumeration.hpp"
#include <rgsml/core/uuid.hpp>
#include <rgsml/core/sha256.hpp>
#include <string>
#include <string_view>
#include <optional>
#include <span>
namespace rgsml::app::l3_automatch::m09_frozen {
// Stable named roles, independent of enum ordinals, topology order,
// Amount, plan, target or the concrete module UUID allocator.
[[nodiscard]] inline std::string_view controller_role_name(IntentRole role)noexcept {
    switch(role) {
    case IntentRole::tonal_eq:return "tonal-eq";
    case IntentRole::spectral_dynamic_eq:return "spectral-dynamic-eq";
    case IntentRole::macro_compressor:return "macro-compressor";
    case IntentRole::micro_transient:return "micro-transient";
    case IntentRole::stereo_side_eq:return "stereo-side-eq";
    case IntentRole::stereo_ms:return "stereo-ms";
    case IntentRole::loudness_gain:return "loudness-gain";
    case IntentRole::terminal_limiter:return "terminal-limiter";
    }
    return {};
}
[[nodiscard]] inline std::optional<core::Uuid>
native_controller_role_semantic_id(core::Uuid controller,IntentRole role)
{
    const auto name=controller_role_name(role);
    if(controller.is_nil()||name.empty())return {};
    const auto text=std::string{"rgsml.reference-match.semantic-role/1/"}+
        controller.to_string()+"/"+std::string{name};
    core::Sha256 hash;
    hash.update(std::as_bytes(std::span{text.data(),text.size()}));
    const auto digest=hash.finalize();
    core::Uuid::Bytes id{};
    for(std::size_t i=0;i<id.size();++i)id[i]=std::to_integer<std::uint8_t>(digest[i]);
    id[6]=static_cast<std::uint8_t>((id[6]&0x0fU)|0x80U);
    id[8]=static_cast<std::uint8_t>((id[8]&0x3fU)|0x80U);
    return core::Uuid{id};
}
} // namespace rgsml::app::l3_automatch::m09_frozen
