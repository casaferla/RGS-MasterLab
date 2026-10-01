#include <rgsml/dsp/processing_chain.hpp>

#include "internal/revision.hpp"

#include <rgsml/core/error.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace rgsml::dsp {
namespace {

using rgsml::core::Error;
using rgsml::core::ErrorCode;
using rgsml::core::Result;
using rgsml::core::Status;

[[nodiscard]] Error dsp_error(
    ErrorCode code,
    std::string category,
    std::string message)
{
    return Error{code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] bool is_type_id(std::string_view value) noexcept
{
    constexpr std::string_view prefix = "rgsml.dsp.";
    if (!value.starts_with(prefix) || value.size() == prefix.size()) {
        return false;
    }
    bool segmentHasCharacter = false;
    for (const char character : value.substr(prefix.size())) {
        const bool lower = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';
        if (lower || digit) {
            segmentHasCharacter = true;
        } else if (character == '-' && segmentHasCharacter) {
            segmentHasCharacter = false;
        } else {
            return false;
        }
    }
    return segmentHasCharacter;
}

template <typename T>
[[nodiscard]] bool contains(std::span<const T> values, T value) noexcept
{
    return std::ranges::find(values, value) != values.end();
}

[[nodiscard]] bool valid_context(ProcessingChainContext context) noexcept
{
    const bool validStage = context.stage == ProcessingStage::RESTORE_PREP
        || context.stage == ProcessingStage::MASTER;
    const bool validSegment =
        static_cast<std::uint8_t>(context.segment)
        <= static_cast<std::uint8_t>(ChainSegment::TERMINAL);
    if (!validStage || !validSegment) {
        return false;
    }
    const bool restoreSegment = context.segment == ChainSegment::REPAIR
        || context.segment == ChainSegment::PRE_MASTER_CONDITIONING;
    return restoreSegment
        ? context.stage == ProcessingStage::RESTORE_PREP
        : context.stage == ProcessingStage::MASTER;
}

[[nodiscard]] std::optional<std::size_t> find_index(
    std::span<const ModuleInstance> instances,
    const ModuleInstanceId& id) noexcept
{
    const auto iterator = std::ranges::find(
        instances,
        id,
        &ModuleInstance::instance_id);
    if (iterator == instances.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(iterator - instances.begin());
}

}  // namespace

Result<std::uint64_t> internal::next_revision(std::uint64_t current)
{
    if (current == std::numeric_limits<std::uint64_t>::max()) {
        return Result<std::uint64_t>::failure(dsp_error(
            ErrorCode::IntegerOverflow,
            "CHAIN_REVISION_OVERFLOW",
            "Processing-chain revision cannot wrap."));
    }
    return Result<std::uint64_t>::success(current + 1U);
}

Result<ModuleInstance> ModuleInstance::create_manual(
    ModuleInstanceId instance_id,
    std::string module_type_id)
{
    return create(ModuleInstanceSpec{
        std::move(instance_id),
        std::move(module_type_id),
        true,
        false,
        false,
        false,
        ModuleProvenance::MANUAL,
        ModuleOwner::USER,
        ModuleLinkState::UNLINKED,
        std::nullopt,
        ModuleParameterState{}});
}

Result<ModuleInstance> ModuleInstance::create(ModuleInstanceSpec spec)
{
    if (!is_type_id(spec.module_type_id)) {
        return Result<ModuleInstance>::failure(dsp_error(
            ErrorCode::InvalidArgument,
            "MODULE_TYPE_NOT_FOUND",
            "Module instance type ID is not canonical."));
    }
    if (spec.provenance != ModuleProvenance::MANUAL
        || spec.owner != ModuleOwner::USER) {
        return Result<ModuleInstance>::failure(dsp_error(
            ErrorCode::InvalidArgument,
            "MODULE_OWNERSHIP_INVALID",
            "Task 010 instances must have MANUAL/USER ownership."));
    }
    if (spec.link_state != ModuleLinkState::UNLINKED
        || spec.semantic_node_id.has_value()) {
        return Result<ModuleInstance>::failure(dsp_error(
            ErrorCode::InvalidArgument,
            "MODULE_LINK_STATE_INVALID",
            "Task 010 instances must be UNLINKED without a semantic node."));
    }
    return Result<ModuleInstance>::success(ModuleInstance{std::move(spec)});
}

ModuleInstance::ModuleInstance(ModuleInstanceSpec spec)
    : spec_(std::move(spec))
{
}

const ModuleInstanceId& ModuleInstance::instance_id() const noexcept
{
    return spec_.instance_id;
}
std::string_view ModuleInstance::module_type_id() const noexcept
{
    return spec_.module_type_id;
}
bool ModuleInstance::enabled() const noexcept { return spec_.enabled; }
bool ModuleInstance::user_bypass() const noexcept { return spec_.user_bypass; }
bool ModuleInstance::controller_suspended() const noexcept
{
    return spec_.controller_suspended;
}
bool ModuleInstance::domain_suspended() const noexcept
{
    return spec_.domain_suspended;
}
bool ModuleInstance::active() const noexcept
{
    return spec_.enabled && !spec_.user_bypass
        && !spec_.controller_suspended && !spec_.domain_suspended;
}
ModuleProvenance ModuleInstance::provenance() const noexcept
{
    return spec_.provenance;
}
ModuleOwner ModuleInstance::owner() const noexcept { return spec_.owner; }
ModuleLinkState ModuleInstance::link_state() const noexcept
{
    return spec_.link_state;
}
std::optional<rgsml::core::Uuid> ModuleInstance::semantic_node_id() const noexcept
{
    return spec_.semantic_node_id;
}
const ModuleParameterState& ModuleInstance::parameter_state() const noexcept
{
    return spec_.parameter_state;
}
void ModuleInstance::set_enabled(bool enabled) noexcept { spec_.enabled = enabled; }
void ModuleInstance::set_user_bypass(bool bypassed) noexcept
{
    spec_.user_bypass = bypassed;
}
void ModuleInstance::set_controller_suspended(bool suspended) noexcept
{
    spec_.controller_suspended = suspended;
}
void ModuleInstance::set_domain_suspended(bool suspended) noexcept
{
    spec_.domain_suspended = suspended;
}
ModuleInstance ModuleInstance::duplicate_with_id(ModuleInstanceId new_id) const
{
    auto duplicate = *this;
    duplicate.spec_.instance_id = std::move(new_id);
    return duplicate;
}

Result<ProcessingChain> ProcessingChain::create(
    const ModuleRegistry& registry,
    ProcessingChainContext context)
{
    if (!valid_context(context)) {
        return Result<ProcessingChain>::failure(dsp_error(
            ErrorCode::InvalidArgument,
            "MODULE_STAGE_NOT_ALLOWED",
            "Processing-chain stage and segment are inconsistent."));
    }
    return Result<ProcessingChain>::success(ProcessingChain{registry, context});
}

Result<ProcessingChain> ProcessingChain::restore(
    const ModuleRegistry& registry,
    ProcessingChainContext context,
    std::uint64_t revision,
    std::vector<ModuleInstance> instances)
{
    if (!valid_context(context)) {
        return Result<ProcessingChain>::failure(dsp_error(
            ErrorCode::InvalidArgument,
            "MODULE_STAGE_NOT_ALLOWED",
            "Processing-chain stage and segment are inconsistent."));
    }
    ProcessingChain chain{registry, context};
    const auto valid = chain.validate(instances);
    if (!valid) {
        return Result<ProcessingChain>::failure(*valid.error());
    }
    chain.revision_ = revision;
    chain.instances_ = std::move(instances);
    return Result<ProcessingChain>::success(std::move(chain));
}

ProcessingChain::ProcessingChain(
    const ModuleRegistry& registry,
    ProcessingChainContext context) noexcept
    : registry_(&registry)
    , context_(context)
{
}

ProcessingChainContext ProcessingChain::context() const noexcept { return context_; }
std::uint64_t ProcessingChain::revision() const noexcept { return revision_; }
std::span<const ModuleInstance> ProcessingChain::instances() const noexcept
{
    return instances_;
}

Result<std::reference_wrapper<const ModuleInstance>>
ProcessingChain::find_instance(const ModuleInstanceId& instance_id) const
{
    const auto index = find_index(instances_, instance_id);
    if (!index.has_value()) {
        return Result<std::reference_wrapper<const ModuleInstance>>::failure(
            dsp_error(
                ErrorCode::ResourceNotFound,
                "MODULE_INSTANCE_NOT_FOUND",
                "The requested module instance is absent."));
    }
    return Result<std::reference_wrapper<const ModuleInstance>>::success(
        std::cref(instances_[*index]));
}

Status ProcessingChain::validate(std::span<const ModuleInstance> candidate) const
{
    for (std::size_t left = 0; left < candidate.size(); ++left) {
        for (std::size_t right = left + 1U; right < candidate.size(); ++right) {
            if (candidate[left].instance_id() == candidate[right].instance_id()) {
                return Status::failure(dsp_error(
                    ErrorCode::InvalidArgument,
                    "DUPLICATE_MODULE_INSTANCE_ID",
                    "Module instance IDs must be unique."));
            }
        }
    }

    for (std::size_t index = 0; index < candidate.size(); ++index) {
        const auto& instance = candidate[index];
        const auto descriptorResult = registry_->find_descriptor(instance.module_type_id());
        if (!descriptorResult) {
            return Status::failure(*descriptorResult.error());
        }
        const auto& descriptor = descriptorResult.value()->get();
        if (!contains<ProcessingStage>(descriptor.allowed_stages(), context_.stage)) {
            return Status::failure(dsp_error(
                ErrorCode::InvalidArgument,
                "MODULE_STAGE_NOT_ALLOWED",
                "Module is not permitted in this processing stage."));
        }
        if (!contains<ChainSegment>(descriptor.allowed_segments(), context_.segment)) {
            return Status::failure(dsp_error(
                ErrorCode::InvalidArgument,
                "MODULE_SEGMENT_NOT_ALLOWED",
                "Module is not permitted in this chain segment."));
        }
        const bool terminalContext = context_.segment == ChainSegment::TERMINAL;
        const bool terminalModule =
            descriptor.placement_class() == PlacementClass::TERMINAL_SLOT;
        if (terminalContext != terminalModule) {
            return Status::failure(dsp_error(
                ErrorCode::InvalidArgument,
                "MODULE_PLACEMENT_INVALID",
                "Module placement class does not match the chain segment."));
        }
        if (descriptor.must_be_last() && index + 1U != candidate.size()) {
            return Status::failure(dsp_error(
                ErrorCode::InvalidArgument,
                "MODULE_ORDER_VIOLATION",
                "A must-be-last module is followed by another module."));
        }
    }

    for (const auto& descriptor : registry_->descriptors()) {
        std::vector<std::size_t> positions;
        std::size_t activeCount = 0U;
        for (std::size_t index = 0; index < candidate.size(); ++index) {
            if (candidate[index].module_type_id() == descriptor.type_id()) {
                positions.push_back(index);
                activeCount += candidate[index].active() ? 1U : 0U;
            }
        }
        if (!descriptor.duplicable() && positions.size() > 1U) {
            return Status::failure(dsp_error(
                ErrorCode::InvalidArgument,
                "MODULE_NOT_DUPLICABLE",
                "A non-duplicable module type appears more than once."));
        }
        if (descriptor.single_active_instance() && activeCount > 1U) {
            return Status::failure(dsp_error(
                ErrorCode::InvalidArgument,
                "MODULE_SINGLE_ACTIVE_VIOLATION",
                "A single-active module type has multiple active instances."));
        }
        if (positions.empty()) {
            continue;
        }

        const auto enforce_before =
            [&candidate, &positions](std::span<const std::string> targets) {
                for (const auto& target : targets) {
                    std::optional<std::size_t> firstTarget;
                    for (std::size_t index = 0; index < candidate.size(); ++index) {
                        if (candidate[index].module_type_id() == target) {
                            firstTarget = index;
                            break;
                        }
                    }
                    if (firstTarget.has_value() && positions.back() >= *firstTarget) {
                        return false;
                    }
                }
                return true;
            };
        const auto enforce_after =
            [&candidate, &positions](std::span<const std::string> sources) {
                for (const auto& source : sources) {
                    std::optional<std::size_t> lastSource;
                    for (std::size_t index = 0; index < candidate.size(); ++index) {
                        if (candidate[index].module_type_id() == source) {
                            lastSource = index;
                        }
                    }
                    if (lastSource.has_value() && positions.front() <= *lastSource) {
                        return false;
                    }
                }
                return true;
            };
        if (!enforce_before(descriptor.must_precede())
            || !enforce_after(descriptor.must_follow())) {
            return Status::failure(dsp_error(
                ErrorCode::InvalidArgument,
                "MODULE_ORDER_VIOLATION",
                "A module hard-order constraint is violated."));
        }
    }

    if (context_.segment == ChainSegment::TERMINAL) {
        std::optional<TerminalSlot> previous;
        for (const auto& instance : candidate) {
            const auto descriptor = registry_->find_descriptor(instance.module_type_id());
            if (!descriptor || !descriptor.value()->get().terminal_slot().has_value()) {
                return Status::failure(dsp_error(
                    ErrorCode::InvalidArgument,
                    "MODULE_PLACEMENT_INVALID",
                    "Terminal chain contains a module without a terminal slot."));
            }
            const auto slot = *descriptor.value()->get().terminal_slot();
            if (previous.has_value()
                && static_cast<std::uint8_t>(slot)
                    <= static_cast<std::uint8_t>(*previous)) {
                return Status::failure(dsp_error(
                    ErrorCode::InvalidArgument,
                    "MODULE_ORDER_VIOLATION",
                    "Terminal modules do not follow the frozen slot order."));
            }
            previous = slot;
        }
    }
    return Status::success();
}

Status ProcessingChain::commit(std::vector<ModuleInstance> candidate)
{
    auto valid = validate(candidate);
    if (!valid) {
        return valid;
    }
    auto next = internal::next_revision(revision_);
    if (!next) {
        return Status::failure(*next.error());
    }
    instances_ = std::move(candidate);
    revision_ = *next.value();
    return Status::success();
}

Status ProcessingChain::add(
    ModuleInstanceId instance_id,
    std::string_view module_type_id,
    std::size_t final_index)
{
    if (final_index > instances_.size()) {
        return Status::failure(dsp_error(
            ErrorCode::OutOfRange,
            "MODULE_PLACEMENT_INVALID",
            "Add position is outside the final chain."));
    }
    if (find_index(instances_, instance_id).has_value()) {
        return Status::failure(dsp_error(
            ErrorCode::InvalidArgument,
            "DUPLICATE_MODULE_INSTANCE_ID",
            "Module instance ID is already present."));
    }
    auto descriptor = registry_->find_descriptor(module_type_id);
    if (!descriptor) {
        return Status::failure(*descriptor.error());
    }
    auto instance = ModuleInstance::create_manual(
        std::move(instance_id),
        std::string(module_type_id));
    if (!instance) {
        return Status::failure(*instance.error());
    }
    auto candidate = instances_;
    candidate.insert(
        candidate.begin() + static_cast<std::ptrdiff_t>(final_index),
        std::move(*instance.value()));
    return commit(std::move(candidate));
}

Status ProcessingChain::remove(const ModuleInstanceId& instance_id)
{
    const auto index = find_index(instances_, instance_id);
    if (!index.has_value()) {
        return Status::failure(dsp_error(
            ErrorCode::ResourceNotFound,
            "MODULE_INSTANCE_NOT_FOUND",
            "Cannot remove an absent module instance."));
    }
    auto candidate = instances_;
    candidate.erase(candidate.begin() + static_cast<std::ptrdiff_t>(*index));
    return commit(std::move(candidate));
}

Status ProcessingChain::move(
    const ModuleInstanceId& instance_id,
    std::size_t final_index)
{
    const auto sourceIndex = find_index(instances_, instance_id);
    if (!sourceIndex.has_value()) {
        return Status::failure(dsp_error(
            ErrorCode::ResourceNotFound,
            "MODULE_INSTANCE_NOT_FOUND",
            "Cannot move an absent module instance."));
    }
    if (final_index >= instances_.size()) {
        return Status::failure(dsp_error(
            ErrorCode::OutOfRange,
            "MODULE_PLACEMENT_INVALID",
            "Move final index is outside the chain."));
    }
    if (*sourceIndex == final_index) {
        return Status::success();
    }
    auto candidate = instances_;
    auto moved = std::move(candidate[*sourceIndex]);
    candidate.erase(candidate.begin() + static_cast<std::ptrdiff_t>(*sourceIndex));
    candidate.insert(
        candidate.begin() + static_cast<std::ptrdiff_t>(final_index),
        std::move(moved));
    return commit(std::move(candidate));
}

Status ProcessingChain::duplicate(
    const ModuleInstanceId& source_id,
    ModuleInstanceId new_id,
    std::size_t final_index)
{
    const auto sourceIndex = find_index(instances_, source_id);
    if (!sourceIndex.has_value()) {
        return Status::failure(dsp_error(
            ErrorCode::ResourceNotFound,
            "MODULE_INSTANCE_NOT_FOUND",
            "Cannot duplicate an absent module instance."));
    }
    if (find_index(instances_, new_id).has_value()) {
        return Status::failure(dsp_error(
            ErrorCode::InvalidArgument,
            "DUPLICATE_MODULE_INSTANCE_ID",
            "Duplicate requires a fresh caller-supplied ID."));
    }
    if (final_index > instances_.size()) {
        return Status::failure(dsp_error(
            ErrorCode::OutOfRange,
            "MODULE_PLACEMENT_INVALID",
            "Duplicate position is outside the final chain."));
    }
    const auto descriptor =
        registry_->find_descriptor(instances_[*sourceIndex].module_type_id());
    if (!descriptor) {
        return Status::failure(*descriptor.error());
    }
    if (!descriptor.value()->get().duplicable()) {
        return Status::failure(dsp_error(
            ErrorCode::InvalidArgument,
            "MODULE_NOT_DUPLICABLE",
            "The source module type is not duplicable."));
    }
    auto candidate = instances_;
    auto duplicate = candidate[*sourceIndex].duplicate_with_id(std::move(new_id));
    candidate.insert(
        candidate.begin() + static_cast<std::ptrdiff_t>(final_index),
        std::move(duplicate));
    return commit(std::move(candidate));
}

Status ProcessingChain::set_user_bypass(
    const ModuleInstanceId& instance_id,
    bool bypassed)
{
    const auto index = find_index(instances_, instance_id);
    if (!index.has_value()) {
        return Status::failure(dsp_error(
            ErrorCode::ResourceNotFound,
            "MODULE_INSTANCE_NOT_FOUND",
            "Cannot bypass an absent module instance."));
    }
    const auto descriptor =
        registry_->find_descriptor(instances_[*index].module_type_id());
    if (!descriptor) {
        return Status::failure(*descriptor.error());
    }
    if (!descriptor.value()->get().bypassable()) {
        return Status::failure(dsp_error(
            ErrorCode::InvalidArgument,
            "MODULE_NOT_BYPASSABLE",
            "The module type is not bypassable."));
    }
    if (instances_[*index].user_bypass() == bypassed) {
        return Status::success();
    }
    auto candidate = instances_;
    candidate[*index].set_user_bypass(bypassed);
    return commit(std::move(candidate));
}

}  // namespace rgsml::dsp
