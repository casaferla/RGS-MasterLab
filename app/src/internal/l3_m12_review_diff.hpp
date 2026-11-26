#pragma once
#include "l3_m12_project_transaction_draft.hpp"
namespace rgsml::app::l3_automatch::m12_frozen {
enum class ReviewNodeChange {add,remove,update,retain,suspend,resume};
struct ReviewNode final {
    core::Uuid instance;
    std::string type;
    ReviewNodeChange change;
};
struct M12ReviewDiff final {
    std::vector<ReviewNode> nodes;
    [[nodiscard]] constexpr bool authorizes_commit()const noexcept{return false;}
};
[[nodiscard]] inline M12ReviewDiff describe_exact_m12_review_diff(
    const std::optional<project::Chain>& before,const std::optional<project::Chain>& after)
{
    M12ReviewDiff diff;
    if(before)for(const auto& node:before->modules) {
        const bool retained=after&&std::any_of(after->modules.begin(),after->modules.end(),
            [&](const auto& n){return n.instanceId==node.instanceId;});
        if(!retained)diff.nodes.push_back({node.instanceId,node.typeId,ReviewNodeChange::remove});
    }
    if(after)for(const auto& node:after->modules) {
        const project::Module* old=nullptr;
        if(before) {
            const auto found=std::find_if(before->modules.begin(),before->modules.end(),
                [&](const auto& n){return n.instanceId==node.instanceId;});
            if(found!=before->modules.end())old=&*found;
        }
        auto change=ReviewNodeChange::add;
        if(old) {
            auto normalized=node;
            normalized.controllerSuspended=old->controllerSuspended;
            if(exact_project_module_equal(*old,node))change=ReviewNodeChange::retain;
            else if(exact_project_module_equal(*old,normalized))
                change=node.controllerSuspended?ReviewNodeChange::suspend:ReviewNodeChange::resume;
            else change=ReviewNodeChange::update;
        }
        diff.nodes.push_back({node.instanceId,node.typeId,change});
    }
    return diff;
}
} // namespace rgsml::app::l3_automatch::m12_frozen
