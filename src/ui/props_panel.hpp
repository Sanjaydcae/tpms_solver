#pragma once

#include <imgui.h>
#include "theme.hpp"
#include "../state/project_state.hpp"
#include "../preprocess/preprocess_engine.hpp"

namespace tpms::ui {

// ── Section header helper ─────────────────────────────────────────────────────
inline void props_header(const char* label) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, col_accent());
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();
}

// ── Row helpers (label + widget aligned) ─────────────────────────────────────
inline void begin_props_table() {
    ImGui::BeginTable("##props", 2,
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV);
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 128);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
}
inline void end_props_table() { ImGui::EndTable(); }

inline void prop_row(const char* label, const char* fmt, ...) {
    char buf[256];
    va_list args; va_start(args, fmt); vsnprintf(buf, sizeof buf, fmt, args); va_end(args);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(buf);
}

inline void prop_label(const char* label) {
    ImGui::TextDisabled("%s", label);
    ImGui::SetNextItemWidth(-1);
}

// ─────────────────────────────────────────────────────────────────────────────

inline void draw_props_panel(ProjectState& state) {
    ImGui::SetNextWindowSize({430, -1}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Properties")) { ImGui::End(); return; }

    auto invalidate_preprocess = [&]() {
        state.validation_ok = false;
        state.has_preprocessed_model = false;
        state.preprocess_n_dofs = 0;
        state.preprocess_n_constrained = 0;
        state.preprocess_n_loaded = 0;
        state.preprocess_summary.clear();
        state.solver_has_run = false;
        state.solver_running = false;
        state.solver_converged = false;
        state.solver_iterations = 0;
        state.solver_current_iteration = 0;
        state.solver_progress = 0.f;
        state.solver_initial_residual = 0.f;
        state.solver_final_residual = 0.f;
        state.solver_current_relative_residual = 0.f;
        state.solver_status_text.clear();
        state.solver_summary.clear();
        state.solver_residual_history.clear();
        state.fem_summary.clear();
        state.has_results = false;
        state.active_result.clear();
        state.active_component.clear();
        state.result_min = 0.f;
        state.result_max = 0.f;
        state.result_min_node = -1;
        state.result_max_node = -1;
        state.result_unit.clear();
        state.result_summary.clear();
        state.validation_summary.clear();
        state.result_scalars.clear();
        state.displacement_result_scalars.clear();
        state.displacement_x_result_scalars.clear();
        state.displacement_y_result_scalars.clear();
        state.displacement_z_result_scalars.clear();
        state.von_mises_result_scalars.clear();
        state.strain_result_scalars.clear();
        state.reaction_force_result_scalars.clear();
        state.displacement_solution.clear();
        state.field_outputs.clear();
        state.dirty = true;
    };

    switch (state.active_tab) {

    // ── Home props ────────────────────────────────────────────────────────────
    case WorkflowTab::Home: {
        props_header("PROJECT");
        ImGui::TextUnformatted("Name:");
        ImGui::SameLine();
        static char name_buf[128];
        if (name_buf[0] == '\0')
            snprintf(name_buf, sizeof name_buf, "%s", state.project_name.c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##pname", name_buf, sizeof name_buf)) {
            state.project_name = name_buf;
            state.dirty = true;
        }

        ImGui::Spacing();
        props_header("STATUS");
        begin_props_table();
        prop_row("Geometry",     "%s", state.has_geometry    ? "Ready"     : "Not generated");
        prop_row("Surface Mesh", "%s", state.has_surface_mesh? "Ready"     : "Not generated");
        prop_row("Volume Mesh",  "%s", state.has_volume_mesh ? "Ready"     : "Not generated");
        prop_row("BCs",          "%s", (state.has_fixed_bc && state.has_load_bc) ? "Assigned" : "Incomplete");
        prop_row("Solve",        "%s", state.has_results     ? "Complete"  : "Not run");
        end_props_table();
        break;
    }

    // ── Geometry props ────────────────────────────────────────────────────────
    case WorkflowTab::Geometry: {
        props_header("TPMS DESIGN");
        const char* designs[] = { "Gyroid", "Diamond", "Schwarz-P", "Lidinoid", "Neovius" };
        int idx = static_cast<int>(state.design);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##design", &idx, designs, 5))
            state.design = static_cast<TPMSDesign>(idx);

        props_header("GEOMETRY MODE");
        const char* modes[] = { "Shell", "Solid" };
        int mode_idx = static_cast<int>(state.geometry_mode);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##geom_mode", &mode_idx, modes, 2))
            state.geometry_mode = static_cast<GeometryMode>(mode_idx);

        props_header("DOMAIN TYPE");
        const char* domains[] = { "Cuboid", "Cylinder", "Sphere" };
        int domain_idx = static_cast<int>(state.domain);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##domain_type", &domain_idx, domains, 3))
            state.domain = static_cast<DomainType>(domain_idx);

        props_header("DOMAIN SIZE  (mm)");
        prop_label("X / Y / Z");
        ImGui::DragFloat3("##size", &state.size_x, 1.f, 1.f, 500.f, "%.1f");

        props_header("UNIT CELL  (mm)");
        prop_label("X / Y / Z");
        ImGui::DragFloat3("##cell", &state.cell_x, 0.5f, 0.5f, 100.f, "%.1f");

        props_header("ORIGIN  (mm)");
        prop_label("X / Y / Z");
        ImGui::DragFloat3("##origin", &state.origin_x, 0.5f, -250.f, 250.f, "%.1f");

        props_header("PARAMETERS");
        prop_label("Wall Thickness");
        ImGui::SliderFloat("##wall_thickness", &state.wall_thickness, 0.1f, 5.f, "%.2f mm");
        prop_label("Preview Resolution");
        ImGui::SliderInt("##resolution", &state.resolution, 20, 240);

        props_header("SLICE PREVIEW");
        const char* slice_axes[] = { "XY", "XZ", "YZ" };
        int slice_axis = static_cast<int>(state.slice_axis);
        prop_label("Slice Plane");
        if (ImGui::Combo("##slice_plane", &slice_axis, slice_axes, 3))
            state.slice_axis = static_cast<SliceAxis>(slice_axis);
        prop_label("Slice Position");
        ImGui::SliderFloat("##slice_position", &state.slice_position, 0.0f, 1.0f, "%.2f");

        props_header("DISPLAY");
        const char* geom_views[] = { "Solid", "Surface" };
        int geom_view_idx = state.view_display_mode == ViewDisplayMode::Surface ? 1 : 0;
        prop_label("Geometry View");
        if (ImGui::Combo("##geometry_view", &geom_view_idx, geom_views, 2)) {
            state.view_display_mode = geom_view_idx == 0 ? ViewDisplayMode::Solid : ViewDisplayMode::Surface;
        }

        if (state.has_geometry) {
            ImGui::Spacing();
            props_header("GEOMETRY STATS");
            begin_props_table();
            prop_row("Design", "%s", state.design_name());
            prop_row("Mode", "%s", state.geometry_mode_name());
            prop_row("Domain", "%s", state.domain_name());
            prop_row("Grid",   "%dx%dx%d", state.field_nx, state.field_ny, state.field_nz);
            prop_row("Cells", "%dx%dx%d", state.cell_count_x, state.cell_count_y, state.cell_count_z);
            prop_row("Field", "%.3f to %.3f", state.field_min, state.field_max);
            prop_row("Solid Fraction", "%.1f %%", state.solid_fraction * 100.0f);
            end_props_table();

            if (!state.geometry_summary.empty()) {
                ImGui::Spacing();
                ImGui::PushTextWrapPos();
                ImGui::TextDisabled("%s", state.geometry_summary.c_str());
                ImGui::PopTextWrapPos();
            }
        }
        break;
    }

    // ── Meshing props ─────────────────────────────────────────────────────────
    case WorkflowTab::Meshing: {
        props_header("MESH SETTINGS");
        const char* engines[] = {
            "GMSH",
            "Netgen",
            "Advancing Front",
            "Marching Cubes",
            "Voxel Tet"
        };
        int eng = static_cast<int>(state.mesh_engine);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("Meshing Engine##eng", &eng, engines, 5))
            state.mesh_engine = static_cast<MeshEngine>(eng);
        ImGui::PushTextWrapPos();
        ImGui::TextDisabled("Use GMSH or Netgen for the cleanest general-purpose mesh. Marching Cubes and Voxel Tet are faster preview-oriented options.");
        ImGui::PopTextWrapPos();

        props_header("ELEMENT TYPES");
        begin_props_table();
        prop_row("Surface", "Tri3 only");
        prop_row("Volume", "Tet4 only");
        end_props_table();

        props_header("VOLUME TARGET");
        const char* targets[] = { "TPMS Body", "Domain Box" };
        int target = static_cast<int>(state.volume_mesh_target);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("Volume Mesh Target##vol_target", &target, targets, 2))
            state.volume_mesh_target = static_cast<VolumeMeshTarget>(target);
        ImGui::PushTextWrapPos();
        ImGui::TextDisabled("TPMS Body meshes the lattice itself. Domain Box creates a full cuboid block mesh for checking the box/domain envelope.");
        ImGui::PopTextWrapPos();

        begin_props_table();
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Target Size");
        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##gs", &state.elem_size, 0.1f, 0.1f, 20.f, "%.2f mm");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Minimum Size");
        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##ms", &state.elem_size_min, 0.05f, 0.05f, 10.f, "%.2f mm");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Maximum Size");
        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##mxs", &state.elem_size_max, 0.1f, 0.1f, 50.f, "%.2f mm");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Growth Rate");
        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##growth", &state.mesh_growth_rate, 1.0f, 2.5f, "%.2f");
        end_props_table();

        props_header("DISPLAY");
        const char* mesh_views[] = { "Solid", "Surface", "Mesh" };
        int mesh_view_idx = static_cast<int>(state.view_display_mode);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("Mesh View Mode", &mesh_view_idx, mesh_views, 3)) {
            state.view_display_mode = static_cast<ViewDisplayMode>(mesh_view_idx);
        }

        if (state.has_surface_mesh || state.has_volume_mesh) {
            props_header("MESH STATISTICS");
            begin_props_table();
            if (state.has_surface_mesh) {
                prop_row("Surf Engine", "%s", state.surf_engine.c_str());
                prop_row("Surf Nodes",  "%d", state.surf_nodes);
                prop_row("Triangles",   "%d", state.surf_tris);
            }
            if (state.has_volume_mesh) {
                prop_row("Vol Engine",  "%s", state.vol_engine.c_str());
                prop_row("Vol Nodes",   "%d", state.vol_nodes);
                prop_row("Tetrahedra",  "%d", state.vol_tets);
            }
            prop_row("Min Quality", "%.3f", state.mesh_quality_min);
            prop_row("Avg Quality", "%.3f", state.mesh_quality_avg);
            prop_row("Max Aspect",  "%.2f", state.mesh_aspect_max);
            end_props_table();

            if (!state.mesh_summary.empty()) {
                ImGui::Spacing();
                ImGui::PushTextWrapPos();
                ImGui::TextDisabled("%s", state.mesh_summary.c_str());
                ImGui::PopTextWrapPos();
            }
        }
        break;
    }

    // ── BCs props ─────────────────────────────────────────────────────────────
    case WorkflowTab::BCs: {
        static char material_name_buf[128] = "Structural Steel";
        static std::string synced_material_name = "Structural Steel";
        if (synced_material_name != state.material.material_name) {
            std::snprintf(material_name_buf, sizeof material_name_buf, "%s", state.material.material_name.c_str());
            synced_material_name = state.material.material_name;
        }

        props_header("MATERIAL LIBRARY");
        const char* materials[] = {
            "Structural Steel", "Stainless Steel", "Aluminium",
            "Titanium", "ABS", "PLA", "PEEK"
        };
        int material_idx = 0;
        for (int i = 0; i < 7; ++i) {
            if (state.material.library_name == materials[i]) { material_idx = i; break; }
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("Library Material", &material_idx, materials, 7)) {
            state.material = preprocess::material_from_library(materials[material_idx]);
            state.has_material = true;
            snprintf(material_name_buf, sizeof material_name_buf, "%s", state.material.material_name.c_str());
            synced_material_name = state.material.material_name;
            invalidate_preprocess();
        }

        props_header("MATERIAL");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("Material Name", material_name_buf, sizeof material_name_buf)) {
            state.material.material_name = material_name_buf;
            synced_material_name = state.material.material_name;
            state.has_material = true;
            invalidate_preprocess();
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("Density (kg/m3)", &state.material.density, 10.f, 1.f, 20000.f, "%.0f")) {
            state.has_material = true;
            invalidate_preprocess();
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("Young's Modulus (MPa)", &state.material.young_modulus, 500.f, 1.f, 1e7f, "%.0f")) {
            state.has_material = true;
            invalidate_preprocess();
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("Poisson Ratio", &state.material.poisson_ratio, 0.01f, 0.f, 0.49f, "%.3f")) {
            state.has_material = true;
            invalidate_preprocess();
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("Yield Strength (MPa)", &state.material.yield_strength, 5.f, 1.f, 5000.f, "%.0f")) {
            state.has_material = true;
            invalidate_preprocess();
        }

        // ── BC target face selector ───────────────────────────────────────────
        props_header("BC TARGET FACE");

        if (!state.face_infos.empty()) {
            // Build combo items from extracted face sets
            static int face_sel = 0;
            // Sync selection to pending_bc_face
            for (int i = 0; i < (int)state.face_infos.size(); ++i) {
                if (state.face_infos[i].label == state.pending_bc_face) {
                    face_sel = i; break;
                }
            }
            std::vector<const char*> face_labels;
            std::vector<char> face_label_bufs(state.face_infos.size() * 64);
            for (int i = 0; i < (int)state.face_infos.size(); ++i) {
                char* buf = face_label_bufs.data() + i * 64;
                const char* label = state.face_infos[i].label.c_str();
                const char* axis = label;
                if (state.face_infos[i].label == "Bottom") axis = "Bottom (Z-min)";
                else if (state.face_infos[i].label == "Top") axis = "Top (Z-max)";
                else if (state.face_infos[i].label == "Back") axis = "Back (Y-min)";
                else if (state.face_infos[i].label == "Front") axis = "Front (Y-max)";
                else if (state.face_infos[i].label == "Left") axis = "Left (X-min)";
                else if (state.face_infos[i].label == "Right") axis = "Right (X-max)";
                std::snprintf(buf, 64, "%s  |  %d nodes", axis, state.face_infos[i].node_count);
                face_labels.push_back(buf);
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##face_sel", &face_sel,
                             face_labels.data(), (int)face_labels.size())) {
                state.pending_bc_face = state.face_infos[face_sel].label;
            }
        } else {
            // Manual entry when no mesh face sets are available
            const char* faces[] = {"Bottom","Top","Back","Front","Left","Right"};
            const char* face_display[] = {
                "Bottom (Z-min)",
                "Top (Z-max)",
                "Back (Y-min)",
                "Front (Y-max)",
                "Left (X-min)",
                "Right (X-max)"
            };
            int fs = 0;
            for (int i = 0; i < 6; ++i)
                if (state.pending_bc_face == faces[i]) { fs = i; break; }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##face_manual", &fs, face_display, 6))
                state.pending_bc_face = faces[fs];
            ImGui::TextDisabled("Generate volume mesh to see node counts.");
        }

        ImGui::TextDisabled("Default MVP setup: Fixed = Bottom (Z-min), Force/Displacement = Top (Z-max).");

        // ── Pending force configuration ───────────────────────────────────────
        props_header("PENDING FORCE (N)");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat3("##pforce", &state.pending_force_fx, 10.f, -1e6f, 1e6f, "%.0f");

        // ── Pending displacement configuration ────────────────────────────────
        props_header("PENDING DISPLACEMENT (mm)");
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat3("##pdisp", &state.pending_disp_ux, 0.01f, -100.f, 100.f, "%.3f");

        ImGui::TextDisabled("Use Boundary Conditions menu actions to apply the default Z-face setup.");

        // ── Assigned fixed supports ───────────────────────────────────────────
        props_header("FIXED SUPPORTS");
        for (int i = 0; i < (int)state.fixed_supports.size(); ++i) {
            ImGui::BulletText("%s", state.fixed_supports[i].face_label.c_str());
            ImGui::SameLine();
            ImGui::PushID(i);
            if (ImGui::SmallButton("x")) {
                state.fixed_supports.erase(state.fixed_supports.begin() + i);
                state.has_fixed_bc = !state.fixed_supports.empty();
                invalidate_preprocess();
                --i;
            }
            ImGui::PopID();
        }
        if (state.fixed_supports.empty()) ImGui::TextDisabled("  none");

        // ── Assigned force loads ──────────────────────────────────────────────
        props_header("FORCE LOADS");
        for (int i = 0; i < (int)state.force_loads.size(); ++i) {
            auto& fl = state.force_loads[i];
            ImGui::BulletText("%s  [%.0f, %.0f, %.0f] N",
                              fl.face_label.c_str(), fl.fx, fl.fy, fl.fz);
            ImGui::SameLine();
            ImGui::PushID(100 + i);
            if (ImGui::SmallButton("x")) {
                state.force_loads.erase(state.force_loads.begin() + i);
                state.has_load_bc = !state.force_loads.empty() || !state.displacement_loads.empty();
                invalidate_preprocess();
                --i;
            }
            ImGui::PopID();
        }
        if (state.force_loads.empty()) ImGui::TextDisabled("  none");

        // ── Assigned displacement loads ───────────────────────────────────────
        props_header("DISPLACEMENT LOADS");
        for (int i = 0; i < (int)state.displacement_loads.size(); ++i) {
            auto& dl = state.displacement_loads[i];
            ImGui::BulletText("%s  [%.3f, %.3f, %.3f] mm",
                              dl.face_label.c_str(), dl.ux, dl.uy, dl.uz);
            ImGui::SameLine();
            ImGui::PushID(200 + i);
            if (ImGui::SmallButton("x")) {
                state.displacement_loads.erase(state.displacement_loads.begin() + i);
                state.has_load_bc = !state.force_loads.empty() || !state.displacement_loads.empty();
                invalidate_preprocess();
                --i;
            }
            ImGui::PopID();
        }
        if (state.displacement_loads.empty()) ImGui::TextDisabled("  none");

        props_header("ANALYSIS SETUP");
        ImGui::TextDisabled("Analysis Type");
        ImGui::SameLine(140);
        ImGui::TextUnformatted(state.analysis_type_name());
        if (state.has_preprocessed_model) {
            ImGui::Spacing();
            props_header("PREPROCESSED MODEL");
            begin_props_table();
            prop_row("DOFs", "%d", state.preprocess_n_dofs);
            prop_row("Constrained", "%d", state.preprocess_n_constrained);
            prop_row("Loaded nodes", "%d", state.preprocess_n_loaded);
            end_props_table();
        }
        break;
    }

    // ── Solve props ───────────────────────────────────────────────────────────
    case WorkflowTab::Solve: {
        props_header("SOLVER SETTINGS");
        ImGui::TextDisabled("Analysis");
        ImGui::SameLine(140);
        ImGui::TextUnformatted(state.analysis_type_name());
        ImGui::SetNextItemWidth(-1);
        ImGui::DragInt("Max Iterations", &state.solver_max_iter, 10, 10, 10000);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputFloat("Tolerance", &state.solver_tol, 0.f, 0.f, "%.2e");

        props_header("VALIDATION");
        begin_props_table();
        prop_row("Mesh",       state.has_volume_mesh ? "OK"   : "Missing");
        prop_row("Material",   state.has_material    ? "OK"   : "Not set");
        prop_row("Fixed BC",   state.has_fixed_bc    ? "OK"   : "Missing");
        prop_row("Load BC",    state.has_load_bc     ? "OK"   : "Missing");
        prop_row("Status",     state.validation_ok   ? "PASS" : "—");
        end_props_table();

        if (state.has_preprocessed_model) {
            props_header("PREPROCESSED MODEL");
            begin_props_table();
            prop_row("DOFs",         "%d", state.preprocess_n_dofs);
            prop_row("Constrained",  "%d", state.preprocess_n_constrained);
            prop_row("Loaded nodes", "%d", state.preprocess_n_loaded);
            end_props_table();
            if (!state.preprocess_summary.empty()) {
                ImGui::Spacing();
                ImGui::PushTextWrapPos();
                ImGui::TextDisabled("%s", state.preprocess_summary.c_str());
                ImGui::PopTextWrapPos();
            }
        }

        props_header("SOLVE STATUS");
        if (state.solver_running) {
            ImGui::ProgressBar(state.solver_progress, ImVec2(-1, 0),
                               state.solver_status_text.empty() ? "Solving..." : state.solver_status_text.c_str());
            ImGui::Spacing();
        }
        begin_props_table();
        prop_row("Run State", state.solver_running ? "Running" : (state.solver_has_run ? "Finished" : "Not run"));
        prop_row("Convergence", state.solver_has_run ? (state.solver_converged ? "Converged" : "Approximate / stopped") : "—");
        prop_row("Current Iter.", "%d", state.solver_current_iteration);
        prop_row("Final Iter.", "%d", state.solver_iterations);
        prop_row("Progress", "%.0f %%", state.solver_progress * 100.0f);
        prop_row("Rel. Residual", "%.3e", state.solver_current_relative_residual);
        prop_row("Initial Resid.", "%.3e", state.solver_initial_residual);
        prop_row("Final Resid.", "%.3e", state.solver_final_residual);
        prop_row("Results", state.has_results ? "Available" : "Not available");
        end_props_table();
        if (!state.solver_status_text.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("%s", state.solver_status_text.c_str());
        }
        if (!state.solver_summary.empty()) {
            ImGui::Spacing();
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("%s", state.solver_summary.c_str());
            ImGui::PopTextWrapPos();
        }
        if (!state.fem_summary.empty()) {
            props_header("FEM CORE");
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("%s", state.fem_summary.c_str());
            ImGui::PopTextWrapPos();
        }
        if (!state.solver_residual_history.empty()) {
            props_header("RESIDUAL HISTORY");
            const int count = (int)state.solver_residual_history.size();
            const float* data = state.solver_residual_history.data();
            ImGui::PlotLines("##residual_history", data, count, 0, nullptr, 0.0f, 1.0f, ImVec2(-1, 90));
            ImGui::TextDisabled("Relative residual, initial = 1.0");
        }
        break;
    }

    // ── Results props ─────────────────────────────────────────────────────────
    case WorkflowTab::Results: {
        props_header("ACTIVE RESULT");
        ImGui::TextUnformatted(state.active_result.empty() ? "None" : state.active_result.c_str());

        if (state.has_results) {
            props_header("AVAILABLE OUTPUTS");
            begin_props_table();
            prop_row("Displacement Total", state.displacement_result_scalars.empty() ? "Missing" : "Ready");
            prop_row("Displacement X", state.displacement_x_result_scalars.empty() ? "Missing" : "Ready");
            prop_row("Displacement Y", state.displacement_y_result_scalars.empty() ? "Missing" : "Ready");
            prop_row("Displacement Z", state.displacement_z_result_scalars.empty() ? "Missing" : "Ready");
            prop_row("Von Mises", state.von_mises_result_scalars.empty() ? "Missing" : "Ready");
            prop_row("Strain", state.strain_result_scalars.empty() ? "Missing" : "Ready");
            prop_row("Reaction Force", state.reaction_force_result_scalars.empty() ? "Missing" : "Ready");
            end_props_table();

            props_header("COLOUR MAP");
            static int cmap = 0;
            const char* cmaps[] = { "Jet", "Viridis", "Hot", "Gray" };
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##cmap", &cmap, cmaps, 4);

            props_header("DEFORMATION");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("Scale", &state.result_deform_scale, 0.1f, 0.f, 100.f, "%.1fx");

            props_header("RANGE");
            begin_props_table();
            prop_row("Minimum", "%.4g %s", state.result_min, state.result_unit.c_str());
            prop_row("Min Node", "%d", state.result_min_node);
            prop_row("Maximum", "%.4g %s", state.result_max, state.result_unit.c_str());
            prop_row("Max Node", "%d", state.result_max_node);
            end_props_table();

            if (!state.result_summary.empty()) {
                ImGui::Spacing();
                ImGui::PushTextWrapPos();
                ImGui::TextDisabled("%s", state.result_summary.c_str());
                ImGui::PopTextWrapPos();
            }
            if (!state.validation_summary.empty()) {
                props_header("HEALTH CHECK");
                ImGui::PushTextWrapPos();
                ImGui::TextDisabled("%s", state.validation_summary.c_str());
                ImGui::PopTextWrapPos();
            }
        }
        break;
    }
    } // switch

    ImGui::End();
}

} // namespace tpms::ui
