#include <rgsml/dsp/module_registry.hpp>

#include <rgsml/core/error.hpp>
#include <rgsml/dsp/gain_module.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/parametric_eq_module.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace rgsml::dsp {
namespace {

using rgsml::core::Error;
using rgsml::core::ErrorCode;
using rgsml::core::Result;

constexpr auto kGainTypeId = "rgsml.dsp.gain";
constexpr auto kEqTypeId = "rgsml.dsp.parametric-eq";

class GainFactory final : public IModuleFactory {
public:
    explicit GainFactory(ModuleDescriptor descriptor)
        : descriptor_(std::move(descriptor))
    {
    }

    [[nodiscard]] std::string_view module_type_id() const noexcept override
    {
        return descriptor_.type_id();
    }

    [[nodiscard]] Result<std::unique_ptr<IModule>> create() const override
    {
        auto parameters = GainParameters::create(0.0);
        if (!parameters) {
            return Result<std::unique_ptr<IModule>>::failure(*parameters.error());
        }
        auto module = GainModule::create(descriptor_, *parameters.value());
        if (!module) {
            return Result<std::unique_ptr<IModule>>::failure(*module.error());
        }
        std::unique_ptr<IModule> result = std::move(*module.value());
        return Result<std::unique_ptr<IModule>>::success(std::move(result));
    }

private:
    ModuleDescriptor descriptor_;
};

class ParametricEqFactory final : public IModuleFactory {
public:
    explicit ParametricEqFactory(ModuleDescriptor descriptor)
        : descriptor_(std::move(descriptor))
    {
    }

    [[nodiscard]] std::string_view module_type_id() const noexcept override
    {
        return descriptor_.type_id();
    }

    [[nodiscard]] Result<std::unique_ptr<IModule>> create() const override
    {
        auto parameters = ParametricEqParameters::create_legacy_default();
        if (!parameters) {
            return Result<std::unique_ptr<IModule>>::failure(*parameters.error());
        }
        auto module = ParametricEqModule::create(descriptor_, *parameters.value());
        if (!module) {
            return Result<std::unique_ptr<IModule>>::failure(*module.error());
        }
        std::unique_ptr<IModule> result = std::move(*module.value());
        return Result<std::unique_ptr<IModule>>::success(std::move(result));
    }

private:
    ModuleDescriptor descriptor_;
};

[[nodiscard]] Error registry_error(
    ErrorCode code,
    std::string category,
    std::string message)
{
    return Error{code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] bool unsigned_ascii_less(
    std::string_view left,
    std::string_view right) noexcept
{
    return std::lexicographical_compare(
        left.begin(),
        left.end(),
        right.begin(),
        right.end(),
        [](char lhs, char rhs) {
            return static_cast<unsigned char>(lhs) < static_cast<unsigned char>(rhs);
        });
}

[[nodiscard]] ModuleDescriptorSpec make_spec(
    std::string typeId,
    std::vector<ModuleCategory> categories,
    std::vector<ProcessingStage> stages,
    std::vector<ChainSegment> segments,
    bool duplicable,
    PlacementClass placement,
    std::optional<TerminalSlot> terminalSlot,
    std::vector<std::string> recommendedBefore = {},
    bool mustBeLast = false,
    bool singleActive = false,
    std::optional<std::string> algorithmVersion = std::nullopt,
    std::optional<std::string> parameterSchemaId = std::nullopt)
{
    const auto displayNameKey = typeId + ".display-name";
    return ModuleDescriptorSpec{
        std::move(typeId),
        displayNameKey,
        std::move(categories),
        std::move(stages),
        std::move(segments),
        duplicable,
        true,
        placement,
        terminalSlot,
        {},
        {},
        std::move(recommendedBefore),
        {},
        mustBeLast,
        singleActive,
        std::move(algorithmVersion),
        std::move(parameterSchemaId)};
}

[[nodiscard]] std::vector<ModuleDescriptorSpec> package_v1_specs()
{
    using enum ChainSegment;
    using enum ModuleCategory;
    using enum PlacementClass;
    using enum ProcessingStage;
    using enum TerminalSlot;

    std::vector<ModuleDescriptorSpec> specs;
    specs.reserve(11);
    specs.push_back(make_spec(
        "rgsml.dsp.dc-offset",
        {RESTORATION},
        {RESTORE_PREP},
        {REPAIR},
        true,
        INLINE_CHAIN,
        std::nullopt));
    specs.push_back(make_spec(
        "rgsml.dsp.declip",
        {RESTORATION},
        {RESTORE_PREP},
        {REPAIR},
        true,
        INLINE_CHAIN,
        std::nullopt));
    specs.push_back(make_spec(
        "rgsml.dsp.dehum",
        {RESTORATION},
        {RESTORE_PREP},
        {REPAIR},
        true,
        INLINE_CHAIN,
        std::nullopt));
    specs.push_back(make_spec(
        "rgsml.dsp.gain",
        {UTILITY},
        {RESTORE_PREP, MASTER},
        {PRE_MASTER_CONDITIONING, MANUAL, DNA_LINKED, REF_LINKED},
        true,
        INLINE_CHAIN,
        std::nullopt,
        {},
        false,
        false,
        "1.0.0",
        "rgsml.dsp.gain.parameters/1.0.0"));
    specs.push_back(make_spec(
        "rgsml.dsp.parametric-eq",
        {FILTER_EQ},
        {RESTORE_PREP, MASTER},
        {REPAIR, PRE_MASTER_CONDITIONING, MANUAL, DNA_LINKED, REF_LINKED},
        true,
        INLINE_CHAIN,
        std::nullopt,
        {},
        false,
        false,
        "1.0.0",
        "rgsml.dsp.parametric-eq.parameters/1.0.0"));
    specs.push_back(make_spec(
        "rgsml.dsp.compressor",
        {DYNAMICS},
        {MASTER},
        {MANUAL, DNA_LINKED, REF_LINKED},
        true,
        INLINE_CHAIN,
        std::nullopt));
    specs.push_back(make_spec(
        "rgsml.dsp.stereo-ms",
        {SPATIAL},
        {MASTER},
        {MANUAL, DNA_LINKED, REF_LINKED},
        true,
        INLINE_CHAIN,
        std::nullopt));
    specs.push_back(make_spec(
        "rgsml.dsp.true-peak-limiter",
        {DYNAMICS, OUTPUT},
        {MASTER},
        {TERMINAL},
        false,
        TERMINAL_SLOT,
        FINAL_TRUE_PEAK_LIMITER,
        {},
        false,
        true));
    specs.push_back(make_spec(
        "rgsml.dsp.dither",
        {OUTPUT},
        {MASTER},
        {TERMINAL},
        false,
        TERMINAL_SLOT,
        DITHER,
        {},
        true,
        true));
    specs.push_back(make_spec(
        "rgsml.dsp.dynamic-eq",
        {FILTER_EQ, DYNAMICS},
        {RESTORE_PREP, MASTER},
        {PRE_MASTER_CONDITIONING, MANUAL, DNA_LINKED, REF_LINKED},
        true,
        INLINE_CHAIN,
        std::nullopt,
        {"rgsml.dsp.true-peak-limiter"},
        false,
        false,
        "1.0.0",
        "rgsml.dsp.dynamic-eq.parameters/1.0.0"));
    specs.push_back(make_spec(
        "rgsml.dsp.transient-shaper",
        {DYNAMICS},
        {RESTORE_PREP, MASTER},
        {PRE_MASTER_CONDITIONING, MANUAL, DNA_LINKED, REF_LINKED},
        true,
        INLINE_CHAIN,
        std::nullopt,
        {"rgsml.dsp.true-peak-limiter"},
        false,
        false,
        "1.0.0",
        "rgsml.dsp.transient-shaper.parameters/1.0.0"));
    return specs;
}

[[nodiscard]] bool has_hard_cycle(
    const std::vector<ModuleRegistration>& registrations)
{
    const auto index_of = [&registrations](std::string_view typeId) {
        const auto iterator = std::lower_bound(
            registrations.begin(),
            registrations.end(),
            typeId,
            [](const ModuleRegistration& registration, std::string_view value) {
                return unsigned_ascii_less(registration.descriptor.type_id(), value);
            });
        return static_cast<std::size_t>(iterator - registrations.begin());
    };

    std::vector<std::vector<std::size_t>> edges(registrations.size());
    for (std::size_t index = 0; index < registrations.size(); ++index) {
        const auto& descriptor = registrations[index].descriptor;
        for (const auto& target : descriptor.must_precede()) {
            edges[index].push_back(index_of(target));
        }
        for (const auto& source : descriptor.must_follow()) {
            edges[index_of(source)].push_back(index);
        }
    }

    std::vector<std::uint8_t> state(registrations.size(), 0U);
    const std::function<bool(std::size_t)> visit =
        [&edges, &state, &visit](std::size_t node) {
            if (state[node] == 1U) {
                return true;
            }
            if (state[node] == 2U) {
                return false;
            }
            state[node] = 1U;
            for (const auto next : edges[node]) {
                if (visit(next)) {
                    return true;
                }
            }
            state[node] = 2U;
            return false;
        };

    for (std::size_t index = 0; index < registrations.size(); ++index) {
        if (visit(index)) {
            return true;
        }
    }
    return false;
}

}  // namespace

Result<ModuleRegistry> ModuleRegistry::create(
    std::vector<ModuleRegistration> registrations)
{
    std::ranges::sort(registrations, [](const auto& left, const auto& right) {
        return unsigned_ascii_less(
            left.descriptor.type_id(),
            right.descriptor.type_id());
    });

    for (std::size_t index = 0; index < registrations.size(); ++index) {
        const auto& registration = registrations[index];
        if (index > 0U
            && registrations[index - 1U].descriptor.type_id()
                == registration.descriptor.type_id()) {
            return Result<ModuleRegistry>::failure(registry_error(
                ErrorCode::InvalidArgument,
                "DUPLICATE_MODULE_TYPE",
                "A module type is registered more than once."));
        }
        if (registration.factory
            && registration.factory->module_type_id()
                != registration.descriptor.type_id()) {
            return Result<ModuleRegistry>::failure(registry_error(
                ErrorCode::InvalidArgument,
                "MODULE_IMPLEMENTATION_UNAVAILABLE",
                "Factory identity does not match its descriptor."));
        }
    }

    const auto type_exists = [&registrations](std::string_view typeId) {
        return std::binary_search(
            registrations.begin(),
            registrations.end(),
            typeId,
            [](const auto& left, const auto& right) {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(left)>,
                                  ModuleRegistration>) {
                    return unsigned_ascii_less(left.descriptor.type_id(), right);
                } else {
                    return unsigned_ascii_less(left, right.descriptor.type_id());
                }
            });
    };

    for (const auto& registration : registrations) {
        const auto validate_targets =
            [&type_exists](std::span<const std::string> targets) {
                return std::ranges::all_of(targets, type_exists);
            };
        const auto& descriptor = registration.descriptor;
        if (!validate_targets(descriptor.must_precede())
            || !validate_targets(descriptor.must_follow())
            || !validate_targets(descriptor.recommended_before())
            || !validate_targets(descriptor.recommended_after())) {
            return Result<ModuleRegistry>::failure(registry_error(
                ErrorCode::InvalidArgument,
                "MODULE_TYPE_NOT_FOUND",
                "A descriptor order edge references an unregistered type."));
        }
    }
    if (has_hard_cycle(registrations)) {
        return Result<ModuleRegistry>::failure(registry_error(
            ErrorCode::InvalidArgument,
            "MODULE_ORDER_VIOLATION",
            "The descriptor hard-order graph contains a cycle."));
    }

    return Result<ModuleRegistry>::success(ModuleRegistry{std::move(registrations)});
}

Result<ModuleRegistry> ModuleRegistry::create_dsp_package_v1()
{
    std::vector<ModuleRegistration> registrations;
    registrations.reserve(11);
    for (auto& spec : package_v1_specs()) {
        auto descriptor = ModuleDescriptor::create(std::move(spec));
        if (!descriptor) {
            return Result<ModuleRegistry>::failure(*descriptor.error());
        }
        auto canonical_descriptor = std::move(*descriptor.value());
        std::shared_ptr<const IModuleFactory> factory;
        if (canonical_descriptor.type_id() == kGainTypeId) {
            factory = std::make_shared<GainFactory>(canonical_descriptor);
        } else if (canonical_descriptor.type_id() == kEqTypeId) {
            factory = std::make_shared<ParametricEqFactory>(canonical_descriptor);
        }
        registrations.push_back(ModuleRegistration{
            std::move(canonical_descriptor),
            std::move(factory)});
    }
    return create(std::move(registrations));
}

ModuleRegistry::ModuleRegistry(std::vector<ModuleRegistration> registrations)
    : registrations_(std::move(registrations))
{
    descriptors_.reserve(registrations_.size());
    for (const auto& registration : registrations_) {
        descriptors_.push_back(registration.descriptor);
    }
}

std::span<const ModuleDescriptor> ModuleRegistry::descriptors() const noexcept
{
    return descriptors_;
}

Result<std::reference_wrapper<const ModuleDescriptor>>
ModuleRegistry::find_descriptor(std::string_view type_id) const
{
    const auto iterator = std::lower_bound(
        descriptors_.begin(),
        descriptors_.end(),
        type_id,
        [](const ModuleDescriptor& descriptor, std::string_view value) {
            return unsigned_ascii_less(descriptor.type_id(), value);
        });
    if (iterator == descriptors_.end() || iterator->type_id() != type_id) {
        return Result<std::reference_wrapper<const ModuleDescriptor>>::failure(
            registry_error(
                ErrorCode::ResourceNotFound,
                "MODULE_TYPE_NOT_FOUND",
                "The requested module type is not registered."));
    }
    return Result<std::reference_wrapper<const ModuleDescriptor>>::success(
        std::cref(*iterator));
}

bool ModuleRegistry::has_factory(std::string_view type_id) const noexcept
{
    const auto iterator = std::lower_bound(
        registrations_.begin(),
        registrations_.end(),
        type_id,
        [](const ModuleRegistration& registration, std::string_view value) {
            return unsigned_ascii_less(registration.descriptor.type_id(), value);
        });
    return iterator != registrations_.end()
        && iterator->descriptor.type_id() == type_id
        && static_cast<bool>(iterator->factory);
}

std::size_t ModuleRegistry::factory_count() const noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(
        registrations_,
        [](const ModuleRegistration& registration) {
            return static_cast<bool>(registration.factory);
        }));
}

Result<std::unique_ptr<IModule>>
ModuleRegistry::create_module(std::string_view type_id) const
{
    const auto iterator = std::lower_bound(
        registrations_.begin(),
        registrations_.end(),
        type_id,
        [](const ModuleRegistration& registration, std::string_view value) {
            return unsigned_ascii_less(registration.descriptor.type_id(), value);
        });
    if (iterator == registrations_.end() || iterator->descriptor.type_id() != type_id) {
        return Result<std::unique_ptr<IModule>>::failure(registry_error(
            ErrorCode::ResourceNotFound,
            "MODULE_TYPE_NOT_FOUND",
            "The requested module type is not registered."));
    }
    if (!iterator->factory) {
        return Result<std::unique_ptr<IModule>>::failure(registry_error(
            ErrorCode::UnsupportedOperation,
            "MODULE_IMPLEMENTATION_UNAVAILABLE",
            "No production implementation is registered for this descriptor."));
    }

    try {
        auto module = iterator->factory->create();
        if (!module) {
            return Result<std::unique_ptr<IModule>>::failure(*module.error());
        }
        if (!*module.value()
            || (*module.value())->descriptor().type_id() != type_id) {
            return Result<std::unique_ptr<IModule>>::failure(registry_error(
                ErrorCode::InvalidState,
                "MODULE_IMPLEMENTATION_UNAVAILABLE",
                "Factory produced a module with a mismatched descriptor."));
        }
        return module;
    } catch (...) {
        return Result<std::unique_ptr<IModule>>::failure(registry_error(
            ErrorCode::InvalidState,
            "MODULE_IMPLEMENTATION_UNAVAILABLE",
            "Module factory threw across its public boundary."));
    }
}

Result<std::unique_ptr<IModule>>
ModuleRegistry::create_module(
    std::string_view type_id,
    const ModuleParameterPayload& payload) const
{
    const auto iterator = std::lower_bound(
        registrations_.begin(),
        registrations_.end(),
        type_id,
        [](const ModuleRegistration& registration, std::string_view value) {
            return unsigned_ascii_less(registration.descriptor.type_id(), value);
        });
    if (iterator == registrations_.end() || iterator->descriptor.type_id() != type_id) {
        return Result<std::unique_ptr<IModule>>::failure(registry_error(
            ErrorCode::ResourceNotFound,
            "MODULE_TYPE_NOT_FOUND",
            "The requested module type is not registered."));
    }
    if (!iterator->factory) {
        return Result<std::unique_ptr<IModule>>::failure(registry_error(
            ErrorCode::UnsupportedOperation,
            "MODULE_IMPLEMENTATION_UNAVAILABLE",
            "No production implementation is registered for this descriptor."));
    }

    try {
        if (type_id == kGainTypeId) {
            const auto* gain_params = std::get_if<GainParameters>(&payload);
            if (gain_params == nullptr) {
                return Result<std::unique_ptr<IModule>>::failure(registry_error(
                    ErrorCode::InvalidArgument,
                    "MODULE_PARAMETER_PAYLOAD_MISMATCH",
                    "Gain module requires GainParameters payload."));
            }
            auto module = GainModule::create(iterator->descriptor, *gain_params);
            if (!module) {
                return Result<std::unique_ptr<IModule>>::failure(*module.error());
            }
            std::unique_ptr<IModule> result = std::move(*module.value());
            return Result<std::unique_ptr<IModule>>::success(std::move(result));
        }

        if (type_id == kEqTypeId) {
            const auto* eq_params = std::get_if<ParametricEqParameters>(&payload);
            if (eq_params == nullptr) {
                return Result<std::unique_ptr<IModule>>::failure(registry_error(
                    ErrorCode::InvalidArgument,
                    "MODULE_PARAMETER_PAYLOAD_MISMATCH",
                    "Parametric EQ module requires ParametricEqParameters payload."));
            }
            auto module = ParametricEqModule::create(iterator->descriptor, *eq_params);
            if (!module) {
                return Result<std::unique_ptr<IModule>>::failure(*module.error());
            }
            std::unique_ptr<IModule> result = std::move(*module.value());
            return Result<std::unique_ptr<IModule>>::success(std::move(result));
        }

        return Result<std::unique_ptr<IModule>>::failure(registry_error(
            ErrorCode::InvalidArgument,
            "MODULE_PARAMETER_PAYLOAD_MISMATCH",
            "The specified module type is not parameterized or unsupported."));
    } catch (...) {
        return Result<std::unique_ptr<IModule>>::failure(registry_error(
            ErrorCode::InvalidState,
            "MODULE_IMPLEMENTATION_UNAVAILABLE",
            "Module creation threw across its public boundary."));
    }
}

}  // namespace rgsml::dsp
