#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../preprocess/preprocess_engine.hpp"

namespace tpms::solve {

using ProgressCallback = std::function<void(int iteration, double relative_residual)>;

struct LinearSolveResult {
    bool ok = false;
    bool converged = false;
    int iterations = 0;
    double initial_residual = 0.0;
    double final_residual = 0.0;
    std::string message;

    // Full displacement vector, length = model.n_dofs, order UX/UY/UZ per node.
    std::vector<double> displacement;

    // Nodal scalar output for the result viewer.
    std::vector<float> displacement_magnitude;
    std::vector<float> displacement_x;
    std::vector<float> displacement_y;
    std::vector<float> displacement_z;
    std::vector<float> reaction_force_magnitude;

    // Relative residual history, including the initial value.
    std::vector<float> residual_history;
};

LinearSolveResult solve_linear_static(
    const preprocess::PreprocessedModel& model,
    int max_iterations,
    double tolerance,
    ProgressCallback progress_callback = {}
);

} // namespace tpms::solve
