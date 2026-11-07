// Actual canonical C++ type surface compile, without linking/activation.
// Private port is an interface-only integration rehearsal, NOT real rendering.
#include "m22_canonical_chain_port_candidate.hpp"
#include <type_traits>
#include <utility>
#include <span>
#include <vector>

using namespace rgsml;
using namespace rgsml::m22_private;

static_assert(std::is_same_v<
    decltype(port_stored_manual_chain_candidate(
        std::declval<const dsp::ProcessingChain&>(),
        std::declval<const dsp::ModuleRegistry&>(),
        std::declval<std::span<const dsp::ModuleExecutionBinding>>(),
        std::declval<std::span<const ExternalFrozenExecutionEvidence>>(),
        std::declval<TypedPortError&>())),
    std::optional<std::vector<InlineSnapshot>>>);

void compile_canonical_dsp_port(
    const dsp::ProcessingChain& chain,
    const dsp::ModuleRegistry& registry,
    std::span<const dsp::ModuleExecutionBinding> bindings,
    std::span<const ExternalFrozenExecutionEvidence> proofs) {
    TypedPortError err{};
    [[maybe_unused]] auto out =
        port_stored_manual_chain_candidate(chain,registry,bindings,proofs,err);
}
