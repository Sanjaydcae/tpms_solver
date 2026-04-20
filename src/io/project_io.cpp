#include "project_io.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace tpms::io {

// ── Helpers ───────────────────────────────────────────────────────────────────

static const char* design_str(TPMSDesign d) {
    switch (d) {
        case TPMSDesign::Gyroid:   return "Gyroid";
        case TPMSDesign::Diamond:  return "Diamond";
        case TPMSDesign::SchwarzP: return "SchwarzP";
        case TPMSDesign::Lidinoid: return "Lidinoid";
        case TPMSDesign::Neovius:  return "Neovius";
    }
    return "Gyroid";
}

static TPMSDesign design_from_str(const std::string& s) {
    if (s == "Diamond")  return TPMSDesign::Diamond;
    if (s == "SchwarzP") return TPMSDesign::SchwarzP;
    if (s == "Lidinoid") return TPMSDesign::Lidinoid;
    if (s == "Neovius")  return TPMSDesign::Neovius;
    return TPMSDesign::Gyroid;
}

static const char* domain_str(DomainType d) {
    switch (d) {
        case DomainType::Cuboid:   return "Cuboid";
        case DomainType::Cylinder: return "Cylinder";
        case DomainType::Sphere:   return "Sphere";
    }
    return "Cuboid";
}

static DomainType domain_from_str(const std::string& s) {
    if (s == "Cylinder") return DomainType::Cylinder;
    if (s == "Sphere")   return DomainType::Sphere;
    return DomainType::Cuboid;
}

static const char* geom_mode_str(GeometryMode m) {
    return m == GeometryMode::Solid ? "Solid" : "Shell";
}

static GeometryMode geom_mode_from_str(const std::string& s) {
    return s == "Solid" ? GeometryMode::Solid : GeometryMode::Shell;
}

static const char* mesh_engine_str(MeshEngine e) {
    switch (e) {
        case MeshEngine::GMSH:           return "GMSH";
        case MeshEngine::Netgen:         return "Netgen";
        case MeshEngine::AdvancingFront: return "AdvancingFront";
        case MeshEngine::MarchingCubes:  return "MarchingCubes";
        case MeshEngine::VoxelTet:       return "VoxelTet";
    }
    return "GMSH";
}

static MeshEngine mesh_engine_from_str(const std::string& s) {
    if (s == "Netgen")         return MeshEngine::Netgen;
    if (s == "AdvancingFront") return MeshEngine::AdvancingFront;
    if (s == "MarchingCubes")  return MeshEngine::MarchingCubes;
    if (s == "VoxelTet")       return MeshEngine::VoxelTet;
    return MeshEngine::GMSH;
}

// ── Save ──────────────────────────────────────────────────────────────────────

IoResult save_project(const ProjectState& state, const std::string& path) {
    json j;

    j["version"]      = "1.0";
    j["project_name"] = state.project_name;

    // Geometry
    j["geometry"]["design"]         = design_str(state.design);
    j["geometry"]["domain"]         = domain_str(state.domain);
    j["geometry"]["geometry_mode"]  = geom_mode_str(state.geometry_mode);
    j["geometry"]["size"]           = {state.size_x, state.size_y, state.size_z};
    j["geometry"]["cell"]           = {state.cell_x, state.cell_y, state.cell_z};
    j["geometry"]["origin"]         = {state.origin_x, state.origin_y, state.origin_z};
    j["geometry"]["wall_thickness"] = state.wall_thickness;
    j["geometry"]["resolution"]     = state.resolution;

    // Mesh
    j["mesh"]["engine"]       = mesh_engine_str(state.mesh_engine);
    j["mesh"]["elem_size"]    = state.elem_size;
    j["mesh"]["elem_size_min"]= state.elem_size_min;
    j["mesh"]["elem_size_max"]= state.elem_size_max;
    j["mesh"]["growth_rate"]  = state.mesh_growth_rate;

    // Material
    j["material"]["library_name"]  = state.material.library_name;
    j["material"]["material_name"] = state.material.material_name;
    j["material"]["density"]       = state.material.density;
    j["material"]["young_modulus"] = state.material.young_modulus;
    j["material"]["poisson_ratio"] = state.material.poisson_ratio;
    j["material"]["yield_strength"]= state.material.yield_strength;

    // Fixed supports
    for (const auto& fs : state.fixed_supports)
        j["fixed_supports"].push_back(fs.face_label);

    // Force loads
    for (const auto& fl : state.force_loads)
        j["force_loads"].push_back({
            {"face", fl.face_label},
            {"fx", fl.fx}, {"fy", fl.fy}, {"fz", fl.fz}
        });

    // Displacement loads
    for (const auto& dl : state.displacement_loads)
        j["displacement_loads"].push_back({
            {"face", dl.face_label},
            {"ux", dl.ux}, {"uy", dl.uy}, {"uz", dl.uz}
        });

    // Pending BC configuration
    j["pending_bc"]["face"]         = state.pending_bc_face;
    j["pending_bc"]["force"]        = {state.pending_force_fx, state.pending_force_fy, state.pending_force_fz};
    j["pending_bc"]["displacement"] = {state.pending_disp_ux, state.pending_disp_uy, state.pending_disp_uz};

    // Solver
    j["solver"]["max_iter"] = state.solver_max_iter;
    j["solver"]["tol"]      = state.solver_tol;

    std::ofstream f(path);
    if (!f) return {false, "Cannot open file for writing: " + path};

    f << j.dump(2);
    if (!f) return {false, "Write error: " + path};

    return {true, "Project saved to: " + path};
}

// ── Load ──────────────────────────────────────────────────────────────────────

IoResult load_project(ProjectState& state, const std::string& path) {
    std::ifstream f(path);
    if (!f) return {false, "Cannot open file: " + path};

    json j;
    try {
        f >> j;
    } catch (const json::parse_error& e) {
        return {false, std::string("JSON parse error: ") + e.what()};
    }

    // Version check (be lenient)
    if (j.contains("project_name"))
        state.project_name = j["project_name"].get<std::string>();

    // Geometry
    if (j.contains("geometry")) {
        const auto& g = j["geometry"];
        if (g.contains("design"))        state.design        = design_from_str(g["design"]);
        if (g.contains("domain"))        state.domain        = domain_from_str(g["domain"]);
        if (g.contains("geometry_mode")) state.geometry_mode = geom_mode_from_str(g["geometry_mode"]);
        if (g.contains("size") && g["size"].size() == 3) {
            state.size_x = g["size"][0]; state.size_y = g["size"][1]; state.size_z = g["size"][2];
        }
        if (g.contains("cell") && g["cell"].size() == 3) {
            state.cell_x = g["cell"][0]; state.cell_y = g["cell"][1]; state.cell_z = g["cell"][2];
        }
        if (g.contains("origin") && g["origin"].size() == 3) {
            state.origin_x = g["origin"][0]; state.origin_y = g["origin"][1]; state.origin_z = g["origin"][2];
        }
        if (g.contains("wall_thickness")) state.wall_thickness = g["wall_thickness"];
        if (g.contains("resolution"))     state.resolution     = g["resolution"];
    }

    // Mesh
    if (j.contains("mesh")) {
        const auto& m = j["mesh"];
        if (m.contains("engine"))        state.mesh_engine    = mesh_engine_from_str(m["engine"]);
        if (m.contains("elem_size"))     state.elem_size      = m["elem_size"];
        if (m.contains("elem_size_min")) state.elem_size_min  = m["elem_size_min"];
        if (m.contains("elem_size_max")) state.elem_size_max  = m["elem_size_max"];
        if (m.contains("growth_rate"))   state.mesh_growth_rate = m["growth_rate"];
    }

    // Material
    if (j.contains("material")) {
        const auto& m = j["material"];
        if (m.contains("library_name"))  state.material.library_name  = m["library_name"];
        if (m.contains("material_name")) state.material.material_name = m["material_name"];
        if (m.contains("density"))       state.material.density       = m["density"];
        if (m.contains("young_modulus")) state.material.young_modulus = m["young_modulus"];
        if (m.contains("poisson_ratio")) state.material.poisson_ratio = m["poisson_ratio"];
        if (m.contains("yield_strength"))state.material.yield_strength= m["yield_strength"];
        state.has_material = true;
    }

    // Fixed supports
    state.fixed_supports.clear();
    if (j.contains("fixed_supports")) {
        for (const auto& fs : j["fixed_supports"])
            state.fixed_supports.push_back({fs.get<std::string>()});
        state.has_fixed_bc = !state.fixed_supports.empty();
    }

    // Force loads
    state.force_loads.clear();
    if (j.contains("force_loads")) {
        for (const auto& fl : j["force_loads"])
            state.force_loads.push_back({
                fl.value("face", "Top"),
                fl.value("fx", 0.f),
                fl.value("fy", 0.f),
                fl.value("fz", -1000.f)
            });
    }

    // Displacement loads
    state.displacement_loads.clear();
    if (j.contains("displacement_loads")) {
        for (const auto& dl : j["displacement_loads"])
            state.displacement_loads.push_back({
                dl.value("face", "Top"),
                dl.value("ux", 0.f),
                dl.value("uy", 0.f),
                dl.value("uz", 0.f)
            });
    }
    state.has_load_bc = !state.force_loads.empty() || !state.displacement_loads.empty();

    // Pending BC
    if (j.contains("pending_bc")) {
        const auto& p = j["pending_bc"];
        if (p.contains("face")) state.pending_bc_face = p["face"].get<std::string>();
        if (p.contains("force") && p["force"].size() == 3) {
            state.pending_force_fx = p["force"][0];
            state.pending_force_fy = p["force"][1];
            state.pending_force_fz = p["force"][2];
        }
        if (p.contains("displacement") && p["displacement"].size() == 3) {
            state.pending_disp_ux = p["displacement"][0];
            state.pending_disp_uy = p["displacement"][1];
            state.pending_disp_uz = p["displacement"][2];
        }
    }

    // Solver
    if (j.contains("solver")) {
        const auto& s = j["solver"];
        if (s.contains("max_iter")) state.solver_max_iter = s["max_iter"];
        if (s.contains("tol"))      state.solver_tol      = s["tol"];
    }

    // Computed data is not saved — mark as not available
    state.has_geometry     = false;
    state.has_surface_mesh = false;
    state.has_volume_mesh  = false;
    state.validation_ok    = false;
    state.has_results      = false;
    state.dirty            = false;

    return {true, "Project loaded from: " + path};
}

} // namespace tpms::io
