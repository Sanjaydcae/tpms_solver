#pragma once

#include <imgui.h>
#include "theme.hpp"
#include "../state/project_state.hpp"

namespace tpms::ui {

inline void draw_tree_panel(const ProjectState& state) {
    ImGui::SetNextWindowSize({260, -1}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Model Tree")) { ImGui::End(); return; }

    // ── Helper lambdas ────────────────────────────────────────────────────────
    auto leaf = [](const char* label, bool exists) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf
                                 | ImGuiTreeNodeFlags_NoTreePushOnOpen
                                 | ImGuiTreeNodeFlags_SpanFullWidth;
        if (exists)
            ImGui::PushStyleColor(ImGuiCol_Text, {0.16f, 0.22f, 0.30f, 1.f});
        else
            ImGui::PushStyleColor(ImGuiCol_Text, {0.46f, 0.53f, 0.61f, 1.f});
        ImGui::TreeNodeEx(label, flags);
        ImGui::PopStyleColor();
    };

    auto branch = [](const char* label, bool open_by_default = true) -> bool {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth
                                 | ImGuiTreeNodeFlags_DefaultOpen * open_by_default;
        return ImGui::TreeNodeEx(label, flags);
    };

    // ── Tree ──────────────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, col_accent());
    ImGui::TextUnformatted("Project");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("— %s", state.project_name.c_str());
    ImGui::Separator();

    // Geometry
    if (branch("Geometry")) {
        leaf("  TPMS Body",  state.has_geometry);
        leaf("  Domain Box", state.has_geometry);
        leaf("  Parameters", true);
        ImGui::TreePop();
    }

    // Mesh
    if (branch("Mesh")) {
        leaf("  Surface Mesh", state.has_surface_mesh);
        leaf("  Volume Mesh",  state.has_volume_mesh);
        ImGui::TreePop();
    }

    // Materials
    if (branch("Materials")) {
        leaf(("  " + state.material.material_name).c_str(), state.has_material);
        ImGui::TreePop();
    }

    // Boundary Conditions
    if (branch("Boundary Conditions")) {
        for (auto& fs : state.fixed_supports)
            leaf(("  Fixed: " + fs.face_label).c_str(), true);
        for (auto& fl : state.force_loads)
            leaf(("  Force: " + fl.face_label).c_str(), true);
        for (auto& dl : state.displacement_loads)
            leaf(("  Displacement: " + dl.face_label).c_str(), true);
        if (state.fixed_supports.empty() && state.force_loads.empty() && state.displacement_loads.empty())
            leaf("  (none)", false);
        ImGui::TreePop();
    }

    // Solve
    if (branch("Solve")) {
        leaf(("  " + std::string(state.analysis_type_name())).c_str(), true);
        leaf(state.solver_has_run
             ? (state.solver_converged ? "  Solver: Converged" : "  Solver: Approximate")
             : "  Solver: Not run",
             state.solver_has_run);
        ImGui::TreePop();
    }

    // Results
    if (branch("Results", false)) {
        if (state.has_results) {
            leaf("  Total Displacement", !state.displacement_result_scalars.empty());
            leaf("  X Displacement", !state.displacement_x_result_scalars.empty());
            leaf("  Y Displacement", !state.displacement_y_result_scalars.empty());
            leaf("  Z Displacement", !state.displacement_z_result_scalars.empty());
            leaf("  Von Mises Stress", !state.von_mises_result_scalars.empty());
            leaf("  Equivalent Strain", !state.strain_result_scalars.empty());
            leaf("  Reaction Force", !state.reaction_force_result_scalars.empty());
        } else {
            leaf("  (no results)", false);
        }
        ImGui::TreePop();
    }

    ImGui::End();
}

} // namespace tpms::ui
