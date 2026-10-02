#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/module_instance.hpp>
#include <rgsml/dsp/module_registry.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

namespace rgsml::dsp {

struct ProcessingChainContext final {
    ProcessingStage stage;
    ChainSegment segment;

    friend bool operator==(const ProcessingChainContext&, const ProcessingChainContext&) = default;
};

class ProcessingChain final {
public:
    [[nodiscard]] static rgsml::core::Result<ProcessingChain>
    create(const ModuleRegistry& registry, ProcessingChainContext context);

    [[nodiscard]] static rgsml::core::Result<ProcessingChain> restore(
        const ModuleRegistry& registry,
        ProcessingChainContext context,
        std::uint64_t revision,
        std::vector<ModuleInstance> instances);

    [[nodiscard]] ProcessingChainContext context() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] std::span<const ModuleInstance> instances() const noexcept;
    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const ModuleInstance>>
    find_instance(const ModuleInstanceId& instance_id) const;

    [[nodiscard]] rgsml::core::Status add(
        ModuleInstanceId instance_id,
        std::string_view module_type_id,
        std::size_t final_index);
    [[nodiscard]] rgsml::core::Status remove(const ModuleInstanceId& instance_id);
    [[nodiscard]] rgsml::core::Status move(
        const ModuleInstanceId& instance_id,
        std::size_t final_index);
    [[nodiscard]] rgsml::core::Status duplicate(
        const ModuleInstanceId& source_id,
        ModuleInstanceId new_id,
        std::size_t final_index);
    [[nodiscard]] rgsml::core::Status set_user_bypass(
        const ModuleInstanceId& instance_id,
        bool bypassed);

private:
    ProcessingChain(const ModuleRegistry& registry, ProcessingChainContext context) noexcept;

    [[nodiscard]] rgsml::core::Status validate(
        std::span<const ModuleInstance> candidate) const;
    [[nodiscard]] rgsml::core::Status commit(std::vector<ModuleInstance> candidate);

    const ModuleRegistry* registry_;
    ProcessingChainContext context_;
    std::uint64_t revision_{0};
    std::vector<ModuleInstance> instances_;
};

}  // namespace rgsml::dsp
