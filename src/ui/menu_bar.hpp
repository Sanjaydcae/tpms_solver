#pragma once

#include <imgui.h>
#include "ribbon.hpp"
#include "../state/project_state.hpp"

namespace tpms::ui {

inline RibbonAction draw_main_menu_bar(ProjectState& state) {
    RibbonAction action;
    if (!ImGui::BeginMainMenuBar()) {
        return action;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project",  "Ctrl+N")) action.type = RibbonAction::NewProject;
        if (ImGui::MenuItem("Open Project", "Ctrl+O")) action.type = RibbonAction::OpenProject;
        if (ImGui::MenuItem("Save",         "Ctrl+S", false, state.dirty)) action.type = RibbonAction::SaveProject;
        ImGui::Separator();

        // ── Export Geometry ───────────────────────────────────────────────────
        const bool can_surf = state.has_surface_mesh;
        if (ImGui::BeginMenu("Export Geometry", can_surf)) {
            if (ImGui::MenuItem("Binary STL (.stl)")) action.type = RibbonAction::ExportSTLBinary;
            if (ImGui::MenuItem("ASCII STL (.stl)"))  action.type = RibbonAction::ExportSTLAscii;
            if (ImGui::MenuItem("OBJ (.obj)"))         action.type = RibbonAction::ExportOBJ;
            if (ImGui::MenuItem("PLY (.ply)"))         action.type = RibbonAction::ExportPLY;
            ImGui::EndMenu();
        }

        // ── Export Volume Mesh ────────────────────────────────────────────────
        if (ImGui::MenuItem("Export Volume Mesh (.vtk)", nullptr, false, state.has_volume_mesh))
            action.type = RibbonAction::ExportMesh;

        if (ImGui::MenuItem("Export Active Result (.vtk)", nullptr, false, state.has_results))
            action.type = RibbonAction::ExportActiveResult;

        if (ImGui::MenuItem("Export Report (.md)", nullptr, false, true))
            action.type = RibbonAction::ExportReport;

        // ── Export Field ──────────────────────────────────────────────────────
        const bool can_field = state.has_geometry;
        if (ImGui::BeginMenu("Export Field", can_field)) {
            if (ImGui::MenuItem("Field Slice CSV")) action.type = RibbonAction::ExportFieldCSV;
            if (ImGui::MenuItem("Field Raw Binary")) action.type = RibbonAction::ExportFieldRaw;
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) action.type = RibbonAction::ExitApp;
        ImGui::EndMenu();
    }

    auto activate_tab = [&](const char* label, WorkflowTab tab) {
        const bool selected = state.active_tab == tab;
        if (ImGui::MenuItem(label, nullptr, selected)) {
            state.active_tab = tab;
        }
    };

    if (ImGui::BeginMenu("Geometry")) {
        activate_tab("Open Geometry Workspace", WorkflowTab::Geometry);
        ImGui::Separator();
        const char* designs[] = { "Gyroid", "Diamond", "Schwarz-P", "Lidinoid", "Neovius" };
        int design_idx = static_cast<int>(state.design);
        if (ImGui::BeginCombo("TPMS Type", designs[design_idx])) {
            for (int i = 0; i < 5; ++i) {
                const bool selected = design_idx == i;
                if (ImGui::Selectable(designs[i], selected)) {
                    state.design = static_cast<TPMSDesign>(i);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::MenuItem("Generate Geometry", nullptr, false, true)) {
            action.type = RibbonAction::GenerateGeometry;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Meshing")) {
        activate_tab("Open Meshing Workspace", WorkflowTab::Meshing);
        ImGui::Separator();
        if (ImGui::MenuItem("Surface Mesh", nullptr, false, state.can_surface_mesh())) {
            action.type = RibbonAction::GenerateSurfaceMesh;
        }
        if (ImGui::MenuItem("Volume Mesh", nullptr, false, state.can_volume_mesh())) {
            action.type = RibbonAction::GenerateVolumeMesh;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Boundary Conditions")) {
        activate_tab("Open BC Workspace", WorkflowTab::BCs);
        ImGui::Separator();
        if (ImGui::MenuItem("Assign Material", nullptr, false, state.can_assign_bc())) {
            action.type = RibbonAction::AssignMaterial;
        }
        if (ImGui::MenuItem("Fixed Support", nullptr, false, state.can_assign_bc())) {
            action.type = RibbonAction::AddFixedSupport;
        }
        if (ImGui::MenuItem("Force Load", nullptr, false, state.can_assign_bc())) {
            action.type = RibbonAction::AddForceLoad;
        }
        if (ImGui::MenuItem("Displacement Load", nullptr, false, state.can_assign_bc())) {
            action.type = RibbonAction::AddDisplacementLoad;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Solve")) {
        activate_tab("Open Solve Workspace", WorkflowTab::Solve);
        ImGui::Separator();
        if (ImGui::MenuItem("Validate Model", nullptr, false, state.can_validate())) {
            action.type = RibbonAction::Validate;
        }
        if (ImGui::MenuItem("Run Solve", nullptr, false, state.can_solve())) {
            action.type = RibbonAction::Solve;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Results")) {
        activate_tab("Open Results Workspace", WorkflowTab::Results);
        ImGui::Separator();
        if (ImGui::MenuItem("Von Mises", nullptr, false, state.can_show_results())) {
            action.type = RibbonAction::ShowVonMises;
        }
        if (ImGui::BeginMenu("Displacement", state.can_show_results())) {
            if (ImGui::MenuItem("Total", nullptr, false, state.can_show_results())) {
                action.type = RibbonAction::ShowDisplacement;
            }
            if (ImGui::MenuItem("X Component", nullptr, false, state.can_show_results())) {
                action.type = RibbonAction::ShowDisplacementX;
            }
            if (ImGui::MenuItem("Y Component", nullptr, false, state.can_show_results())) {
                action.type = RibbonAction::ShowDisplacementY;
            }
            if (ImGui::MenuItem("Z Component", nullptr, false, state.can_show_results())) {
                action.type = RibbonAction::ShowDisplacementZ;
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Strain", nullptr, false, state.can_show_results())) {
            action.type = RibbonAction::ShowStrain;
        }
        if (ImGui::MenuItem("Reaction Force", nullptr, false, state.can_show_results())) {
            action.type = RibbonAction::ShowReactionForce;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Export Active Result (.vtk)", nullptr, false, state.can_show_results())) {
            action.type = RibbonAction::ExportActiveResult;
        }
        if (ImGui::MenuItem("Open in PyVista", nullptr, false, state.has_volume_mesh)) {
            action.type = RibbonAction::OpenPyVista;
        }
        if (ImGui::MenuItem("Generate Report (.md)", nullptr, false, true)) {
            action.type = RibbonAction::ExportReport;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        activate_tab("Home", WorkflowTab::Home);
        activate_tab("Geometry", WorkflowTab::Geometry);
        activate_tab("Meshing", WorkflowTab::Meshing);
        activate_tab("Boundary Conditions", WorkflowTab::BCs);
        activate_tab("Solve", WorkflowTab::Solve);
        activate_tab("Results", WorkflowTab::Results);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Model Health Check")) {
            action.type = RibbonAction::RunHealthCheck;
        }
        if (ImGui::MenuItem("Open Active Result in PyVista", nullptr, false, state.has_volume_mesh)) {
            action.type = RibbonAction::OpenPyVista;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("TPMS Studio v0.1", nullptr, false, false);
        ImGui::Separator();
        ImGui::MenuItem("H2one Cleantech Private Limited", nullptr, false, false);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
    return action;
}

} // namespace tpms::ui
