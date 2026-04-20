#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <array>
#include <future>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

// GLFW + OpenGL
#include <GLFW/glfw3.h>

// Dear ImGui
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// App modules
#include "state/project_state.hpp"
#include "geometry/geometry_engine.hpp"
#include "geometry/meshing_engine.hpp"
#include "geometry/tpms_field.hpp"
#include "geometry/field_texture.hpp"
#include "preprocess/preprocess_engine.hpp"
#include "solve/linear_solver.hpp"
#include "fem/fem_core.hpp"
#include "io/geometry_io.hpp"
#include "io/project_io.hpp"
#include "io/file_dialog.hpp"
#include "io/report_io.hpp"
#include "validation/validation_core.hpp"
#include "ui/theme.hpp"
#include "ui/layout.hpp"
#include "ui/menu_bar.hpp"
#include "ui/tree_panel.hpp"
#include "ui/props_panel.hpp"
#include "ui/viewport_panel.hpp"
#include "ui/console_panel.hpp"
#include "ui/status_bar.hpp"

// ── Window title helpers ──────────────────────────────────────────────────────
static void update_title(GLFWwindow* win, const tpms::ProjectState& state) {
    char buf[256];
    snprintf(buf, sizeof buf, "TPMS Studio v0.1 — %s%s",
             state.project_name.c_str(),
             state.dirty ? " *" : "");
    glfwSetWindowTitle(win, buf);
}

// ── GLFW error callback ───────────────────────────────────────────────────────
static void glfw_error(int code, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

struct AsyncSolveOutput {
    tpms::solve::LinearSolveResult solve;
    tpms::fem::StressRecoveryResult stress;
};

struct SolverRuntime {
    bool active = false;
    std::future<AsyncSolveOutput> future;
    std::shared_ptr<std::atomic<float>> progress;
    std::shared_ptr<std::atomic<int>> iteration;
    std::shared_ptr<std::atomic<double>> relative_residual;
};

static SolverRuntime g_solver_runtime;

static void clear_geometry_preview(tpms::ProjectState& state) {
    auto* field        = static_cast<tpms::geometry::FieldData*>(state.field_data);
    auto* tex          = static_cast<tpms::geometry::FieldTexture*>(state.slice_texture);
    auto* surface_mesh = static_cast<tpms::geometry::SurfaceMeshData*>(state.surface_mesh_data);
    auto* volume_mesh  = static_cast<tpms::geometry::VolumeMeshData*>(state.volume_mesh_data);
    auto* pm           = static_cast<tpms::preprocess::PreprocessedModel*>(state.preprocessed_model);

    if (tex)          { tex->free(); delete tex; }
    if (field)          delete field;
    if (surface_mesh)   delete surface_mesh;
    if (volume_mesh)    delete volume_mesh;
    if (pm)             delete pm;

    state.field_data         = nullptr;
    state.slice_texture      = nullptr;
    state.surface_mesh_data  = nullptr;
    state.volume_mesh_data   = nullptr;
    state.preprocessed_model = nullptr;

    state.field_nx = 0; state.field_ny = 0; state.field_nz = 0;
    state.field_min = 0.f; state.field_max = 0.f;
    state.cell_count_x = 0; state.cell_count_y = 0; state.cell_count_z = 0;
    state.solid_fraction = 0.f;
    state.geometry_summary.clear();
    state.mesh_quality_min = 0.f; state.mesh_quality_avg = 0.f; state.mesh_aspect_max = 0.f;
    state.mesh_summary.clear();

    state.has_preprocessed_model  = false;
    state.preprocess_n_dofs        = 0;
    state.preprocess_n_constrained = 0;
    state.preprocess_n_loaded      = 0;
    state.preprocess_summary.clear();
    state.face_infos.clear();

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
    state.result_min_node = -1;
    state.result_max_node = -1;
    state.result_summary.clear();
    state.validation_summary.clear();
    state.result_scalars.clear();
    state.displacement_result_scalars.clear();
    state.von_mises_result_scalars.clear();
    state.strain_result_scalars.clear();
    state.displacement_solution.clear();
    state.field_outputs.clear();
}

static void clear_solver_results(tpms::ProjectState& state);

static void set_active_scalar_result(
    tpms::ProjectState& state,
    const std::vector<float>& scalars,
    const std::string& name,
    const std::string& component,
    const std::string& unit
) {
    state.result_scalars = scalars;
    if (!state.result_scalars.empty()) {
        auto [min_it, max_it] = std::minmax_element(
            state.result_scalars.begin(), state.result_scalars.end());
        state.result_min = *min_it;
        state.result_max = *max_it;
        state.result_min_node = (int)std::distance(state.result_scalars.begin(), min_it);
        state.result_max_node = (int)std::distance(state.result_scalars.begin(), max_it);
    } else {
        state.result_min = 0.f;
        state.result_max = 0.f;
        state.result_min_node = -1;
        state.result_max_node = -1;
    }
    state.active_result = name;
    state.active_component = component;
    state.result_unit = unit;
    char buf[192];
    std::snprintf(buf, sizeof buf,
        "%s %s range %.4g to %.4g %s",
        name.c_str(), component.c_str(), state.result_min, state.result_max, unit.c_str());
    state.result_summary = buf;
}

static void clear_preprocessed_model(tpms::ProjectState& state) {
    auto* pm = static_cast<tpms::preprocess::PreprocessedModel*>(state.preprocessed_model);
    if (pm) delete pm;
    state.preprocessed_model = nullptr;
    state.has_preprocessed_model = false;
    state.preprocess_n_dofs = 0;
    state.preprocess_n_constrained = 0;
    state.preprocess_n_loaded = 0;
    state.preprocess_summary.clear();
    state.validation_ok = false;
    clear_solver_results(state);
}

static void clear_solver_results(tpms::ProjectState& state) {
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

    state.has_results = false;
    state.active_result.clear();
    state.active_component.clear();
    state.result_deform_scale = 1.0f;
    state.result_min = 0.f;
    state.result_max = 0.f;
    state.result_min_node = -1;
    state.result_max_node = -1;
    state.result_unit.clear();
    state.result_summary.clear();
    state.validation_summary.clear();
    state.field_outputs.clear();
    state.result_scalars.clear();
    state.displacement_result_scalars.clear();
    state.von_mises_result_scalars.clear();
    state.strain_result_scalars.clear();
    state.displacement_solution.clear();
    state.fem_summary.clear();
}

static void apply_solve_output(tpms::ProjectState& state, const AsyncSolveOutput& output) {
    const auto& result = output.solve;
    const auto& stress = output.stress;

    state.solver_running = false;
    state.solver_has_run = true;
    state.solver_converged = result.converged;
    state.solver_iterations = result.iterations;
    state.solver_current_iteration = result.iterations;
    state.solver_progress = 1.0f;
    state.solver_initial_residual = static_cast<float>(result.initial_residual);
    state.solver_final_residual = static_cast<float>(result.final_residual);
    state.solver_current_relative_residual =
        result.initial_residual > 0.0
            ? static_cast<float>(result.final_residual / result.initial_residual)
            : 0.f;
    state.solver_summary = result.message;
    state.solver_status_text = result.converged ? "Solve converged" : "Solve finished with warning";
    state.solver_residual_history = result.residual_history;

    if (!result.ok || result.displacement_magnitude.empty()) {
        state.log_error("Solve failed: " + result.message);
        state.solver_status_text = "Solve failed";
        return;
    }

    state.displacement_solution = result.displacement;
    state.displacement_result_scalars = result.displacement_magnitude;
    set_active_scalar_result(state, state.displacement_result_scalars,
                             "Total Displacement", "ALL", "mm");
    state.field_outputs.clear();
    state.field_outputs.push_back({"DISP", "Total Displacement", "mm"});

    if (stress.ok && !stress.nodal_von_mises.empty()) {
        state.von_mises_result_scalars = stress.nodal_von_mises;
        state.strain_result_scalars = stress.nodal_equivalent_strain;
        state.field_outputs.push_back({"STRESS", "Von Mises", "MPa"});
        state.field_outputs.push_back({"STRAIN", "Equivalent Strain", ""});
        state.fem_summary = stress.summary;
        state.log_info("Module 6 FEM core complete: " + stress.summary);
    } else {
        state.fem_summary = stress.summary;
        state.log_warn("Module 6 stress recovery unavailable: " + stress.summary);
    }

    state.has_results = true;
    state.active_tab = tpms::WorkflowTab::Results;
    state.dirty = true;

    if (result.converged) {
        state.log_info("Solve complete: " + result.message);
    } else {
        state.log_warn("Solve produced an approximate result: " + result.message);
    }
}

static void poll_solver_runtime(tpms::ProjectState& state) {
    if (!g_solver_runtime.active) return;

    if (g_solver_runtime.progress) {
        state.solver_progress = std::clamp(g_solver_runtime.progress->load(), 0.0f, 0.99f);
    }
    if (g_solver_runtime.iteration) {
        state.solver_current_iteration = g_solver_runtime.iteration->load();
    }
    if (g_solver_runtime.relative_residual) {
        state.solver_current_relative_residual =
            static_cast<float>(g_solver_runtime.relative_residual->load());
    }

    const auto ready = g_solver_runtime.future.wait_for(std::chrono::milliseconds(0));
    if (ready != std::future_status::ready) return;

    const auto output = g_solver_runtime.future.get();
    g_solver_runtime.active = false;
    apply_solve_output(state, output);
}

static int slice_index_for_state(const tpms::ProjectState& state, const tpms::geometry::FieldData& field) {
    const float t = std::clamp(state.slice_position, 0.0f, 1.0f);
    switch (state.slice_axis) {
        case tpms::SliceAxis::XY: return (int)std::lround(t * (field.nz - 1));
        case tpms::SliceAxis::XZ: return (int)std::lround(t * (field.ny - 1));
        case tpms::SliceAxis::YZ: return (int)std::lround(t * (field.nx - 1));
    }
    return field.nz / 2;
}

static void configure_fonts(ImGuiIO& io) {
    const std::array<const char*, 8> font_candidates = {
        "assets/fonts/Inter-Regular.ttf",
        "/usr/share/fonts/truetype/inter/Inter-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
    };

    ImFontConfig cfg{};
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = false;
    cfg.SizePixels = 15.0f;

    ImFont* main_font = nullptr;
    for (const char* path : font_candidates) {
        main_font = io.Fonts->AddFontFromFileTTF(path, 15.0f, &cfg);
        if (main_font) break;
    }

    if (!main_font) {
        main_font = io.Fonts->AddFontDefault();
    }

    io.FontDefault = main_font;
}

// ── Action handler ────────────────────────────────────────────────────────────
static void handle_action(tpms::ui::RibbonAction action, tpms::ProjectState& state, bool& request_close) {
    using A = tpms::ui::RibbonAction;
    if (g_solver_runtime.active &&
        action.type != A::None &&
        action.type != A::ShowVonMises &&
        action.type != A::ShowDisplacement &&
        action.type != A::ShowStrain) {
        state.log_warn("Solver is running. Wait for completion before changing the model or exporting.");
        return;
    }
    switch (action.type) {
    case A::None: break;

    case A::NewProject:
        clear_geometry_preview(state);
        state.reset();
        state.log_info("New project created.");
        break;

    case A::SaveProject: {
        auto path = tpms::io::save_dialog("Save Project", state.project_name + ".tpms", "*.tpms");
        if (path.empty()) break;
        auto r = tpms::io::save_project(state, path);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        if (r.ok) state.dirty = false;
        break;
    }

    case A::OpenProject: {
        auto path = tpms::io::open_dialog("Open Project", "*.tpms");
        if (path.empty()) break;
        clear_geometry_preview(state);
        auto r = tpms::io::load_project(state, path);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        break;
    }

    case A::ExportMesh: {
        auto* vm = static_cast<tpms::geometry::VolumeMeshData*>(state.volume_mesh_data);
        if (!vm) { state.log_error("No volume mesh to export."); break; }
        auto path = tpms::io::save_dialog("Export Volume Mesh (VTK)", state.project_name + ".vtk", "*.vtk");
        if (path.empty()) break;
        auto r = tpms::io::export_vtk_ugrid(*vm, path);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        break;
    }

    case A::ExportActiveResult: {
        auto* vm = static_cast<tpms::geometry::VolumeMeshData*>(state.volume_mesh_data);
        if (!vm) { state.log_error("No volume mesh to export result on."); break; }
        if (!state.has_results || state.result_scalars.empty()) {
            state.log_error("No active result to export.");
            break;
        }
        auto path = tpms::io::save_dialog("Export Active Result (VTK)", state.project_name + "_result.vtk", "*.vtk");
        if (path.empty()) break;
        auto r = tpms::io::export_vtk_ugrid_with_point_scalar(
            *vm, state.result_scalars, state.active_result, path);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        break;
    }

    case A::ExportReport: {
        auto path = tpms::io::save_dialog("Export Analysis Report", state.project_name + "_report.md", "*.md");
        if (path.empty()) break;
        auto r = tpms::io::export_markdown_report(state, path);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        break;
    }

    case A::RunHealthCheck: {
        const auto report = tpms::validation::run_model_health_checks(state);
        state.validation_summary = report.summary;
        for (const auto& line : report.lines) {
            state.validation_summary += "\n" + line;
            if (line.rfind("[FAIL]", 0) == 0) state.log_error(line);
            else if (line.rfind("[WARN]", 0) == 0) state.log_warn(line);
            else state.log_info(line);
        }
        report.ok ? state.log_info(report.summary) : state.log_error(report.summary);
        break;
    }

    case A::ExportSTLBinary: {
        auto* surf = static_cast<tpms::geometry::SurfaceMeshData*>(state.surface_mesh_data);
        if (!surf) { state.log_error("No surface mesh to export."); break; }
        auto path = tpms::io::save_dialog("Export Binary STL", state.project_name + ".stl", "*.stl");
        if (path.empty()) break;
        auto r = tpms::io::export_stl_binary(*surf, path);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        break;
    }

    case A::ExportSTLAscii: {
        auto* surf = static_cast<tpms::geometry::SurfaceMeshData*>(state.surface_mesh_data);
        if (!surf) { state.log_error("No surface mesh to export."); break; }
        auto path = tpms::io::save_dialog("Export ASCII STL", state.project_name + ".stl", "*.stl");
        if (path.empty()) break;
        auto r = tpms::io::export_stl_ascii(*surf, path);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        break;
    }

    case A::ExportOBJ: {
        auto* surf = static_cast<tpms::geometry::SurfaceMeshData*>(state.surface_mesh_data);
        if (!surf) { state.log_error("No surface mesh to export."); break; }
        auto path = tpms::io::save_dialog("Export OBJ", state.project_name + ".obj", "*.obj");
        if (path.empty()) break;
        auto r = tpms::io::export_obj(*surf, path, state.project_name);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        break;
    }

    case A::ExportPLY: {
        auto* surf = static_cast<tpms::geometry::SurfaceMeshData*>(state.surface_mesh_data);
        if (!surf) { state.log_error("No surface mesh to export."); break; }
        auto path = tpms::io::save_dialog("Export PLY", state.project_name + ".ply", "*.ply");
        if (path.empty()) break;
        auto r = tpms::io::export_ply(*surf, path);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        break;
    }

    case A::ExportFieldCSV: {
        auto* field = static_cast<tpms::geometry::FieldData*>(state.field_data);
        if (!field) { state.log_error("No field data to export."); break; }
        auto path = tpms::io::save_dialog("Export Field Slice CSV", state.project_name + "_slice.csv", "*.csv");
        if (path.empty()) break;
        int idx = slice_index_for_state(state, *field);
        auto r = tpms::io::export_field_slice_csv(*field, state.slice_axis, idx, path);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        break;
    }

    case A::ExportFieldRaw: {
        auto* field = static_cast<tpms::geometry::FieldData*>(state.field_data);
        if (!field) { state.log_error("No field data to export."); break; }
        auto path = tpms::io::save_dialog("Export Field Raw Binary", state.project_name + "_field.raw", "*.raw");
        if (path.empty()) break;
        auto r = tpms::io::export_field_raw(*field, path);
        r.ok ? state.log_info(r.message) : state.log_error(r.message);
        break;
    }

    case A::ExitApp:
        request_close = true;
        break;

    case A::GenerateGeometry:
        clear_geometry_preview(state);
        {
            const auto result = tpms::geometry::build_geometry(state, 0);
            for (const auto& warning : result.validation.warnings) {
                state.log_warn(warning);
            }
            if (!result.validation.ok) {
                for (const auto& error : result.validation.errors) {
                    state.log_error(error);
                }
                break;
            }

            auto* field = new tpms::geometry::FieldData(result.field);
            auto* tex = new tpms::geometry::FieldTexture();
            tex->upload_slice(*field, state.slice_axis, slice_index_for_state(state, *field));
            state.field_data = field;
            state.slice_texture = tex;
            state.field_nx = field->nx;
            state.field_ny = field->ny;
            state.field_nz = field->nz;
            state.field_min = result.field_min;
            state.field_max = result.field_max;
            state.cell_count_x = result.cell_count_x;
            state.cell_count_y = result.cell_count_y;
            state.cell_count_z = result.cell_count_z;
            state.solid_fraction = result.solid_fraction;
            state.geometry_summary = result.summary;
        }
        state.has_geometry = true;
        state.has_surface_mesh = false;
        state.has_volume_mesh = false;
        state.has_fixed_bc = false;
        state.has_load_bc = false;
        state.has_material = false;
        state.validation_ok = false;
        state.has_results = false;
        state.fixed_supports.clear();
        state.force_loads.clear();
        state.displacement_loads.clear();
        state.view_display_mode = tpms::ViewDisplayMode::Solid;
        state.dirty        = true;
        state.log_info("Geometry generated through Module 2 geometry engine.");
        if (!state.geometry_summary.empty()) {
            state.log_info(state.geometry_summary);
        }
        break;

    case A::GenerateSurfaceMesh:
        {
        auto* field = static_cast<tpms::geometry::FieldData*>(state.field_data);
        const auto result = tpms::geometry::generate_surface_mesh(state, field);
        for (const auto& warning : result.validation.warnings) {
            state.log_warn(warning);
        }
        if (!result.validation.ok) {
            for (const auto& error : result.validation.errors) {
                state.log_error(error);
            }
            break;
        }
        state.has_surface_mesh = true;
        state.surf_engine      = result.engine_name;
        state.surf_nodes       = result.node_count;
        state.surf_tris        = result.triangle_count;
        if (state.surface_mesh_data) delete static_cast<tpms::geometry::SurfaceMeshData*>(state.surface_mesh_data);
        if (state.volume_mesh_data) delete static_cast<tpms::geometry::VolumeMeshData*>(state.volume_mesh_data);
        clear_preprocessed_model(state);
        state.surface_mesh_data = new tpms::geometry::SurfaceMeshData(std::move(result.mesh));
        state.volume_mesh_data = nullptr;
        state.mesh_quality_min = result.min_quality;
        state.mesh_quality_avg = result.avg_quality;
        state.mesh_aspect_max  = result.max_aspect_ratio;
        state.mesh_summary     = result.summary;
        state.has_volume_mesh  = false;
        state.vol_engine.clear();
        state.vol_nodes = 0;
        state.vol_tets = 0;
        state.face_infos.clear();
        state.view_display_mode = tpms::ViewDisplayMode::Mesh;
        state.dirty            = true;
        state.log_info("Surface mesh generated through Module 3 meshing engine.");
        state.log_info(result.summary);
        break;
        }

    case A::GenerateVolumeMesh:
        {
        auto* field = static_cast<tpms::geometry::FieldData*>(state.field_data);
        const auto result = tpms::geometry::generate_volume_mesh(state, field);
        for (const auto& warning : result.validation.warnings) {
            state.log_warn(warning);
        }
        if (!result.validation.ok) {
            for (const auto& error : result.validation.errors) {
                state.log_error(error);
            }
            break;
        }
        state.has_volume_mesh = true;
        state.vol_engine      = result.engine_name;
        state.vol_nodes       = result.node_count;
        state.vol_tets        = result.tet_count;
        if (state.volume_mesh_data) delete static_cast<tpms::geometry::VolumeMeshData*>(state.volume_mesh_data);
        clear_preprocessed_model(state);
        state.volume_mesh_data = new tpms::geometry::VolumeMeshData(std::move(result.mesh));
        state.mesh_quality_min = result.min_quality;
        state.mesh_quality_avg = result.avg_quality;
        state.mesh_aspect_max  = result.max_aspect_ratio;
        state.mesh_summary     = result.summary;
        state.view_display_mode = tpms::ViewDisplayMode::Mesh;
        state.dirty           = true;

        // Extract face node sets so the BC-face selector can show node counts
        {
            auto* vm = static_cast<tpms::geometry::VolumeMeshData*>(state.volume_mesh_data);
            auto face_sets = tpms::preprocess::extract_face_sets(*vm);
            state.face_infos.clear();
            for (const auto& fs : face_sets)
                state.face_infos.push_back({fs.label, (int)fs.node_ids.size()});
        }

        state.log_info("Volume mesh generated through Module 3 meshing engine.");
        state.log_info(result.summary);
        break;
        }

    case A::AssignMaterial:
        {
        auto material = tpms::preprocess::material_from_library(state.material.library_name);
        material.material_name = state.material.material_name.empty() ? material.material_name : state.material.material_name;
        material.density = state.material.density > 0.f ? state.material.density : material.density;
        material.young_modulus = state.material.young_modulus > 0.f ? state.material.young_modulus : material.young_modulus;
        material.poisson_ratio = state.material.poisson_ratio;
        material.yield_strength = state.material.yield_strength > 0.f ? state.material.yield_strength : material.yield_strength;
        tpms::preprocess::assign_material(state, material);
        clear_preprocessed_model(state);
        state.dirty = true;
        state.log_info("Material assigned through Module 4 preprocessing engine.");
        break;
        }

    case A::AddFixedSupport:
        state.pending_bc_face = "Bottom";
        tpms::preprocess::add_fixed_support(state, state.pending_bc_face);
        clear_preprocessed_model(state);
        state.dirty = true;
        state.log_info("Fixed support added to Bottom (Z-min) face.");
        break;

    case A::AddForceLoad:
        state.pending_bc_face = "Top";
        tpms::preprocess::add_force_load(state, {
            state.pending_bc_face,
            state.pending_force_fx,
            state.pending_force_fy,
            state.pending_force_fz
        });
        clear_preprocessed_model(state);
        state.dirty = true;
        {
            char buf[128];
            std::snprintf(buf, sizeof buf, "Force load [%.0f, %.0f, %.0f] N added to Top (Z-max) face.",
                state.pending_force_fx, state.pending_force_fy, state.pending_force_fz);
            state.log_info(buf);
        }
        break;

    case A::AddDisplacementLoad:
        state.pending_bc_face = "Top";
        tpms::preprocess::add_displacement_load(state, {
            state.pending_bc_face,
            state.pending_disp_ux,
            state.pending_disp_uy,
            state.pending_disp_uz
        });
        clear_preprocessed_model(state);
        state.dirty = true;
        {
            char buf[128];
            std::snprintf(buf, sizeof buf, "Displacement load [%.3f, %.3f, %.3f] mm added to Top (Z-max) face.",
                state.pending_disp_ux, state.pending_disp_uy, state.pending_disp_uz);
            state.log_info(buf);
        }
        break;

    case A::Validate:
        {
        const auto report = tpms::preprocess::validate_preprocessing(state);
        for (const auto& warning : report.warnings) state.log_warn(warning);
        if (!report.ok) {
            for (const auto& error : report.errors) state.log_error(error);
            break;
        }

        // Assemble global K, f and apply BCs
        auto* vm = static_cast<tpms::geometry::VolumeMeshData*>(state.volume_mesh_data);
        if (!vm) { state.log_error("Volume mesh not available."); break; }

        auto* old_pm = static_cast<tpms::preprocess::PreprocessedModel*>(state.preprocessed_model);
        if (old_pm) delete old_pm;

        auto* pm = new tpms::preprocess::PreprocessedModel(
            tpms::preprocess::build_preprocessed_model(state, *vm)
        );
        state.preprocessed_model      = pm;
        state.has_preprocessed_model  = pm->valid;
        state.preprocess_summary      = pm->summary;
        state.preprocess_n_dofs        = pm->n_dofs;
        state.preprocess_n_constrained = pm->n_constrained_dofs;
        state.preprocess_n_loaded      = pm->n_loaded_nodes;
        state.validation_ok            = pm->valid;

        if (pm->valid) {
            state.log_info("Module 4 preprocessing complete: " + pm->summary);
        } else {
            state.log_error("Preprocessing failed — check BCs match mesh face labels.");
        }
        break;
        }

    case A::Solve:
        {
        if (g_solver_runtime.active || state.solver_running) {
            state.log_warn("Solver is already running.");
            break;
        }
        if (!state.validation_ok) { state.log_error("Run validation first."); break; }
        auto* pm = static_cast<tpms::preprocess::PreprocessedModel*>(state.preprocessed_model);
        auto* vm = static_cast<tpms::geometry::VolumeMeshData*>(state.volume_mesh_data);
        if (!pm || !pm->valid) { state.log_error("Validated FEM model not available."); break; }
        if (!vm) { state.log_error("Volume mesh not available for result mapping."); break; }

        const tpms::preprocess::PreprocessedModel model_copy = *pm;
        const tpms::geometry::VolumeMeshData mesh_copy = *vm;
        const int max_iter = state.solver_max_iter;
        const double tol = static_cast<double>(state.solver_tol);

        clear_solver_results(state);
        state.solver_running = true;
        state.solver_has_run = true;
        state.solver_progress = 0.01f;
        state.solver_current_iteration = 0;
        state.solver_current_relative_residual = 1.0f;
        state.solver_status_text = "Running linear static solver...";
        state.active_tab = tpms::WorkflowTab::Solve;
        state.log_info("Module 5 solve started in background: Linear Static.");

        g_solver_runtime.active = true;
        g_solver_runtime.progress = std::make_shared<std::atomic<float>>(0.01f);
        g_solver_runtime.iteration = std::make_shared<std::atomic<int>>(0);
        g_solver_runtime.relative_residual = std::make_shared<std::atomic<double>>(1.0);

        auto progress = g_solver_runtime.progress;
        auto iteration = g_solver_runtime.iteration;
        auto relative_residual = g_solver_runtime.relative_residual;

        g_solver_runtime.future = std::async(
            std::launch::async,
            [model_copy, mesh_copy, max_iter, tol, progress, iteration, relative_residual]() mutable {
                AsyncSolveOutput out;
                out.solve = tpms::solve::solve_linear_static(
                    model_copy,
                    max_iter,
                    tol,
                    [progress, iteration, relative_residual, max_iter](int it, double rel) {
                        iteration->store(it);
                        relative_residual->store(std::isfinite(rel) ? rel : 1.0);
                        const float p = max_iter > 0
                            ? std::clamp(static_cast<float>(it) / static_cast<float>(max_iter), 0.01f, 0.95f)
                            : 0.95f;
                        progress->store(p);
                    }
                );

                progress->store(0.97f);
                if (out.solve.ok && !out.solve.displacement.empty()) {
                    out.stress = tpms::fem::recover_tet4_von_mises(
                        mesh_copy,
                        out.solve.displacement,
                        model_copy.E,
                        model_copy.nu
                    );
                }
                progress->store(1.0f);
                return out;
            });
        break;
        }

    case A::ShowVonMises:
        if (!state.von_mises_result_scalars.empty()) {
            set_active_scalar_result(state, state.von_mises_result_scalars,
                                     "Von Mises Stress", "MISES", "MPa");
            state.log_info("Displaying Von Mises stress.");
        } else {
            state.log_error("No Von Mises stress available. Run Solve after validating the model.");
        }
        break;
    case A::ShowDisplacement:
        if (!state.displacement_result_scalars.empty()) {
            set_active_scalar_result(state, state.displacement_result_scalars,
                                     "Total Displacement", "ALL", "mm");
            state.log_info("Displaying total displacement.");
        } else {
            state.log_error("No displacement result available. Run Solve first.");
        }
        break;
    case A::ShowStrain:
        if (!state.strain_result_scalars.empty()) {
            set_active_scalar_result(state, state.strain_result_scalars,
                                     "Equivalent Strain", "EQV", "");
            state.log_info("Displaying equivalent strain.");
        } else {
            state.log_error("No strain result available. Run Solve after validating the model.");
        }
        break;
    }
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    glfwSetErrorCallback(glfw_error);
    if (!glfwInit()) return 1;

    // OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);   // 4× MSAA
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    int win_w = static_cast<int>(mode->width  * 0.85);
    int win_h = static_cast<int>(mode->height * 0.85);

    GLFWwindow* window = glfwCreateWindow(win_w, win_h,
        "TPMS Studio v0.1 — Untitled Project", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // vsync

    // ── ImGui setup ───────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // ViewportsEnable disabled: creates duplicate floating windows in single-window CAE apps
    io.IniFilename = "tpms_studio.ini";

    tpms::ui::apply_theme();

    configure_fonts(io);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // ── Application state ─────────────────────────────────────────────────────
    tpms::ProjectState      state;
    tpms::ui::ViewportPanel viewport;
    tpms::ui::ConsolePanel  console;
    bool request_close = false;

    viewport.init_gl();
    state.log_info("TPMS Studio v0.1 initialised.  H2one Cleantech Private Limited.");
    state.log_info("Ready. Select a tab to begin.");

    // ── Render loop ───────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto menu_action = tpms::ui::draw_main_menu_bar(state);
        if (menu_action.type != tpms::ui::RibbonAction::None)
            handle_action(menu_action, state, request_close);

        poll_solver_runtime(state);

        // Full-window dockspace
        tpms::ui::begin_dockspace();

        // ── Panels — pin to dock slots on first launch ────────────────────────
        auto& dids = tpms::ui::get_dock_ids();
        tpms::ui::pin_window("Model Tree",                       dids.left);
        tpms::ui::draw_tree_panel(state);
        tpms::ui::pin_window("3D Graphics Window",               dids.centre);
        viewport.draw(state);
        tpms::ui::pin_window("Properties",                       dids.right);
        tpms::ui::draw_props_panel(state);
        tpms::ui::pin_window("Messages / Solver Log / Progress", dids.bottom);
        console.draw(state);

        // ── Status bar (overlay, not docked) ─────────────────────────────────
        tpms::ui::draw_status_bar(state);

        // Render
        ImGui::Render();

        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.04f, 0.05f, 0.06f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        // Update window title when dirty state changes
        update_title(window, state);
        if (request_close) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    clear_geometry_preview(state);
    viewport.cleanup();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
