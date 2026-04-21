#pragma once

#include <string>

#include "../geometry/meshing_engine.hpp"
#include "../state/project_state.hpp"
#include "linear_solver.hpp"

namespace tpms::solve {

// Locate the bundled CalculiX binary.  Returns "" if not found.
std::string find_ccx_binary();

// Run a linear-static analysis via CalculiX.
// Writes a temporary .inp, invokes ccx, parses the .dat displacement output.
// Stress recovery is intentionally left to the caller (recover_tet4_von_mises).
LinearSolveResult solve_via_ccx(
    const geometry::VolumeMeshData& mesh,
    const ProjectState& state,
    const std::string& ccx_binary,
    ProgressCallback progress_callback = {}
);

} // namespace tpms::solve
