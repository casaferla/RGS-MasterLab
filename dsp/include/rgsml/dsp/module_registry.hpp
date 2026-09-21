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
