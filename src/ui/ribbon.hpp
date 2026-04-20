#pragma once

#include <imgui.h>
#include <cstdio>
#include "theme.hpp"
#include "../state/project_state.hpp"

namespace tpms::ui {

// ── Ribbon tab bar + contextual toolbar ──────────────────────────────────────

// Returns true if the user clicked a ribbon action (caller handles it)
struct RibbonAction {
    enum Type {
        None,
        // Geometry
        GenerateGeometry,
        // Meshing
        GenerateSurfaceMesh, GenerateVolumeMesh,
        // BCs
        AssignMaterial, AddFixedSupport, AddForceLoad, AddDisplacementLoad,
        // Solve
        Validate, Solve,
        // Results
        ShowVonMises, ShowDisplacement, ShowStrain,
        ExportReport, ExportActiveResult, OpenPyVista, RunHealthCheck,
        // File / project
        NewProject, OpenProject, SaveProject, ExitApp,
        // Geometry exports
        ExportSTLBinary, ExportSTLAscii, ExportOBJ, ExportPLY,
        // Mesh exports
        ExportMesh,        // VTK unstructured grid (.vtk)
        // Field exports
        ExportFieldCSV, ExportFieldRaw,
    } type = None;
};

enum class RibbonGlyph {
    NewFile,
    OpenFolder,
    SaveDisk,
    Export,
    Generate,
    SurfaceMesh,
    VolumeMesh,
    Fixed,
    Force,
    Validate,
    Solve,
    Result,
    Download   // export/save-to-disk icon
};

inline void draw_ribbon_glyph(ImDrawList* dl, ImVec2 p, ImVec2 sz, RibbonGlyph glyph, ImU32 col) {
    const float x = p.x;
    const float y = p.y;
    const float w = sz.x;
    const float h = sz.y;
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const float t = 1.8f;

    switch (glyph) {
        case RibbonGlyph::NewFile:
            dl->AddRect({x + 8, y + 6}, {x + w - 10, y + h - 6}, col, 2.0f, 0, t);
            dl->AddLine({x + w - 20, y + 6}, {x + w - 10, y + 16}, col, t);
            dl->AddLine({x + w - 20, y + 6}, {x + w - 20, y + 16}, col, t);
            dl->AddLine({x + 15, cy}, {x + w - 17, cy}, col, t);
            dl->AddLine({cx, y + 14}, {cx, y + h - 14}, col, t);
            break;
        case RibbonGlyph::OpenFolder:
            dl->AddLine({x + 8, y + 17}, {x + 18, y + 10}, col, t);
            dl->AddLine({x + 18, y + 10}, {x + 28, y + 10}, col, t);
            dl->AddRect({x + 8, y + 16}, {x + w - 8, y + h - 8}, col, 2.0f, 0, t);
            break;
        case RibbonGlyph::SaveDisk:
            dl->AddRect({x + 8, y + 7}, {x + w - 8, y + h - 7}, col, 2.0f, 0, t);
            dl->AddRectFilled({x + 14, y + 12}, {x + w - 14, y + 22}, col);
            dl->AddRect({x + 17, y + 28}, {x + w - 17, y + h - 12}, col, 1.5f, 0, t);
            break;
        case RibbonGlyph::Export:
            dl->AddRect({x + 9, y + 10}, {x + w - 9, y + h - 10}, col, 2.0f, 0, t);
            dl->AddLine({cx, y + 8}, {cx, y + h - 16}, col, t);
            dl->AddTriangleFilled({cx - 7, y + h - 22}, {cx + 7, y + h - 22}, {cx, y + h - 10}, col);
            break;
        case RibbonGlyph::Generate:
            dl->AddCircle({cx, cy}, 13.0f, col, 28, t);
            dl->AddTriangleFilled({cx + 11, cy}, {cx + 4, cy - 5}, {cx + 4, cy + 5}, col);
            break;
        case RibbonGlyph::SurfaceMesh:
            for (int i = 0; i < 4; ++i) {
                float yy = y + 10 + i * 8.0f;
                dl->AddLine({x + 10, yy}, {x + w - 10, yy}, col, 1.2f);
            }
            for (int i = 0; i < 4; ++i) {
                float xx = x + 12 + i * 10.0f;
                dl->AddLine({xx, y + 9}, {xx, y + h - 10}, col, 1.2f);
            }
            break;
        case RibbonGlyph::VolumeMesh:
            dl->AddRect({x + 12, y + 12}, {x + w - 12, y + h - 12}, col, 1.5f, 0, t);
            dl->AddLine({x + 18, y + 8}, {x + w - 6, y + 18}, col, 1.2f);
            dl->AddLine({x + 18, y + 8}, {x + 12, y + 12}, col, 1.2f);
            dl->AddLine({x + w - 6, y + 18}, {x + w - 12, y + h - 12}, col, 1.2f);
            dl->AddLine({x + 18, y + 8}, {x + 18, y + h - 18}, col, 1.2f);
            break;
        case RibbonGlyph::Fixed:
            dl->AddRectFilled({x + 10, y + h - 16}, {x + w - 10, y + h - 10}, col);
            dl->AddLine({cx, y + 10}, {cx, y + h - 18}, col, t);
            dl->AddTriangleFilled({cx - 6, y + 18}, {cx + 6, y + 18}, {cx, y + 10}, col);
            break;
        case RibbonGlyph::Force:
            dl->AddLine({x + 12, cy}, {x + w - 14, cy}, col, t + 0.4f);
            dl->AddTriangleFilled({x + w - 20, cy - 7}, {x + w - 20, cy + 7}, {x + w - 8, cy}, col);
            break;
        case RibbonGlyph::Validate:
            dl->AddLine({x + 12, cy}, {x + 20, y + h - 14}, col, t);
            dl->AddLine({x + 20, y + h - 14}, {x + w - 10, y + 14}, col, t);
            break;
        case RibbonGlyph::Solve:
            dl->AddCircle({cx, cy}, 13.0f, col, 28, t);
            dl->AddLine({cx, cy - 10}, {cx, cy + 10}, col, t);
            dl->AddLine({cx - 10, cy}, {cx + 10, cy}, col, t);
            break;
        case RibbonGlyph::Result:
            dl->AddLine({x + 10, y + h - 10}, {x + 10, y + 10}, col, t);
            dl->AddLine({x + 10, y + h - 10}, {x + w - 8, y + h - 10}, col, t);
            dl->AddRectFilled({x + 18, y + h - 20}, {x + 26, y + h - 10}, col);
            dl->AddRectFilled({x + 30, y + h - 28}, {x + 38, y + h - 10}, col);
            dl->AddRectFilled({x + 42, y + h - 34}, {x + 50, y + h - 10}, col);
            break;
        case RibbonGlyph::Download:
            // Arrow pointing down into a tray
            dl->AddLine({cx, y + 8}, {cx, y + h - 18}, col, t);
            dl->AddTriangleFilled({cx - 8, y + h - 24}, {cx + 8, y + h - 24}, {cx, y + h - 14}, col);
            dl->AddLine({x + 10, y + h - 10}, {x + w - 10, y + h - 10}, col, t + 0.5f);
            break;
    }
}

inline bool ribbon_tile_button(
    const char* id,
    const char* label,
    RibbonGlyph glyph,
    bool enabled,
    ImVec4 accent = col_accent(),
    ImVec2 size = {84.f, 72.f}
) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGui::PushID(id);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton("##tile", size) && enabled;
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    ImU32 bg = IM_COL32(240, 245, 251, 255);
    ImU32 border = IM_COL32(190, 205, 221, 255);
    ImU32 icon_bg = IM_COL32(223, 233, 245, 255);
    ImU32 text = IM_COL32(31, 39, 53, 255);

    if (!enabled) {
        bg = IM_COL32(238, 241, 245, 255);
        border = IM_COL32(205, 212, 220, 255);
        icon_bg = IM_COL32(228, 232, 237, 255);
        text = IM_COL32(123, 133, 145, 255);
    } else if (held) {
        bg = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.20f));
        border = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.65f));
        icon_bg = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.24f));
    } else if (hovered) {
        bg = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.12f));
        border = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.45f));
        icon_bg = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.18f));
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImRect r(pos, {pos.x + size.x, pos.y + size.y});
    dl->AddRectFilled(r.Min, r.Max, bg, 4.0f);
    dl->AddRect(r.Min, r.Max, border, 4.0f, 0, 1.0f);

    ImRect icon_r({pos.x + 12, pos.y + 8}, {pos.x + size.x - 12, pos.y + 42});
    dl->AddRectFilled(icon_r.Min, icon_r.Max, icon_bg, 4.0f);
    dl->AddRect(icon_r.Min, icon_r.Max, border, 4.0f, 0, 1.0f);
    draw_ribbon_glyph(dl, icon_r.Min, icon_r.GetSize(), glyph, enabled ? ImGui::GetColorU32(accent) : text);

    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText({pos.x + (size.x - ts.x) * 0.5f, pos.y + 50}, text, label);
    ImGui::PopID();
    return clicked;
}

inline void begin_ribbon_group(const char* title, float width = 0.f, float height = 90.f) {
    if (width <= 0.f) width = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild(title, {width, height}, true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextDisabled("%s", title);
    ImGui::Separator();
}

inline void end_ribbon_group() {
    ImGui::EndChild();
}

inline RibbonAction draw_ribbon(ProjectState& state) {
    RibbonAction action;

    // ── Tab bar ───────────────────────────────────────────────────────────────
    ImGuiTabBarFlags tab_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("##ribbon_tabs", tab_flags)) {

        (void)[&](const char*, WorkflowTab) -> bool { return false; };  // unused lambda removed

        // ── Home ─────────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Home")) {
            state.active_tab = WorkflowTab::Home;
            ImGui::EndTabItem();
        }

        // ── Geometry ─────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Geometry")) {
            state.active_tab = WorkflowTab::Geometry;
            ImGui::EndTabItem();
        }

        // ── Meshing ───────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Meshing")) {
            state.active_tab = WorkflowTab::Meshing;
            ImGui::EndTabItem();
        }

        // ── Boundary Conditions ───────────────────────────────────────────────
        if (ImGui::BeginTabItem("Boundary Conditions")) {
            state.active_tab = WorkflowTab::BCs;
            ImGui::EndTabItem();
        }

        // ── Solve ─────────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Solve")) {
            state.active_tab = WorkflowTab::Solve;
            ImGui::EndTabItem();
        }

        // ── Results ───────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Results")) {
            state.active_tab = WorkflowTab::Results;
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();

    // ── Contextual toolbar per active tab ─────────────────────────────────────
    ImGui::BeginChild("##toolbar_scroller", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);

    switch (state.active_tab) {
    // ── Home toolbar ─────────────────────────────────────────────────────────
    case WorkflowTab::Home: {
        begin_ribbon_group("Project", 300.f);
        if (ribbon_tile_button("new", "New", RibbonGlyph::NewFile, true)) action.type = RibbonAction::NewProject;
        ImGui::SameLine();
        if (ribbon_tile_button("open", "Open", RibbonGlyph::OpenFolder, true)) action.type = RibbonAction::OpenProject;
        ImGui::SameLine();
        if (ribbon_tile_button("save", "Save", RibbonGlyph::SaveDisk, state.dirty)) action.type = RibbonAction::SaveProject;
        end_ribbon_group();
        ImGui::SameLine();
        begin_ribbon_group("Export", 108.f);
        if (ribbon_tile_button("export_mesh", "Export", RibbonGlyph::Export, state.has_volume_mesh)) action.type = RibbonAction::ExportMesh;
        end_ribbon_group();
        break;
    }

    // ── Geometry toolbar ─────────────────────────────────────────────────────
    case WorkflowTab::Geometry: {
        begin_ribbon_group("Setup", 280.f);
        ImGui::PushItemWidth(120.f);
        const char* designs[] = { "Gyroid", "Diamond", "Schwarz-P", "Lidinoid", "Neovius" };
        int idx = static_cast<int>(state.design);
        if (ImGui::Combo("TPMS Type", &idx, designs, 5))
            state.design = static_cast<TPMSDesign>(idx);
        const char* domains[] = { "Cuboid", "Cylinder", "Sphere" };
        int domain_idx = static_cast<int>(state.domain);
        if (ImGui::Combo("Domain", &domain_idx, domains, 3))
            state.domain = static_cast<DomainType>(domain_idx);
        ImGui::PopItemWidth();
        end_ribbon_group();
        ImGui::SameLine();
        begin_ribbon_group("Actions", 108.f);
        if (ribbon_tile_button("gen_geom", "Generate", RibbonGlyph::Generate, true))
            action.type = RibbonAction::GenerateGeometry;
        end_ribbon_group();
        ImGui::SameLine();
        begin_ribbon_group("Export Geometry", 310.f);
        const bool has_surf = state.has_surface_mesh;
        if (ribbon_tile_button("exp_stlb", "STL", RibbonGlyph::Download, has_surf))
            action.type = RibbonAction::ExportSTLBinary;
        ImGui::SameLine(0, 4);
        if (ribbon_tile_button("exp_obj",  "OBJ", RibbonGlyph::Download, has_surf))
            action.type = RibbonAction::ExportOBJ;
        ImGui::SameLine(0, 4);
        if (ribbon_tile_button("exp_ply",  "PLY", RibbonGlyph::Download, has_surf))
            action.type = RibbonAction::ExportPLY;
        end_ribbon_group();
        ImGui::SameLine();
        begin_ribbon_group("Export Field", 210.f);
        const bool has_field = state.has_geometry;
        if (ribbon_tile_button("exp_fcsv", "Slice CSV", RibbonGlyph::Download, has_field))
            action.type = RibbonAction::ExportFieldCSV;
        ImGui::SameLine(0, 4);
        if (ribbon_tile_button("exp_fraw", "Field Raw", RibbonGlyph::Download, has_field))
            action.type = RibbonAction::ExportFieldRaw;
        end_ribbon_group();
        break;
    }

    // ── Meshing toolbar ───────────────────────────────────────────────────────
    case WorkflowTab::Meshing: {
        begin_ribbon_group("Mesh", 210.f);
        if (ribbon_tile_button("surf", "Surface", RibbonGlyph::SurfaceMesh, state.can_surface_mesh()))
            action.type = RibbonAction::GenerateSurfaceMesh;
        ImGui::SameLine(0, 16);
        if (ribbon_tile_button("vol", "Volume", RibbonGlyph::VolumeMesh, state.can_volume_mesh()))
            action.type = RibbonAction::GenerateVolumeMesh;
        end_ribbon_group();
        ImGui::SameLine();
        begin_ribbon_group("Controls", 150.f);
        ImGui::SetNextItemWidth(108.f);
        ImGui::InputFloat("Element", &state.elem_size, 0.1f, 1.f, "%.1f mm");
        end_ribbon_group();
        break;
    }

    // ── BCs toolbar ───────────────────────────────────────────────────────────
    case WorkflowTab::BCs: {
        begin_ribbon_group("Target Face", 200.f);
        ImGui::TextDisabled("Face:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.f);
        const char* faces[] = {"Bottom","Top","Back","Front","Left","Right"};
        int fs = 0;
        for (int i = 0; i < 6; ++i)
            if (state.pending_bc_face == faces[i]) { fs = i; break; }
        if (ImGui::Combo("##rface", &fs, faces, 6))
            state.pending_bc_face = faces[fs];
        end_ribbon_group();
        ImGui::SameLine();
        begin_ribbon_group("Supports", 110.f);
        if (ribbon_tile_button("fixed", "Fixed", RibbonGlyph::Fixed, state.can_assign_bc()))
            action.type = RibbonAction::AddFixedSupport;
        end_ribbon_group();
        ImGui::SameLine();
        begin_ribbon_group("Loads", 210.f);
        if (ribbon_tile_button("force", "Force", RibbonGlyph::Force, state.can_assign_bc()))
            action.type = RibbonAction::AddForceLoad;
        ImGui::SameLine(0, 8);
        if (ribbon_tile_button("disp_load", "Displ.", RibbonGlyph::Fixed, state.can_assign_bc(),
                               {0.52f, 0.35f, 0.76f, 1.f}))
            action.type = RibbonAction::AddDisplacementLoad;
        end_ribbon_group();
        break;
    }

    // ── Solve toolbar ─────────────────────────────────────────────────────────
    case WorkflowTab::Solve: {
        begin_ribbon_group("Solve", 210.f);
        if (ribbon_tile_button("validate", "Validate", RibbonGlyph::Validate, state.can_validate(), col_warning()))
            action.type = RibbonAction::Validate;
        ImGui::SameLine(0, 8);
        if (ribbon_tile_button("solve", "Solve", RibbonGlyph::Solve, state.can_solve(), col_success()))
            action.type = RibbonAction::Solve;
        end_ribbon_group();
        break;
    }

    // ── Results toolbar ───────────────────────────────────────────────────────
    case WorkflowTab::Results: {
        begin_ribbon_group("Results", 300.f);
        if (ribbon_tile_button("vm", "Stress", RibbonGlyph::Result, state.can_show_results()))
            action.type = RibbonAction::ShowVonMises;
        ImGui::SameLine();
        if (ribbon_tile_button("disp", "Disp", RibbonGlyph::Result, state.can_show_results()))
            action.type = RibbonAction::ShowDisplacement;
        ImGui::SameLine();
        if (ribbon_tile_button("strain", "Strain", RibbonGlyph::Result, state.can_show_results()))
            action.type = RibbonAction::ShowStrain;
        end_ribbon_group();
        break;
    }
    }

    ImGui::EndChild();
    return action;
}

} // namespace tpms::ui
