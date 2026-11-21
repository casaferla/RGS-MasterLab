#pragma once
// M18 PRIVATE: ProjectSession binding deliberately kept out of portable
// chain/Qt source tests; Project headers are not part of those link targets.
// This code remains unregistered in main.cpp, no Export QML activation.
#include "project_session_view_model.hpp"
#include "../../../platform/windows/src/internal/m18_owner_mutation_fanout.hpp"

namespace rgsml::app::internal {

struct M18ProductProjectSaveBinding final {
    static void bind_project_open_and_save(
        ProjectSessionViewModel& project,
        const platform::windows::internal::M18OwnerMutationFanout& fanout)
    {
        // Both Project transitions receive the SAME Product epoch state.
        // Open permits only enumerated nested controller mutations while
        // its exclusive physical mutation lease remains held.
        project.set_private_m18_project_open_batch_begin(
            fanout.project_open_batch());
        project.set_private_m18_project_save_batch_begin(
            fanout.project_save_batch());
    }

    static void bind_project_save_only(
        ProjectSessionViewModel& project,
        const platform::windows::internal::M18OwnerMutationFanout& fanout)
    {
        // SaveAs has one physical exclusive mutation epoch from snapshot
        // through file readback. Open Project is deliberately fail-closed
        // until a nested Product-wide Project batch is independently wired.
        project.set_private_m18_project_save_batch_begin(
            fanout.project_save_batch());
    }
};

} // namespace rgsml::app::internal
