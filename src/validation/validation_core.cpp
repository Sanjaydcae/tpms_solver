#include "validation_core.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tpms::validation {
namespace {

void pass(HealthCheckReport& r, const std::string& msg) {
    ++r.passed;
    r.lines.push_back("[PASS] " + msg);
}

void warn(HealthCheckReport& r, const std::string& msg) {
    ++r.warnings;
    r.lines.push_back("[WARN] " + msg);
}

void fail(HealthCheckReport& r, const std::string& msg) {
    r.ok = false;
    ++r.errors;
    r.lines.push_back("[FAIL] " + msg);
}

bool all_finite(const std::vector<float>& values) {
    return std::all_of(values.begin(), values.end(), [](float v) {
        return std::isfinite(v);
    });
}

bool all_finite(const std::vector<double>& values) {
    return std::all_of(values.begin(), values.end(), [](double v) {
        return std::isfinite(v);
    });
}

} // namespace

HealthCheckReport run_model_health_checks(const ProjectState& state) {
    HealthCheckReport r;

    state.has_geometry ? pass(r, "Geometry is generated.")
                       : fail(r, "Geometry is missing.");

    if (state.size_x > 0.f && state.size_y > 0.f && state.size_z > 0.f)
        pass(r, "Domain dimensions are positive.");
    else
        fail(r, "Domain dimensions must be positive.");

    if (state.cell_x > 0.f && state.cell_y > 0.f && state.cell_z > 0.f)
        pass(r, "Unit-cell dimensions are positive.");
    else
        fail(r, "Unit-cell dimensions must be positive.");

    if (state.has_surface_mesh && state.surf_nodes > 0 && state.surf_tris > 0)
        pass(r, "Surface mesh exists.");
    else if (state.has_geometry)
        warn(r, "Surface mesh has not been generated.");
    else
        fail(r, "Surface mesh is missing.");

    if (state.has_volume_mesh && state.vol_nodes > 0 && state.vol_tets > 0)
        pass(r, "Volume mesh exists.");
    else
        fail(r, "Volume mesh is missing.");

    if (state.mesh_quality_min > 0.0f)
        pass(r, "Mesh quality metric is positive.");
    else if (state.has_volume_mesh)
        warn(r, "Mesh quality metric is zero or unavailable.");

    state.has_material ? pass(r, "Material is assigned.")
                       : fail(r, "Material is not assigned.");

    if (state.material.young_modulus > 0.f && state.material.poisson_ratio >= 0.f && state.material.poisson_ratio < 0.5f)
        pass(r, "Linear elastic material constants are valid.");
    else
        fail(r, "Invalid linear elastic material constants.");

    state.has_fixed_bc ? pass(r, "Fixed support exists.")
                       : fail(r, "Fixed support is missing.");

    state.has_load_bc ? pass(r, "At least one load/displacement exists.")
                      : fail(r, "Load/displacement is missing.");

    if (!state.validation_ok)
        warn(r, "Preprocessing validation has not passed in the current model state.");
    else
        pass(r, "Preprocessing validation passed.");

    if (state.solver_has_run) {
        pass(r, "Solver has run.");
        if (state.solver_converged)
            pass(r, "Solver converged.");
        else
            warn(r, "Solver did not converge to the requested tolerance.");
        if (std::isfinite(state.solver_final_residual))
            pass(r, "Final residual is finite.");
        else
            fail(r, "Final residual is not finite.");
    } else {
        warn(r, "Solver has not been run.");
    }

    if (state.has_results) {
        pass(r, "Results are available.");
        if (!state.displacement_result_scalars.empty() && all_finite(state.displacement_result_scalars))
            pass(r, "Displacement result is finite.");
        else
            fail(r, "Displacement result is missing or not finite.");
        if (!state.von_mises_result_scalars.empty() && all_finite(state.von_mises_result_scalars))
            pass(r, "Von Mises result is finite.");
        else
            warn(r, "Von Mises result is missing or not finite.");
        if (!state.strain_result_scalars.empty() && all_finite(state.strain_result_scalars))
            pass(r, "Equivalent strain result is finite.");
        else
            warn(r, "Equivalent strain result is missing or not finite.");
        if (!state.displacement_solution.empty() && all_finite(state.displacement_solution))
            pass(r, "Displacement vector is finite.");
    } else {
        warn(r, "No results available yet.");
    }

    char buf[160];
    std::snprintf(buf, sizeof buf,
        "Model health check: %d passed, %d warnings, %d errors.",
        r.passed, r.warnings, r.errors);
    r.summary = buf;
    return r;
}

} // namespace tpms::validation
