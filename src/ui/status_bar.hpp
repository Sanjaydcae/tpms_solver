#pragma once

#include <imgui.h>
#include <cstdio>
#include "theme.hpp"
#include "../state/project_state.hpp"

namespace tpms::ui {

inline void draw_status_bar(const ProjectState& state) {
    ImGuiViewport* vp   = ImGui::GetMainViewport();
    const float    h    = 24.f;
    const float    y    = vp->WorkPos.y + vp->WorkSize.y - h;

    ImGui::SetNextWindowPos({vp->WorkPos.x, y});
    ImGui::SetNextWindowSize({vp->WorkSize.x, h});

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration
      | ImGuiWindowFlags_NoInputs
      | ImGuiWindowFlags_NoNav
      | ImGuiWindowFlags_NoMove
      | ImGuiWindowFlags_NoDocking
      | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.91f, 0.95f, 0.985f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Border, {0.74f, 0.80f, 0.87f, 1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10, 4});
    ImGui::Begin("##status_bar", nullptr, flags);

    const char* status =
        state.solver_running   ? "Status: Solver running"    :
        state.has_results      ? "Status: Results available" :
        state.validation_ok    ? "Status: Ready to solve"    :
        state.has_volume_mesh  ? "Status: Volume mesh ready" :
        state.has_surface_mesh ? "Status: Surface mesh ready":
        state.has_geometry     ? "Status: Geometry ready"    :
                                 "Status: Idle";

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(col_accent(), "%s", status);

    char right_buf[192];
    std::snprintf(right_buf, sizeof right_buf,
                  "Units: mm-N-MPa | Solver: %s | License: H2one Cleantech",
                  state.solver_backend_name.c_str());
    const char* right = right_buf;
    const float rw = ImGui::CalcTextSize(right).x;
    const float avail = ImGui::GetWindowContentRegionMax().x;
    if (avail - ImGui::GetCursorPosX() > rw + 24.f) {
        ImGui::SameLine(avail - rw);
        ImGui::TextDisabled("%s", right);
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace tpms::ui
