#include "linear_solver.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tpms::solve {
namespace {

double dot(const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) s += a[i] * b[i];
    return s;
}

double norm2(const std::vector<double>& v) {
    return std::sqrt(std::max(0.0, dot(v, v)));
}

void matvec_coo(
    const preprocess::PreprocessedModel& model,
    const std::vector<double>& x,
    std::vector<double>& y
) {
    std::fill(y.begin(), y.end(), 0.0);
    const std::size_t nnz = model.K_val.size();
    for (std::size_t k = 0; k < nnz; ++k) {
        const int r = model.K_row[k];
        const int c = model.K_col[k];
        if (r >= 0 && r < model.n_dofs && c >= 0 && c < model.n_dofs) {
            y[r] += model.K_val[k] * x[c];
        }
    }
}

std::vector<double> jacobi_inverse(const preprocess::PreprocessedModel& model) {
    std::vector<double> inv((std::size_t)model.n_dofs, 1.0);
    std::vector<double> diag((std::size_t)model.n_dofs, 0.0);

    for (std::size_t k = 0; k < model.K_val.size(); ++k) {
        const int r = model.K_row[k];
        const int c = model.K_col[k];
        if (r == c && r >= 0 && r < model.n_dofs) {
            diag[(std::size_t)r] += model.K_val[k];
        }
    }

    for (int i = 0; i < model.n_dofs; ++i) {
        const double d = diag[(std::size_t)i];
        if (std::isfinite(d) && std::abs(d) > 1e-30) {
            inv[(std::size_t)i] = 1.0 / d;
        }
    }
    return inv;
}

void fill_displacement_magnitude(LinearSolveResult& out, int n_nodes) {
    out.displacement_magnitude.assign((std::size_t)n_nodes, 0.0f);
    out.displacement_x.assign((std::size_t)n_nodes, 0.0f);
    out.displacement_y.assign((std::size_t)n_nodes, 0.0f);
    out.displacement_z.assign((std::size_t)n_nodes, 0.0f);
    for (int n = 0; n < n_nodes; ++n) {
        const double ux = out.displacement[(std::size_t)n * 3 + 0];
        const double uy = out.displacement[(std::size_t)n * 3 + 1];
        const double uz = out.displacement[(std::size_t)n * 3 + 2];
        const double mag = std::sqrt(ux * ux + uy * uy + uz * uz);
        out.displacement_x[(std::size_t)n] = static_cast<float>(ux);
        out.displacement_y[(std::size_t)n] = static_cast<float>(uy);
        out.displacement_z[(std::size_t)n] = static_cast<float>(uz);
        out.displacement_magnitude[(std::size_t)n] = static_cast<float>(mag);
    }
}

void fill_reaction_force_magnitude(
    LinearSolveResult& out,
    const preprocess::PreprocessedModel& model
) {
    out.reaction_force_magnitude.assign((std::size_t)model.n_nodes, 0.0f);
    std::vector<double> rx((std::size_t)model.n_nodes, 0.0);
    std::vector<double> ry((std::size_t)model.n_nodes, 0.0);
    std::vector<double> rz((std::size_t)model.n_nodes, 0.0);

    const std::size_t n = std::min({
        model.penalty_dofs.size(),
        model.penalty_alpha.size(),
        model.penalty_prescribed.size()
    });

    for (std::size_t i = 0; i < n; ++i) {
        const int dof = model.penalty_dofs[i];
        if (dof < 0 || dof >= model.n_dofs) continue;
        const int node = dof / 3;
        const int comp = dof % 3;
        const double u = out.displacement[(std::size_t)dof];
        const double reaction = model.penalty_alpha[i] * (model.penalty_prescribed[i] - u);
        if (comp == 0) rx[(std::size_t)node] += reaction;
        if (comp == 1) ry[(std::size_t)node] += reaction;
        if (comp == 2) rz[(std::size_t)node] += reaction;
    }

    for (int node = 0; node < model.n_nodes; ++node) {
        const double x = rx[(std::size_t)node];
        const double y = ry[(std::size_t)node];
        const double z = rz[(std::size_t)node];
        out.reaction_force_magnitude[(std::size_t)node] =
            static_cast<float>(std::sqrt(x * x + y * y + z * z));
    }
}

} // namespace

LinearSolveResult solve_linear_static(
    const preprocess::PreprocessedModel& model,
    int max_iterations,
    double tolerance,
    ProgressCallback progress_callback
) {
    LinearSolveResult out;

    if (!model.valid || model.n_dofs <= 0 || model.n_nodes <= 0) {
        out.message = "Preprocessed model is not valid.";
        return out;
    }
    if ((int)model.f.size() != model.n_dofs) {
        out.message = "Load vector size does not match DOF count.";
        return out;
    }
    if (model.K_row.size() != model.K_col.size() || model.K_row.size() != model.K_val.size()) {
        out.message = "Stiffness matrix triplets are inconsistent.";
        return out;
    }

    max_iterations = std::clamp(max_iterations, 1, 50000);
    tolerance = std::clamp(tolerance, 1e-14, 1e-2);

    const int n = model.n_dofs;
    out.displacement.assign((std::size_t)n, 0.0);

    std::vector<double> r = model.f;
    std::vector<double> z((std::size_t)n, 0.0);
    std::vector<double> p((std::size_t)n, 0.0);
    std::vector<double> Ap((std::size_t)n, 0.0);
    const auto Minv = jacobi_inverse(model);

    out.initial_residual = norm2(r);
    out.final_residual = out.initial_residual;
    if (!std::isfinite(out.initial_residual) || out.initial_residual <= 1e-30) {
        out.ok = true;
        out.converged = true;
        out.message = "Zero load vector; displacement solution is zero.";
        out.residual_history.push_back(0.0f);
        fill_displacement_magnitude(out, model.n_nodes);
        fill_reaction_force_magnitude(out, model);
        return out;
    }
    if (progress_callback) progress_callback(0, 1.0);

    for (int i = 0; i < n; ++i) {
        z[(std::size_t)i] = Minv[(std::size_t)i] * r[(std::size_t)i];
        p[(std::size_t)i] = z[(std::size_t)i];
    }

    double rz_old = dot(r, z);
    if (!std::isfinite(rz_old) || std::abs(rz_old) <= 1e-300) {
        out.message = "Solver could not start; matrix preconditioner is singular.";
        return out;
    }
    out.residual_history.push_back(1.0f);

    for (int it = 1; it <= max_iterations; ++it) {
        matvec_coo(model, p, Ap);
        const double pAp = dot(p, Ap);
        if (!std::isfinite(pAp) || std::abs(pAp) <= 1e-300) {
            out.message = "Solver stopped because the stiffness matrix appears singular.";
            out.iterations = it - 1;
            fill_displacement_magnitude(out, model.n_nodes);
            fill_reaction_force_magnitude(out, model);
            return out;
        }

        const double alpha = rz_old / pAp;
        for (int i = 0; i < n; ++i) {
            out.displacement[(std::size_t)i] += alpha * p[(std::size_t)i];
            r[(std::size_t)i] -= alpha * Ap[(std::size_t)i];
        }

        out.final_residual = norm2(r);
        const double rel = out.final_residual / out.initial_residual;
        out.residual_history.push_back(static_cast<float>(rel));
        out.iterations = it;
        if (progress_callback) progress_callback(it, rel);

        if (std::isfinite(rel) && rel <= tolerance) {
            out.ok = true;
            out.converged = true;
            break;
        }

        for (int i = 0; i < n; ++i) {
            z[(std::size_t)i] = Minv[(std::size_t)i] * r[(std::size_t)i];
        }
        const double rz_new = dot(r, z);
        if (!std::isfinite(rz_new) || std::abs(rz_old) <= 1e-300) {
            out.message = "Solver stopped due to numerical breakdown.";
            fill_displacement_magnitude(out, model.n_nodes);
            fill_reaction_force_magnitude(out, model);
            return out;
        }
        const double beta = rz_new / rz_old;
        for (int i = 0; i < n; ++i) {
            p[(std::size_t)i] = z[(std::size_t)i] + beta * p[(std::size_t)i];
        }
        rz_old = rz_new;
    }

    out.ok = true;
    fill_displacement_magnitude(out, model.n_nodes);
    fill_reaction_force_magnitude(out, model);

    char buf[256];
    std::snprintf(buf, sizeof buf,
        "%s in %d iterations | residual %.3e -> %.3e",
        out.converged ? "Converged" : "Stopped",
        out.iterations,
        out.initial_residual,
        out.final_residual);
    out.message = buf;
    return out;
}

} // namespace tpms::solve
