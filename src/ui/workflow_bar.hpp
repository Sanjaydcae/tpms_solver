#pragma once

#include <imgui.h>

#include "ribbon.hpp"
#include "theme.hpp"
#include "../state/project_state.hpp"

namespace tpms::ui {

inline RibbonAction draw_workflow_bar(ProjectState& state) {
    RibbonAction action;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    constexpr float h = 46.f;
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize({vp->WorkSize.x, h});

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoNav;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.955f, 0.975f, 0.992f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Border, {0.70f, 0.78f, 0.86f, 1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12, 7});
    ImGui::Begin("##commercial_workflow_bar", nullptr, flags);

    ImGui::PushStyleColor(ImGuiCol_Text, {0.08f, 0.18f, 0.30f, 1.f});
    ImGui::TextUnformatted("TPMS Studio");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("| H2one Cleantech");
    ImGui::SameLine();
    ImGui::TextDisabled("| %s%s", state.project_name.c_str(), state.dirty ? " *" : "");

    ImGui::SameLine(360.f);
    auto tab_button = [&](const char* label, WorkflowTab tab) {
        const bool active = state.active_tab == tab;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, {0.13f, 0.38f, 0.66f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.13f, 0.38f, 0.66f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_Text, {1.f, 1.f, 1.f, 1.f});
        }
        if (ImGui::Button(label, {96.f, 30.f})) state.active_tab = tab;
        if (active) ImGui::PopStyleColor(3);
    };

    tab_button("Geometry", WorkflowTab::Geometry);
    ImGui::SameLine(0, 4);
    tab_button("Meshing", WorkflowTab::Meshing);
    ImGui::SameLine(0, 4);
    tab_button("BCs", WorkflowTab::BCs);
    ImGui::SameLine(0, 4);
    tab_button("Solve", WorkflowTab::Solve);
    ImGui::SameLine(0, 4);
    tab_button("Results", WorkflowTab::Results);

    const float right_x = ImGui::GetWindowContentRegionMax().x - 310.f;
    if (ImGui::GetCursorPosX() < right_x) ImGui::SameLine(right_x);
    if (ImGui::Button("Generate", {92.f, 30.f})) action.type = RibbonAction::GenerateGeometry;
    ImGui::SameLine(0, 5);
    if (ImGui::Button("Validate", {86.f, 30.f}) && state.can_validate()) action.type = RibbonAction::Validate;
    ImGui::SameLine(0, 5);
    ImGui::PushStyleColor(ImGuiCol_Button, state.can_solve() ? col_success() : col_disabled());
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.62f, 0.38f, 1.f});
    if (ImGui::Button("Solve", {76.f, 30.f}) && state.can_solve()) action.type = RibbonAction::Solve;
    ImGui::PopStyleColor(2);

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    return action;
}

} // namespace tpms::ui
