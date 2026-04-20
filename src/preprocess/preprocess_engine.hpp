#pragma once

#include <string>
#include <vector>

#include "../state/project_state.hpp"
#include "../geometry/meshing_engine.hpp"

namespace tpms::preprocess {

// ── Validation report ─────────────────────────────────────────────────────────

struct ValidationReport {
    bool ok = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// ── Face node sets ────────────────────────────────────────────────────────────

struct NodeSet {
    std::string      label;
    std::vector<int> node_ids;
};

// Extract 6 axis-aligned face node sets from a volume mesh.
// tol_fraction: fraction of bounding-box extent used as face-capture tolerance.
std::vector<NodeSet> extract_face_sets(
    const geometry::VolumeMeshData& mesh,
    float tol_fraction = 0.03f
);

// ── Preprocessed FEM model ────────────────────────────────────────────────────

struct PreprocessedModel {
    int n_nodes = 0;
    int n_dofs  = 0;   // 3 * n_nodes

    // Global stiffness matrix in COO (triplet) format.
    // Duplicates are intentional — the solver sums them during CSR conversion.
    // Penalty entries for Dirichlet BCs are appended after assembly.
    std::vector<int>    K_row;
    std::vector<int>    K_col;
    std::vector<double> K_val;

    // Global load vector (N), length n_dofs.
    std::vector<double> f;

    // Dirichlet-constrained DOF indices (for postprocessing/reference).
    std::vector<int>    constrained_dofs;

    // Penalty constraints used for displacement BCs and reaction recovery.
    std::vector<int>    penalty_dofs;
    std::vector<double> penalty_alpha;
    std::vector<double> penalty_prescribed;

    // Face node sets (retained for postprocessing reactions).
    std::vector<NodeSet> face_sets;

    // Material echo
    double E       = 0.0;
    double nu      = 0.0;
    double density = 0.0;

    // Stats
    int n_constrained_dofs = 0;
    int n_loaded_nodes     = 0;
    std::string summary;
    bool valid = false;
};

// Assemble global K and f, apply BCs via penalty method.
// Requires a valid volume mesh and at least one fixed support + load in state.
PreprocessedModel build_preprocessed_model(
    const ProjectState&             state,
    const geometry::VolumeMeshData& mesh
);

// ── Material library ──────────────────────────────────────────────────────────

ProjectState::MaterialDefinition material_from_library(const std::string& name);
void assign_material(ProjectState& state, const ProjectState::MaterialDefinition& mat);

// ── BC helpers ────────────────────────────────────────────────────────────────

void add_fixed_support(ProjectState& state, const std::string& face_label);
void add_force_load(ProjectState& state, const ProjectState::ForceLoad& load);
void add_displacement_load(ProjectState& state, const ProjectState::DisplacementLoad& load);

// ── Preprocessing validation ──────────────────────────────────────────────────

ValidationReport validate_preprocessing(const ProjectState& state);

} // namespace tpms::preprocess
