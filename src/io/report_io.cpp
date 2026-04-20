#include "report_io.hpp"

#include <cstdio>
#include <fstream>

namespace tpms::io {

ReportResult export_markdown_report(
    const ProjectState& state,
    const std::string& path
) {
    std::ofstream f(path);
    if (!f) return {false, "Cannot open report for writing: " + path};

    f << "# TPMS Studio Analysis Report\n\n";
    f << "**Product:** TPMS Studio v0.1\n\n";
    f << "**License Owner:** H2one Cleantech Private Limited\n\n";
    f << "**License Type:** Proprietary Commercial / Internal Development License\n\n";
    f << "**Copyright:** (c) 2026 H2one Cleantech Private Limited. All rights reserved.\n\n";

    f << "## Project\n\n";
    f << "- Project: " << state.project_name << "\n";
    f << "- Units: mm-N-MPa\n";
    f << "- Status: " << (state.has_results ? "Solved" : "Not solved") << "\n\n";

    f << "## Geometry\n\n";
    f << "- TPMS Type: " << state.design_name() << "\n";
    f << "- Domain: " << state.domain_name() << "\n";
    f << "- Mode: " << state.geometry_mode_name() << "\n";
    f << "- Size: " << state.size_x << " x " << state.size_y << " x " << state.size_z << " mm\n";
    f << "- Unit Cell: " << state.cell_x << " x " << state.cell_y << " x " << state.cell_z << " mm\n";
    f << "- Wall Thickness: " << state.wall_thickness << " mm\n";
    f << "- Grid: " << state.field_nx << " x " << state.field_ny << " x " << state.field_nz << "\n";
    if (!state.geometry_summary.empty()) f << "- Summary: " << state.geometry_summary << "\n";
    f << "\n";

    f << "## Mesh\n\n";
    f << "- Surface Mesh: " << (state.has_surface_mesh ? "Ready" : "Missing") << "\n";
    f << "- Surface Nodes: " << state.surf_nodes << "\n";
    f << "- Triangles: " << state.surf_tris << "\n";
    f << "- Volume Mesh: " << (state.has_volume_mesh ? "Ready" : "Missing") << "\n";
    f << "- Volume Nodes: " << state.vol_nodes << "\n";
    f << "- Tetrahedra: " << state.vol_tets << "\n";
    f << "- Min Quality: " << state.mesh_quality_min << "\n";
    f << "- Avg Quality: " << state.mesh_quality_avg << "\n";
    f << "- Max Aspect: " << state.mesh_aspect_max << "\n";
    if (!state.mesh_summary.empty()) f << "- Summary: " << state.mesh_summary << "\n";
    f << "\n";

    f << "## Material\n\n";
    f << "- Name: " << state.material.material_name << "\n";
    f << "- Density: " << state.material.density << " kg/m3\n";
    f << "- Young's Modulus: " << state.material.young_modulus << " MPa\n";
    f << "- Poisson Ratio: " << state.material.poisson_ratio << "\n";
    f << "- Yield Strength: " << state.material.yield_strength << " MPa\n\n";

    f << "## Boundary Conditions\n\n";
    for (const auto& fs : state.fixed_supports)
        f << "- Fixed Support: " << fs.face_label << "\n";
    for (const auto& fl : state.force_loads)
        f << "- Force: " << fl.face_label << " [" << fl.fx << ", " << fl.fy << ", " << fl.fz << "] N\n";
    for (const auto& dl : state.displacement_loads)
        f << "- Displacement: " << dl.face_label << " [" << dl.ux << ", " << dl.uy << ", " << dl.uz << "] mm\n";
    if (state.fixed_supports.empty() && state.force_loads.empty() && state.displacement_loads.empty())
        f << "- No boundary conditions assigned\n";
    f << "\n";

    f << "## Solver\n\n";
    f << "- Analysis: " << state.analysis_type_name() << "\n";
    f << "- Validation: " << (state.validation_ok ? "PASS" : "Not validated") << "\n";
    f << "- Solver Status: " << (state.solver_has_run ? (state.solver_converged ? "Converged" : "Approximate / stopped") : "Not run") << "\n";
    f << "- Iterations: " << state.solver_iterations << "\n";
    f << "- Initial Residual: " << state.solver_initial_residual << "\n";
    f << "- Final Residual: " << state.solver_final_residual << "\n";
    if (!state.solver_summary.empty()) f << "- Summary: " << state.solver_summary << "\n";
    if (!state.fem_summary.empty()) f << "- FEM Core: " << state.fem_summary << "\n";
    f << "\n";

    f << "## Results\n\n";
    if (state.has_results) {
        f << "- Active Result: " << state.active_result << "\n";
        f << "- Component: " << state.active_component << "\n";
        f << "- Minimum: " << state.result_min << " " << state.result_unit << " at node " << state.result_min_node << "\n";
        f << "- Maximum: " << state.result_max << " " << state.result_unit << " at node " << state.result_max_node << "\n";
        f << "- Displacement Output: " << (state.displacement_result_scalars.empty() ? "Missing" : "Ready") << "\n";
        f << "- Von Mises Output: " << (state.von_mises_result_scalars.empty() ? "Missing" : "Ready") << "\n";
        f << "- Strain Output: " << (state.strain_result_scalars.empty() ? "Missing" : "Ready") << "\n";
        if (!state.result_summary.empty()) f << "- Summary: " << state.result_summary << "\n";
    } else {
        f << "- No results available\n";
    }
    f << "\n";

    f << "## Validation Notes\n\n";
    if (!state.validation_summary.empty()) {
        f << state.validation_summary << "\n";
    } else {
        f << "No model health check has been run.\n";
    }

    if (!f) return {false, "Write error while exporting report: " + path};

    char msg[160];
    std::snprintf(msg, sizeof msg, "Exported analysis report: %s", path.c_str());
    return {true, msg};
}

} // namespace tpms::io
