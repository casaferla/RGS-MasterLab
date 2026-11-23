#pragma once
// L3-M12: receipt issued ONLY by the Qt Product owner after its genuine
// exclusive model publication and independent, exact model readback.
// Neither a preflight ticket nor a persisted JSON "MATERIALIZED" string
// can construct a successful receipt. M13 verification is separate.
#include <rgsml/core/uuid.hpp>
#include <cstdint>
#include <string>
#include <utility>

namespace rgsml::app {
class ProjectSessionViewModel;
namespace l3_automatch::m12_frozen {

class M12ProductMaterializationReceipt final {
public:
    M12ProductMaterializationReceipt() = delete;
    [[nodiscard]] const core::Uuid& project_id() const noexcept
    {return projectId_;}
    [[nodiscard]] const core::Uuid& source_id() const noexcept
    {return sourceId_;}
    [[nodiscard]] const core::Uuid& reference_id() const noexcept
    {return referenceId_;}
    [[nodiscard]] const core::Uuid& plan_id() const noexcept
    {return planId_;}
    [[nodiscard]] const core::Uuid& controller_id() const noexcept
    {return controllerId_;}
    [[nodiscard]] const core::Uuid& linked_chain_id() const noexcept
    {return linkedChainId_;}
    [[nodiscard]] const std::string& plan_checksum() const noexcept
    {return planChecksum_;}
    [[nodiscard]] bool materialized_not_verified() const noexcept
    {return true;}
    [[nodiscard]] bool authorizes_m13_verified() const noexcept
    {return false;}
    [[nodiscard]] bool authorizes_wav_export() const noexcept
    {return false;}

private:
    friend class ::rgsml::app::ProjectSessionViewModel;
    M12ProductMaterializationReceipt(
        core::Uuid project,core::Uuid source,core::Uuid reference,
        core::Uuid plan,core::Uuid controller,core::Uuid linked,
        std::string checksum)
        :projectId_(project),sourceId_(source),referenceId_(reference),
         planId_(plan),controllerId_(controller),linkedChainId_(linked),
         planChecksum_(std::move(checksum)) {}
    core::Uuid projectId_;
    core::Uuid sourceId_;
    core::Uuid referenceId_;
    core::Uuid planId_;
    core::Uuid controllerId_;
    core::Uuid linkedChainId_;
    std::string planChecksum_;
};
} // namespace l3_automatch::m12_frozen
} // namespace rgsml::app
