#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace tpms {

// ── Enumerations ─────────────────────────────────────────────────────────────

enum class TPMSDesign { Gyroid, Diamond, SchwarzP, Lidinoid, Neovius };
enum class DomainType { Cuboid, Cylinder, Sphere };
enum class GeometryMode { Shell, Solid };
enum class SliceAxis { XY, XZ, YZ };
enum class ViewDisplayMode { Solid, Surface, Mesh };
enum class WorkflowTab { Home, Geometry, Meshing, BCs, Solve, Results };
enum class MeshEngine { GMSH, Netgen, AdvancingFront, MarchingCubes, VoxelTet };
enum class VolumeMeshTarget { TPMSBody, DomainBox };
enum class AnalysisType { LinearStatic };

// ── Log entry ────────────────────────────────────────────────────────────────

struct LogEntry {
    enum class Level { Info, Warning, Error };
    Level       level;
    std::string text;
};

// ── Project state (single source of truth) ───────────────────────────────────

struct ProjectState {
    // Metadata
    std::string project_name   = "Untitled Project";
    bool        dirty          = false;

    // Workflow gating
    bool has_geometry     = false;
    bool has_surface_mesh = false;
    bool has_volume_mesh  = false;
    bool has_material     = false;
    bool has_fixed_bc     = false;
    bool has_load_bc      = false;
    bool validation_ok    = false;
    bool has_results      = false;

    // Active tab
    WorkflowTab active_tab = WorkflowTab::Home;

    // Geometry parameters
    TPMSDesign design    = TPMSDesign::Gyroid;
    DomainType domain    = DomainType::Cuboid;
    GeometryMode geometry_mode = GeometryMode::Shell;
    float size_x = 50.f, size_y = 50.f, size_z = 50.f;
    float cell_x =  5.f, cell_y =  5.f, cell_z =  5.f;
    float origin_x = 0.f, origin_y = 0.f, origin_z = 0.f;
    float wall_thickness = 0.8f;
    int   resolution     = 80;
    SliceAxis slice_axis = SliceAxis::XY;
    float slice_position = 0.5f;
    ViewDisplayMode view_display_mode = ViewDisplayMode::Solid;
    int   field_nx       = 0;
    int   field_ny       = 0;
    int   field_nz       = 0;
    float field_min      = 0.f;
    float field_max      = 0.f;
    int   cell_count_x   = 0;
    int   cell_count_y   = 0;
    int   cell_count_z   = 0;
    float solid_fraction = 0.f;
    std::string geometry_summary;

    // Mesh parameters
    MeshEngine mesh_engine    = MeshEngine::GMSH;
    VolumeMeshTarget volume_mesh_target = VolumeMeshTarget::TPMSBody;
    float      elem_size      = 2.0f;
    float      elem_size_min  = 0.5f;
    float      elem_size_max  = 4.0f;
    float      mesh_growth_rate = 1.2f;
    float      mesh_quality_min = 0.f;
    float      mesh_quality_avg = 0.f;
    float      mesh_aspect_max  = 0.f;
    std::string mesh_summary;

    // Material
    struct MaterialDefinition {
        std::string library_name = "Structural Steel";
        std::string material_name = "Structural Steel";
        float density = 7850.f;
        float young_modulus = 210000.f;   // MPa
        float poisson_ratio = 0.3f;
        float yield_strength = 250.f;     // MPa
    } material;

    // BCs
    struct FixedSupport { std::string face_label; };
    struct ForceLoad    { std::string face_label; float fx=0, fy=0, fz=-1000; };
    struct DisplacementLoad { std::string face_label; float ux=0, uy=0, uz=0; };
    std::vector<FixedSupport> fixed_supports;
    std::vector<ForceLoad>    force_loads;
    std::vector<DisplacementLoad> displacement_loads;

    // Solve
    AnalysisType analysis_type = AnalysisType::LinearStatic;
    int   solver_max_iter = 1000;
    float solver_tol      = 1e-8f;
    bool  solver_has_run  = false;
    bool  solver_running  = false;
    bool  solver_converged = false;
    int   solver_iterations = 0;
    int   solver_current_iteration = 0;
    float solver_progress = 0.f;
    float solver_initial_residual = 0.f;
    float solver_final_residual   = 0.f;
    float solver_current_relative_residual = 0.f;
    std::string solver_status_text;
    std::string solver_summary;
    std::vector<float> solver_residual_history;
    std::string fem_summary;

    // ── Results ───────────────────────────────────────────────────────────────
    std::string active_result;        // "von_mises", "displacement", "strain"
    std::string active_component;     // "ALL", "X", "Y", "Z"
    float       result_deform_scale = 1.0f;
    float       result_min = 0.f, result_max = 0.f;
    int         result_min_node = -1, result_max_node = -1;
    std::string result_unit;          // "MPa", "mm", etc.
    std::string result_summary;
    std::string validation_summary;

    // Field output lists (populated after solve)
    struct FieldOutput {
        std::string group;    // "DISP", "STRESS"
        std::string name;     // "ALL", "U1", "MISES", etc.
        std::string unit;
    };
    std::vector<FieldOutput> field_outputs;

    // Per-node scalar data for current result (Module 6 will fill these)
    std::vector<float> result_scalars;
    std::vector<float> displacement_result_scalars;
    std::vector<float> von_mises_result_scalars;
    std::vector<float> strain_result_scalars;
    std::vector<double> displacement_solution;

    // Mesh stats (filled after generation)
    int surf_nodes = 0, surf_tris = 0;
    int vol_nodes  = 0, vol_tets  = 0;
    std::string surf_engine, vol_engine;

    // Field + slice preview texture (owned by app, not serialised)
    // Using void* to avoid including geometry headers here; app casts as needed.
    void* field_data         = nullptr;  // geometry::FieldData*
    void* slice_texture      = nullptr;  // geometry::FieldTexture*
    void* surface_mesh_data  = nullptr;  // geometry::SurfaceMeshData*
    void* volume_mesh_data   = nullptr;  // geometry::VolumeMeshData*
    void* preprocessed_model = nullptr;  // preprocess::PreprocessedModel*

    // Preprocessed model stats (shown in UI without casting void*)
    bool        has_preprocessed_model = false;
    int         preprocess_n_dofs         = 0;
    int         preprocess_n_constrained  = 0;
    int         preprocess_n_loaded       = 0;
    std::string preprocess_summary;

    // Face node sets (label + count) for BC-face selector UI
    struct FaceInfo { std::string label; int node_count = 0; };
    std::vector<FaceInfo> face_infos;

    // Pending BC configuration (set in props panel, consumed by ribbon actions)
    std::string pending_bc_face   = "Bottom";
    float pending_force_fx  = 0.f;
    float pending_force_fy  = 0.f;
    float pending_force_fz  = -1000.f;
    float pending_disp_ux   = 0.f;
    float pending_disp_uy   = 0.f;
    float pending_disp_uz   = -0.5f;

    // Log
    std::vector<LogEntry> log;

    // ── Gating queries ────────────────────────────────────────────────────────
    bool can_preview()             const { return true; }
    bool can_surface_mesh()        const { return has_geometry; }
    bool can_volume_mesh()         const { return has_geometry && (volume_mesh_target == VolumeMeshTarget::DomainBox || has_surface_mesh); }
    bool can_assign_bc()           const { return has_volume_mesh; }
    bool can_validate()            const { return has_volume_mesh; }
    bool can_solve()               const { return validation_ok && !solver_running; }
    bool can_show_results()        const { return has_results; }

    void log_info (const std::string& msg) { log.push_back({LogEntry::Level::Info,    msg}); }
    void log_warn (const std::string& msg) { log.push_back({LogEntry::Level::Warning, msg}); }
    void log_error(const std::string& msg) { log.push_back({LogEntry::Level::Error,   msg}); }

    void reset() {
        *this = ProjectState{};
    }

    // ── Design name helpers ───────────────────────────────────────────────────
    static const char* design_name(TPMSDesign d) {
        switch (d) {
            case TPMSDesign::Gyroid:   return "Gyroid";
            case TPMSDesign::Diamond:  return "Diamond";
            case TPMSDesign::SchwarzP: return "Schwarz-P";
            case TPMSDesign::Lidinoid: return "Lidinoid";
            case TPMSDesign::Neovius:  return "Neovius";
        }
        return "Unknown";
    }
    const char* design_name() const { return design_name(design); }

    static const char* domain_name(DomainType d) {
        switch (d) {
            case DomainType::Cuboid:   return "Cuboid";
            case DomainType::Cylinder: return "Cylinder";
            case DomainType::Sphere:   return "Sphere";
        }
        return "Unknown";
    }
    const char* domain_name() const { return domain_name(domain); }

    static const char* geometry_mode_name(GeometryMode mode) {
        switch (mode) {
            case GeometryMode::Shell: return "Shell";
            case GeometryMode::Solid: return "Solid";
        }
        return "Unknown";
    }
    const char* geometry_mode_name() const { return geometry_mode_name(geometry_mode); }

    static const char* slice_axis_name(SliceAxis axis) {
        switch (axis) {
            case SliceAxis::XY: return "XY";
            case SliceAxis::XZ: return "XZ";
            case SliceAxis::YZ: return "YZ";
        }
        return "Unknown";
    }
    const char* slice_axis_name() const { return slice_axis_name(slice_axis); }

    static const char* view_display_mode_name(ViewDisplayMode mode) {
        switch (mode) {
            case ViewDisplayMode::Solid: return "Solid";
            case ViewDisplayMode::Surface: return "Surface";
            case ViewDisplayMode::Mesh: return "Mesh";
        }
        return "Unknown";
    }
    const char* view_display_mode_name() const { return view_display_mode_name(view_display_mode); }

    static const char* analysis_type_name(AnalysisType type) {
        switch (type) {
            case AnalysisType::LinearStatic: return "Linear Static";
        }
        return "Unknown";
    }
    const char* analysis_type_name() const { return analysis_type_name(analysis_type); }

    static const char* mesh_engine_name(MeshEngine engine) {
        switch (engine) {
            case MeshEngine::GMSH: return "GMSH";
            case MeshEngine::Netgen: return "Netgen";
            case MeshEngine::AdvancingFront: return "Advancing Front";
            case MeshEngine::MarchingCubes: return "Marching Cubes";
            case MeshEngine::VoxelTet: return "Voxel Tet";
        }
        return "Unknown";
    }
    const char* mesh_engine_name() const { return mesh_engine_name(mesh_engine); }

    static const char* volume_mesh_target_name(VolumeMeshTarget target) {
        switch (target) {
            case VolumeMeshTarget::TPMSBody:  return "TPMS Body";
            case VolumeMeshTarget::DomainBox: return "Domain Box";
        }
        return "Unknown";
    }
    const char* volume_mesh_target_name() const { return volume_mesh_target_name(volume_mesh_target); }
};

} // namespace tpms
