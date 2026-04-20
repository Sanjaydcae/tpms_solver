#pragma once

#include <imgui.h>
#include <string>
#include <memory>
#include "theme.hpp"
#include "../state/project_state.hpp"
#include "../preview/vtk_viewport.hpp"
#include "../preview/preview_renderer.hpp"

namespace tpms::ui {

// ── Viewport host for the VTK-based preview subsystem ────────────────────────

struct ViewportPanel {
    std::unique_ptr<tpms::preview::VtkViewport> vtk;
    std::unique_ptr<tpms::ui::preview::PreviewRenderer> fallback_preview;

    // Track which data pointers are currently uploaded so we only re-upload on change
    const void* cached_field_ = nullptr;
    const void* cached_surf_ = nullptr;
    const void* cached_vol_  = nullptr;
    const void* cached_res_  = nullptr;
    std::string cached_res_label_;
    std::unique_ptr<geometry::SurfaceMeshData> preview_surface_mesh_;

    bool dragging = false;
    ImVec2 last_mouse{};
    float fallback_azimuth = 45.f;
    float fallback_elevation = 28.f;
    float fallback_distance = 120.f;
    float fallback_pan_x = 0.f;
    float fallback_pan_y = 0.f;
    bool fallback_orbit = false;
    bool fallback_pan = false;

    // Active display tab: 0=3D View, 1=Slice XY, 2=Details
    int active_view = 0;

    void init_gl();
    void cleanup();

    void draw(const ProjectState& state) {
        if (!ImGui::Begin("3D Graphics Window")) { ImGui::End(); return; }

        // Meta info bar
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.96f, 0.98f, 1.00f, 1.f});
        if (ImGui::BeginChild("##meta", {0, 44}, false, ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
            ImGui::Text(
                "TPMS: %s | Domain: %s | Size: %.0f x %.0f x %.0f mm | "
                "Cell: %.0f x %.0f x %.0f mm | Mode: %s | Display: %s | Grid: %d x %d x %d | Mesh: %s",
                state.design_name(),
                state.domain_name(),
                state.size_x, state.size_y, state.size_z,
                state.cell_x, state.cell_y, state.cell_z,
                state.geometry_mode_name(),
                state.view_display_mode_name(),
                state.field_nx, state.field_ny, state.field_nz,
                state.has_volume_mesh  ? "Volume ready"
              : state.has_surface_mesh ? "Surface ready"
              : state.has_geometry     ? "Geometry ready"
              : "None"
            );
            if (!state.geometry_summary.empty()) {
                ImGui::TextDisabled("%s", state.geometry_summary.c_str());
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // View tabs
        if (ImGui::BeginTabBar("##view_tabs")) {
            if (ImGui::BeginTabItem("3D View"))    { active_view = 0; ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Slice (XY)")) { active_view = 1; ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Details"))    { active_view = 2; ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        int w = (int)avail.x;
        int h = (int)avail.y;
        if (w < 4 || h < 4) { ImGui::End(); return; }

        if (active_view == 0) {
            draw_3d_view(state, w, h);
        } else if (active_view == 1) {
            draw_slice_view(state, w, h);
        } else {
            draw_details(state);
        }

        ImGui::End();
    }

private:
    void draw_3d_view(const ProjectState& state, int w, int h);
    void draw_slice_view(const ProjectState& state, int w, int h);
    void draw_details(const ProjectState& state);
};

} // namespace tpms::ui
