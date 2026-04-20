#include "viewport_panel.hpp"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

#include "../geometry/field_texture.hpp"
#include "../geometry/tpms_field.hpp"
#include "../geometry/meshing_engine.hpp"
#include "../preview/vtk_viewport.hpp"

namespace tpms::ui {

// ── init / cleanup ────────────────────────────────────────────────────────────

void ViewportPanel::init_gl() {
    if (!vtk) vtk = std::make_unique<tpms::preview::VtkViewport>();
    vtk->init();
    if (!fallback_preview) fallback_preview = std::make_unique<tpms::ui::preview::PreviewRenderer>();
    fallback_preview->init_gl();
}

void ViewportPanel::cleanup() {
    if (vtk) vtk->cleanup();
    if (fallback_preview) fallback_preview->cleanup();
}

// ── 3D view ───────────────────────────────────────────────────────────────────

void ViewportPanel::draw_3d_view(const ProjectState& state, int w, int h) {
    if (!vtk) { vtk = std::make_unique<tpms::preview::VtkViewport>(); vtk->init(); }

    // ── Sync geometry actors only when data pointers change ──────────────────
    const void* surf_ptr = state.surface_mesh_data;
    const void* vol_ptr  = state.volume_mesh_data;
    const void* field_ptr = state.field_data;

    // Geometry preview: after Generate Geometry there is no formal mesh yet,
    // so create a cached lightweight surface for the VTK viewport.
    if (!surf_ptr && !vol_ptr && field_ptr && field_ptr != cached_field_) {
        cached_field_ = field_ptr;
        cached_surf_ = nullptr;
        cached_vol_ = nullptr;
        cached_res_ = nullptr;
        preview_surface_mesh_.reset();
        vtk->clear_result();

        auto* field = static_cast<const geometry::FieldData*>(field_ptr);
        auto preview_result = geometry::generate_surface_mesh(state, field);
        if (preview_result.validation.ok && preview_result.triangle_count > 0) {
            preview_surface_mesh_ = std::make_unique<geometry::SurfaceMeshData>(std::move(preview_result.mesh));
            vtk->set_surface_mesh(preview_surface_mesh_.get());
            cached_surf_ = preview_surface_mesh_.get();
        } else {
            vtk->set_surface_mesh(nullptr);
        }
    }

    if (surf_ptr) {
        if (surf_ptr != cached_surf_) {
            preview_surface_mesh_.reset();
            cached_surf_ = surf_ptr;
            cached_field_ = field_ptr;
            cached_res_  = nullptr;
            vtk->clear_result();
            vtk->set_surface_mesh(static_cast<const geometry::SurfaceMeshData*>(surf_ptr));
        }
    } else if (!field_ptr && cached_surf_) {
        preview_surface_mesh_.reset();
        cached_surf_ = nullptr;
        cached_field_ = nullptr;
        cached_res_ = nullptr;
        vtk->clear_result();
        vtk->set_surface_mesh(nullptr);
    }

    if (vol_ptr != cached_vol_) {
        preview_surface_mesh_.reset();
        cached_vol_ = vol_ptr;
        cached_field_ = field_ptr;
        cached_res_ = nullptr;
        vtk->clear_result();
        vtk->set_volume_mesh(static_cast<const geometry::VolumeMeshData*>(vol_ptr));
    }

    // ── Result scalar field ───────────────────────────────────────────────────
    const std::string res_key = state.active_result + "|" + state.active_component;
    if (state.has_results && !state.result_scalars.empty()) {
        if (res_key != cached_res_label_ || cached_res_ != vol_ptr) {
            cached_res_       = vol_ptr;
            cached_res_label_ = res_key;
            auto* vm = static_cast<const geometry::VolumeMeshData*>(vol_ptr);
            vtk->set_result(vm, state.result_scalars,
                            state.active_result, state.result_unit,
                            state.result_deform_scale);

            // Build PrePoMax-style info text
            char info[256];
            std::snprintf(info, sizeof info,
                "Result: %s\nUnit: %s\nMin: %.4g\nMax: %.4g",
                state.active_result.c_str(), state.result_unit.c_str(),
                state.result_min, state.result_max);
            vtk->set_result_info(info);
        }
    } else if (!state.has_results && cached_res_ != nullptr) {
        cached_res_ = nullptr;
        cached_res_label_.clear();
        vtk->clear_result();
    }

    // ── Render ───────────────────────────────────────────────────────────────
    GLuint tex = vtk->render(w, h);
    bool using_fallback = false;
    if (!tex && state.has_geometry) {
        if (!fallback_preview) {
            fallback_preview = std::make_unique<tpms::ui::preview::PreviewRenderer>();
            fallback_preview->init_gl();
        }
        fallback_preview->render(
            state,
            w,
            h,
            fallback_azimuth,
            fallback_elevation,
            fallback_distance,
            fallback_pan_x,
            fallback_pan_y
        );
        tex = fallback_preview->texture_id();
        using_fallback = tex != 0;
    }

    ImVec2 canvas = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (tex) {
        dl->AddImage(
            static_cast<ImTextureID>(static_cast<uintptr_t>(tex)),
            canvas,
            {canvas.x + w, canvas.y + h},
            {0, 1}, {1, 0}
        );
    } else {
        dl->AddRectFilled(canvas, {canvas.x + w, canvas.y + h},
                          IM_COL32(31, 36, 43, 255));
        const char* msg =
            state.has_geometry ? "Generating preview..."
                               : "Go to Geometry tab -> Generate Geometry";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        dl->AddText({canvas.x + (w - ts.x) * 0.5f, canvas.y + (h - ts.y) * 0.5f},
                    IM_COL32(120, 150, 190, 255), msg);
    }

    // Invisible button captures mouse events
    ImGui::InvisibleButton("##vtk_vp", {(float)w, (float)h});
    const bool hovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();

    // Local mouse position relative to the canvas top-left
    float mx = io.MousePos.x - canvas.x;
    float my = io.MousePos.y - canvas.y;

    if (hovered) {
        // Wheel → zoom
        if (io.MouseWheel != 0.f) {
            vtk->mouse_wheel(io.MouseWheel);
            fallback_distance = std::max(10.f, fallback_distance - io.MouseWheel * fallback_distance * 0.08f);
        }

        // Button press
        for (int b = 0; b < 3; ++b) {
            if (io.MouseClicked[b]) {
                vtk->mouse_button(b, true, mx, my);
                if (b == 0) fallback_orbit = true;
                if (b == 1) fallback_pan = true;
                last_mouse = io.MousePos;
            }
        }
    }

    // Button release (even outside hover so drag doesn't get stuck)
    for (int b = 0; b < 3; ++b) {
        if (io.MouseReleased[b]) {
            vtk->mouse_button(b, false, mx, my);
            if (b == 0) fallback_orbit = false;
            if (b == 1) fallback_pan = false;
        }
    }

    // Mouse move while any button held
    if (io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f)
        if (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1) || ImGui::IsMouseDown(2))
            vtk->mouse_move(mx, my);
    if (using_fallback && fallback_orbit) {
        fallback_azimuth += io.MouseDelta.x * 0.4f;
        fallback_elevation = std::clamp(fallback_elevation - io.MouseDelta.y * 0.3f, -89.f, 89.f);
    }
    if (using_fallback && fallback_pan) {
        fallback_pan_x += io.MouseDelta.x * 0.1f;
        fallback_pan_y -= io.MouseDelta.y * 0.1f;
    }

    // ── HUD overlay ──────────────────────────────────────────────────────────
    if (state.has_geometry) {
        char hud[256];
        std::snprintf(hud, sizeof hud,
            "%s  |  %.0fx%.0fx%.0f mm  |  %s  |  LMB rotate  RMB pan  Wheel zoom",
            state.design_name(),
            state.size_x, state.size_y, state.size_z,
            state.view_display_mode_name());
        float tw = ImGui::CalcTextSize(hud).x;
        dl->AddRectFilled(canvas, {canvas.x + tw + 16, canvas.y + 22},
                          IM_COL32(20, 24, 30, 200));
        dl->AddText({canvas.x + 8, canvas.y + 4}, IM_COL32(130, 180, 255, 255), hud);
    }

    // Reset camera button
    ImGui::SetCursorScreenPos({canvas.x + w - 90.f, canvas.y + 6.f});
    if (ImGui::SmallButton("Reset Camera")) {
        vtk->reset_camera();
        fallback_azimuth = 45.f;
        fallback_elevation = 28.f;
        fallback_distance = 120.f;
        fallback_pan_x = 0.f;
        fallback_pan_y = 0.f;
    }
}

// ── Slice view ────────────────────────────────────────────────────────────────

void ViewportPanel::draw_slice_view(const ProjectState& state, int w, int h) {
    auto* ftex  = static_cast<geometry::FieldTexture*>(state.slice_texture);
    auto* field = static_cast<geometry::FieldData*>(state.field_data);
    ImVec2 p    = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(p, {p.x + w, p.y + h}, IM_COL32(246, 249, 252, 255));

    if (ftex && ftex->valid() && field && !field->empty()) {
        int slice_index = 0;
        switch (state.slice_axis) {
            case SliceAxis::XY: slice_index = (int)std::lround(state.slice_position * (field->nz - 1)); break;
            case SliceAxis::XZ: slice_index = (int)std::lround(state.slice_position * (field->ny - 1)); break;
            case SliceAxis::YZ: slice_index = (int)std::lround(state.slice_position * (field->nx - 1)); break;
        }
        slice_index = std::max(0, slice_index);
        if (ftex->cached_axis != state.slice_axis || ftex->cached_index != slice_index)
            ftex->upload_slice(*field, state.slice_axis, slice_index);

        float tex_ar = (float)ftex->width / (float)ftex->height;
        float vp_ar  = (float)w / (float)h;
        float tw, th;
        if (tex_ar > vp_ar) { tw = (float)w; th = tw / tex_ar; }
        else                 { th = (float)h; tw = th * tex_ar; }
        float ox = p.x + (w - tw) * 0.5f;
        float oy = p.y + (h - th) * 0.5f;

        dl->AddImage(
            static_cast<ImTextureID>(static_cast<uintptr_t>(ftex->tex_id)),
            {ox, oy}, {ox + tw, oy + th});

        const char* axis_h = "X", *axis_v = "Y";
        if (state.slice_axis == SliceAxis::XZ) axis_v = "Z";
        if (state.slice_axis == SliceAxis::YZ) { axis_h = "Y"; axis_v = "Z"; }
        dl->AddText({ox + tw * 0.5f - 4, oy + th + 4},  IM_COL32(79, 115, 166, 255), axis_h);
        dl->AddText({ox - 14,             oy + th * 0.5f - 6}, IM_COL32(79, 115, 166, 255), axis_v);
        char lbl[64];
        std::snprintf(lbl, sizeof lbl, "%s slice at %.2f",
                      state.slice_axis_name(), state.slice_position);
        dl->AddText({p.x + 8, p.y + 6}, IM_COL32(96, 120, 149, 255), lbl);
    } else {
        const char* msg = "Generate geometry to see slice preview";
        dl->AddText(
            {p.x + w * 0.5f - ImGui::CalcTextSize(msg).x * 0.5f, p.y + h * 0.5f},
            IM_COL32(96, 120, 149, 255), msg);
    }
    ImGui::Dummy({(float)w, (float)h});
}

// ── Details view ──────────────────────────────────────────────────────────────

void ViewportPanel::draw_details(const ProjectState& state) {
    ImGui::Spacing();
    ImGui::TextColored(col_accent(), "Design");
    ImGui::SameLine(100); ImGui::TextUnformatted(state.design_name());

    ImGui::TextDisabled("Domain");
    ImGui::SameLine(100); ImGui::Text("%s", state.domain_name());

    ImGui::TextDisabled("Size");
    ImGui::SameLine(100); ImGui::Text("%.0f x %.0f x %.0f mm", state.size_x, state.size_y, state.size_z);

    ImGui::TextDisabled("Mode");
    ImGui::SameLine(100); ImGui::Text("%s", state.geometry_mode_name());

    ImGui::TextDisabled("Cell");
    ImGui::SameLine(100); ImGui::Text("%.0f x %.0f x %.0f mm", state.cell_x, state.cell_y, state.cell_z);

    ImGui::TextDisabled("Resolution");
    ImGui::SameLine(100); ImGui::Text("%d", state.resolution);

    if (state.has_geometry) {
        ImGui::TextDisabled("Field Grid");
        ImGui::SameLine(100); ImGui::Text("%d x %d x %d", state.field_nx, state.field_ny, state.field_nz);
        ImGui::TextDisabled("Field Range");
        ImGui::SameLine(100); ImGui::Text("%.3f to %.3f", state.field_min, state.field_max);
    }

    if (!state.geometry_summary.empty()) {
        ImGui::Separator();
        ImGui::TextColored(col_accent(), "Geometry Summary");
        ImGui::PushTextWrapPos();
        ImGui::TextUnformatted(state.geometry_summary.c_str());
        ImGui::PopTextWrapPos();
    }

    if (state.has_surface_mesh) {
        ImGui::Separator();
        ImGui::TextColored(col_accent(), "Surface Mesh");
        ImGui::TextDisabled("Nodes");     ImGui::SameLine(100); ImGui::Text("%d", state.surf_nodes);
        ImGui::TextDisabled("Triangles"); ImGui::SameLine(100); ImGui::Text("%d", state.surf_tris);
    }

    if (state.has_volume_mesh) {
        ImGui::Separator();
        ImGui::TextColored(col_accent(), "Volume Mesh");
        ImGui::TextDisabled("Nodes"); ImGui::SameLine(100); ImGui::Text("%d", state.vol_nodes);
        ImGui::TextDisabled("Tets");  ImGui::SameLine(100); ImGui::Text("%d", state.vol_tets);
    }
}

} // namespace tpms::ui
