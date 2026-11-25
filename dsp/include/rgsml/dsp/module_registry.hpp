#pragma once

#include <rgsml/core/result.hpp>
#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <rgsml/dsp/module_parameter_payload.hpp>

#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace rgsml::dsp {

class IModuleFactory {
public:
    virtual ~IModuleFactory() = default;
    [[nodiscard]] virtual std::string_view module_type_id() const noexcept = 0;
    [[nodiscard]] virtual rgsml::core::Result<std::unique_ptr<IModule>> create() const = 0;

protected:
    IModuleFactory() = default;
};

struct ModuleRegistration final {
    ModuleDescriptor descriptor;
    std::shared_ptr<const IModuleFactory> factory;
};

class ModuleRegistry final {
public:
    [[nodiscard]] static rgsml::core::Result<ModuleRegistry>
    create(std::vector<ModuleRegistration> registrations);

    [[nodiscard]] static rgsml::core::Result<ModuleRegistry>
    create_dsp_package_v1();


    // Explicit I2 PREVIEW opt-in: add independent P2 streaming to a caller-owned
    // registry. Never mutate the frozen package v1/M17 terminal registration.
    [[nodiscard]] static rgsml::core::Result<ModuleRegistry>
    with_streaming_true_peak_preview(ModuleRegistry baseline);

    // Explicit AutoMatch snapshot extension: same native P2 factory/schema,
    // permitting an owner-controlled final REF_LINKED streaming DSP.
    // Does not change the caller's live MANUAL registry or M17.
    [[nodiscard]] static rgsml::core::Result<ModuleRegistry>
    with_streaming_true_peak_automatch(ModuleRegistry baseline);

    // Explicit L3 offline opt-in, not part of package_v1 or live Product GUI.
    // Typed DynamicEqParameters only; no implicit unbound defaults.
    [[nodiscard]] static rgsml::core::Result<ModuleRegistry>
    with_dynamic_eq_offline(ModuleRegistry baseline);

    // Explicit L3-M03 opt-in: attach to the existing frozen L1 placeholder;
    // never register a second descriptor or activate in default Product.
    [[nodiscard]] static rgsml::core::Result<ModuleRegistry>
    with_transient_shaper_offline(ModuleRegistry baseline);

    [[nodiscard]] std::span<const ModuleDescriptor> descriptors() const noexcept;
    [[nodiscard]] rgsml::core::Result<std::reference_wrapper<const ModuleDescriptor>>
    find_descriptor(std::string_view type_id) const;
    [[nodiscard]] bool has_factory(std::string_view type_id) const noexcept;
    [[nodiscard]] std::size_t factory_count() const noexcept;
    [[nodiscard]] rgsml::core::Result<std::unique_ptr<IModule>>
    create_module(std::string_view type_id) const;

    [[nodiscard]] rgsml::core::Result<std::unique_ptr<IModule>>
    create_module(
        std::string_view type_id,
        const ModuleParameterPayload& payload) const;

private:
    explicit ModuleRegistry(std::vector<ModuleRegistration> registrations);
    std::vector<ModuleRegistration> registrations_;
    std::vector<ModuleDescriptor> descriptors_;
};

}  // namespace rgsml::dsp
