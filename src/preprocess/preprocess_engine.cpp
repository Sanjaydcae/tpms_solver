#include "preprocess_engine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <limits>

namespace tpms::preprocess {

namespace {

// Normalize UI labels like "Bottom (Z-min)" and saved labels like "Bottom face".
static std::string canonical_face(const std::string& label) {
    auto s = label;
    const auto paren = s.find('(');
    if (paren != std::string::npos) s = s.substr(0, paren);
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();

    const std::string suffix = " face";
    if (s.size() >= suffix.size()) {
        std::string tail = s.substr(s.size() - suffix.size());
        std::transform(tail.begin(), tail.end(), tail.begin(), ::tolower);
        if (tail == suffix) s = s.substr(0, s.size() - suffix.size());
    }

    if (!s.empty()) s[0] = (char)::toupper((unsigned char)s[0]);
    for (std::size_t i = 1; i < s.size(); ++i) {
        s[i] = (char)::tolower((unsigned char)s[i]);
    }
    return s;
}

static bool face_label_exists(const std::vector<ProjectState::FaceInfo>& faces, const std::string& label) {
    if (faces.empty()) return true;
    const std::string wanted = canonical_face(label);
    for (const auto& face : faces) {
        if (canonical_face(face.label) == wanted && face.node_count > 0) {
            return true;
        }
    }
    return false;
}

} // namespace

// ── Material library ──────────────────────────────────────────────────────────

ProjectState::MaterialDefinition material_from_library(const std::string& name) {
    ProjectState::MaterialDefinition m;
    m.library_name  = name;
    m.material_name = name;

    if (name == "ABS") {
        m.density = 1040.f; m.young_modulus = 2300.f;
        m.poisson_ratio = 0.35f; m.yield_strength = 40.f;
    } else if (name == "PLA") {
        m.density = 1240.f; m.young_modulus = 3500.f;
        m.poisson_ratio = 0.36f; m.yield_strength = 60.f;
    } else if (name == "Aluminium") {
        m.density = 2700.f; m.young_modulus = 70000.f;
        m.poisson_ratio = 0.33f; m.yield_strength = 250.f;
    } else if (name == "Stainless Steel") {
        m.density = 8000.f; m.young_modulus = 193000.f;
        m.poisson_ratio = 0.29f; m.yield_strength = 290.f;
    } else if (name == "Titanium") {
        m.density = 4500.f; m.young_modulus = 114000.f;
        m.poisson_ratio = 0.34f; m.yield_strength = 880.f;
    } else if (name == "PEEK") {
        m.density = 1320.f; m.young_modulus = 3600.f;
        m.poisson_ratio = 0.40f; m.yield_strength = 100.f;
    } else {
        // Default: Structural Steel
        m.library_name  = "Structural Steel";
        m.material_name = "Structural Steel";
        m.density = 7850.f; m.young_modulus = 210000.f;
        m.poisson_ratio = 0.30f; m.yield_strength = 250.f;
    }
    return m;
}

void assign_material(ProjectState& state, const ProjectState::MaterialDefinition& mat) {
    state.material     = mat;
    state.has_material = true;
}

// ── BC helpers ────────────────────────────────────────────────────────────────

void add_fixed_support(ProjectState& state, const std::string& face_label) {
    state.fixed_supports.push_back({face_label});
    state.has_fixed_bc = true;
}

void add_force_load(ProjectState& state, const ProjectState::ForceLoad& load) {
    state.force_loads.push_back(load);
    state.has_load_bc = true;
}

void add_displacement_load(ProjectState& state, const ProjectState::DisplacementLoad& load) {
    state.displacement_loads.push_back(load);
    state.has_load_bc = true;
}

// ── Validation ────────────────────────────────────────────────────────────────

ValidationReport validate_preprocessing(const ProjectState& state) {
    ValidationReport report;

    if (!state.has_volume_mesh) {
        report.ok = false;
        report.errors.push_back("Volume mesh required before preprocessing.");
    }
    if (!state.has_material) {
        report.ok = false;
        report.errors.push_back("Assign a material before validation.");
    }
    if (state.has_material) {
        if (state.material.young_modulus <= 0.f) {
            report.ok = false;
            report.errors.push_back("Young's modulus must be greater than zero.");
        }
        if (state.material.poisson_ratio < 0.f || state.material.poisson_ratio >= 0.5f) {
            report.ok = false;
            report.errors.push_back("Poisson ratio must be in [0, 0.5).");
        }
    }
    if (state.fixed_supports.empty()) {
        report.ok = false;
        report.errors.push_back("At least one fixed support is required.");
    }
    if (state.force_loads.empty() && state.displacement_loads.empty()) {
        report.ok = false;
        report.errors.push_back("Add at least one force or displacement load.");
    }
    if (!state.force_loads.empty() && !state.displacement_loads.empty()) {
        report.warnings.push_back(
            "Both force and displacement loads present — verify this is intentional.");
    }
    for (const auto& fs : state.fixed_supports) {
        if (!face_label_exists(state.face_infos, fs.face_label)) {
            report.ok = false;
            report.errors.push_back("Fixed support face has no matching mesh nodes: " + fs.face_label);
        }
    }
    for (const auto& fl : state.force_loads) {
        if (!face_label_exists(state.face_infos, fl.face_label)) {
            report.ok = false;
            report.errors.push_back("Force load face has no matching mesh nodes: " + fl.face_label);
        }
        if (std::abs(fl.fx) + std::abs(fl.fy) + std::abs(fl.fz) <= 1e-12f) {
            report.warnings.push_back("Force load has zero magnitude on face: " + fl.face_label);
        }
    }
    for (const auto& dl : state.displacement_loads) {
        if (!face_label_exists(state.face_infos, dl.face_label)) {
            report.ok = false;
            report.errors.push_back("Displacement face has no matching mesh nodes: " + dl.face_label);
        }
        if (std::abs(dl.ux) + std::abs(dl.uy) + std::abs(dl.uz) <= 1e-12f) {
            report.warnings.push_back("Displacement load has zero prescribed value on face: " + dl.face_label);
        }
    }
    return report;
}

// ── Face node set extraction ───────────────────────────────────────────────────

std::vector<NodeSet> extract_face_sets(
    const geometry::VolumeMeshData& mesh,
    float tol_fraction
) {
    if (mesh.nodes.empty()) return {};

    float xmin =  std::numeric_limits<float>::max();
    float xmax = -std::numeric_limits<float>::max();
    float ymin =  std::numeric_limits<float>::max();
    float ymax = -std::numeric_limits<float>::max();
    float zmin =  std::numeric_limits<float>::max();
    float zmax = -std::numeric_limits<float>::max();

    for (const auto& n : mesh.nodes) {
        xmin = std::min(xmin, n.x); xmax = std::max(xmax, n.x);
        ymin = std::min(ymin, n.y); ymax = std::max(ymax, n.y);
        zmin = std::min(zmin, n.z); zmax = std::max(zmax, n.z);
    }

    const float tolx = std::max((xmax - xmin) * tol_fraction, 1e-4f);
    const float toly = std::max((ymax - ymin) * tol_fraction, 1e-4f);
    const float tolz = std::max((zmax - zmin) * tol_fraction, 1e-4f);

    // Six canonical faces: Bottom(-Z), Top(+Z), Back(-Y), Front(+Y), Left(-X), Right(+X)
    const struct FaceDef {
        const char* label;
        int axis;    // 0=X, 1=Y, 2=Z
        int side;    // -1 = min, +1 = max
    } faces[6] = {
        {"Bottom", 2, -1},
        {"Top",    2, +1},
        {"Back",   1, -1},
        {"Front",  1, +1},
        {"Left",   0, -1},
        {"Right",  0, +1},
    };

    const float lim_lo[3] = {xmin, ymin, zmin};
    const float lim_hi[3] = {xmax, ymax, zmax};
    const float tols[3]   = {tolx, toly, tolz};

    std::vector<NodeSet> out;
    out.resize(6);
    for (int f = 0; f < 6; ++f) out[f].label = faces[f].label;

    for (int ni = 0; ni < (int)mesh.nodes.size(); ++ni) {
        const auto& n = mesh.nodes[ni];
        const float coords[3] = {n.x, n.y, n.z};
        for (int f = 0; f < 6; ++f) {
            const int ax   = faces[f].axis;
            const int side = faces[f].side;
            const float ref = (side < 0) ? lim_lo[ax] : lim_hi[ax];
            if (std::abs(coords[ax] - ref) <= tols[ax]) {
                out[f].node_ids.push_back(ni);
            }
        }
    }
    return out;
}

// ── 4-node linear tet stiffness (returns 12×12 column-major) ─────────────────

namespace {

static void tet4_stiffness(
    double x0, double y0, double z0,
    double x1, double y1, double z1,
    double x2, double y2, double z2,
    double x3, double y3, double z3,
    double E, double nu,
    double Ke[12][12]
) {
    std::memset(Ke, 0, 144 * sizeof(double));

    // Jacobian (columns = edge vectors from node 0)
    const double J[3][3] = {
        {x1-x0, x2-x0, x3-x0},
        {y1-y0, y2-y0, y3-y0},
        {z1-z0, z2-z0, z3-z0},
    };

    const double detJ =
        J[0][0]*(J[1][1]*J[2][2] - J[1][2]*J[2][1])
      - J[0][1]*(J[1][0]*J[2][2] - J[1][2]*J[2][0])
      + J[0][2]*(J[1][0]*J[2][1] - J[1][1]*J[2][0]);

    const double absDetJ = std::abs(detJ);
    if (absDetJ < 1e-20) return;   // degenerate element

    const double V = absDetJ / 6.0;
    const double id = 1.0 / detJ;

    // Inverse of J
    double iJ[3][3];
    iJ[0][0] =  (J[1][1]*J[2][2] - J[1][2]*J[2][1]) * id;
    iJ[0][1] = -(J[0][1]*J[2][2] - J[0][2]*J[2][1]) * id;
    iJ[0][2] =  (J[0][1]*J[1][2] - J[0][2]*J[1][1]) * id;
    iJ[1][0] = -(J[1][0]*J[2][2] - J[1][2]*J[2][0]) * id;
    iJ[1][1] =  (J[0][0]*J[2][2] - J[0][2]*J[2][0]) * id;
    iJ[1][2] = -(J[0][0]*J[1][2] - J[0][2]*J[1][0]) * id;
    iJ[2][0] =  (J[1][0]*J[2][1] - J[1][1]*J[2][0]) * id;
    iJ[2][1] = -(J[0][0]*J[2][1] - J[0][1]*J[2][0]) * id;
    iJ[2][2] =  (J[0][0]*J[1][1] - J[0][1]*J[1][0]) * id;

    // Shape-function derivatives in natural coords: rows = nodes, cols = ξ,η,ζ
    // N0=1-ξ-η-ζ, N1=ξ, N2=η, N3=ζ  →  dN/d(nat) matrix:
    const double dNdxi[4][3] = {{-1,-1,-1},{1,0,0},{0,1,0},{0,0,1}};

    // dN/dx = dN/dxi * iJ   (4×3)
    double dNdx[4][3];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 3; ++j) {
            dNdx[i][j] = 0.0;
            for (int k = 0; k < 3; ++k)
                dNdx[i][j] += dNdxi[i][k] * iJ[k][j];
        }

    // B matrix (6×12): strain-displacement
    double B[6][12] = {};
    for (int i = 0; i < 4; ++i) {
        const int c = 3 * i;
        B[0][c+0] = dNdx[i][0];
        B[1][c+1] = dNdx[i][1];
        B[2][c+2] = dNdx[i][2];
        B[3][c+0] = dNdx[i][1];  B[3][c+1] = dNdx[i][0];
        B[4][c+1] = dNdx[i][2];  B[4][c+2] = dNdx[i][1];
        B[5][c+0] = dNdx[i][2];  B[5][c+2] = dNdx[i][0];
    }

    // D matrix (6×6): isotropic linear elasticity
    const double lam = E * nu / ((1.0 + nu) * (1.0 - 2.0 * nu));
    const double mu  = E / (2.0 * (1.0 + nu));
    double D[6][6] = {};
    D[0][0] = D[1][1] = D[2][2] = lam + 2.0 * mu;
    D[0][1] = D[0][2] = D[1][0] = D[1][2] = D[2][0] = D[2][1] = lam;
    D[3][3] = D[4][4] = D[5][5] = mu;

    // DB = D * B  (6×12)
    double DB[6][12] = {};
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 12; ++j)
            for (int k = 0; k < 6; ++k)
                DB[i][j] += D[i][k] * B[k][j];

    // Ke = V * B^T * DB  (12×12)
    for (int i = 0; i < 12; ++i)
        for (int j = 0; j < 12; ++j)
            for (int k = 0; k < 6; ++k)
                Ke[i][j] += V * B[k][i] * DB[k][j];
}

// Helper: find NodeSet by label in a vector
static const NodeSet* find_face(const std::vector<NodeSet>& sets, const std::string& label) {
    for (const auto& s : sets)
        if (s.label == label) return &s;
    return nullptr;
}

} // namespace

// ── FEM assembly ──────────────────────────────────────────────────────────────

PreprocessedModel build_preprocessed_model(
    const ProjectState&             state,
    const geometry::VolumeMeshData& mesh
) {
    PreprocessedModel model;

    if (mesh.nodes.empty() || mesh.tets.empty()) {
        model.summary = "Empty mesh — cannot assemble.";
        return model;
    }

    model.n_nodes = (int)mesh.nodes.size();
    model.n_dofs  = model.n_nodes * 3;
    model.E       = static_cast<double>(state.material.young_modulus);
    model.nu      = static_cast<double>(state.material.poisson_ratio);
    model.density = static_cast<double>(state.material.density);

    const double E  = model.E;
    const double nu = model.nu;

    // ── Reserve COO storage ──
    // Each tet contributes 12×12 = 144 triplets
    model.K_row.reserve(mesh.tets.size() * 144);
    model.K_col.reserve(mesh.tets.size() * 144);
    model.K_val.reserve(mesh.tets.size() * 144);
    model.f.assign(model.n_dofs, 0.0);

    // ── Assemble element stiffness matrices ───────────────────────────────────
    double Ke[12][12];
    for (const auto& tet : mesh.tets) {
        const auto& p0 = mesh.nodes[tet.a];
        const auto& p1 = mesh.nodes[tet.b];
        const auto& p2 = mesh.nodes[tet.c];
        const auto& p3 = mesh.nodes[tet.d];

        tet4_stiffness(
            p0.x, p0.y, p0.z,
            p1.x, p1.y, p1.z,
            p2.x, p2.y, p2.z,
            p3.x, p3.y, p3.z,
            E, nu, Ke
        );

        const int nodes[4]  = {tet.a, tet.b, tet.c, tet.d};
        int global_dofs[12];
        for (int i = 0; i < 4; ++i) {
            global_dofs[3*i+0] = nodes[i] * 3;
            global_dofs[3*i+1] = nodes[i] * 3 + 1;
            global_dofs[3*i+2] = nodes[i] * 3 + 2;
        }

        for (int i = 0; i < 12; ++i)
            for (int j = 0; j < 12; ++j)
                if (Ke[i][j] != 0.0) {
                    model.K_row.push_back(global_dofs[i]);
                    model.K_col.push_back(global_dofs[j]);
                    model.K_val.push_back(Ke[i][j]);
                }
    }

    // ── Extract face node sets ────────────────────────────────────────────────
    model.face_sets = extract_face_sets(mesh);

    // ── Build force vector from force loads ───────────────────────────────────
    int total_loaded = 0;
    for (const auto& fl : state.force_loads) {
        const std::string cname = canonical_face(fl.face_label);
        const NodeSet* ns = find_face(model.face_sets, cname);
        if (!ns || ns->node_ids.empty()) continue;

        const double n   = static_cast<double>(ns->node_ids.size());
        const double dfx = fl.fx / n;
        const double dfy = fl.fy / n;
        const double dfz = fl.fz / n;

        for (int ni : ns->node_ids) {
            model.f[ni * 3 + 0] += dfx;
            model.f[ni * 3 + 1] += dfy;
            model.f[ni * 3 + 2] += dfz;
        }
        total_loaded += (int)n;
    }

    // ── Apply fixed supports via penalty method ────────────────────────────────
    // Penalty alpha ≈ max diagonal value * 1e12 is typically used.
    // We first estimate the diagonal scale from a sample of K values.
    double k_max = 0.0;
    for (double v : model.K_val)
        if (std::abs(v) > k_max) k_max = std::abs(v);
    const double alpha = (k_max > 0.0) ? k_max * 1e12 : 1e20;

    int total_fixed = 0;
    for (const auto& fs : state.fixed_supports) {
        const std::string cname = canonical_face(fs.face_label);
        const NodeSet* ns = find_face(model.face_sets, cname);
        if (!ns) continue;
        for (int ni : ns->node_ids) {
            for (int d = 0; d < 3; ++d) {
                const int dof = ni * 3 + d;
                model.K_row.push_back(dof);
                model.K_col.push_back(dof);
                model.K_val.push_back(alpha);
                model.f[dof] += alpha * 0.0;   // prescribed displacement = 0
                model.constrained_dofs.push_back(dof);
            }
            ++total_fixed;
        }
    }

    // ── Apply prescribed displacement loads via penalty ────────────────────────
    for (const auto& dl : state.displacement_loads) {
        const std::string cname = canonical_face(dl.face_label);
        const NodeSet* ns = find_face(model.face_sets, cname);
        if (!ns) continue;
        const double ux = dl.ux, uy = dl.uy, uz = dl.uz;
        for (int ni : ns->node_ids) {
            const double prescribed[3] = {ux, uy, uz};
            for (int d = 0; d < 3; ++d) {
                const int dof = ni * 3 + d;
                model.K_row.push_back(dof);
                model.K_col.push_back(dof);
                model.K_val.push_back(alpha);
                model.f[dof] += alpha * prescribed[d];
                model.constrained_dofs.push_back(dof);
            }
            ++total_loaded;
        }
    }

    model.n_constrained_dofs = (int)model.constrained_dofs.size();
    model.n_loaded_nodes     = total_loaded;

    const int nnz = (int)model.K_row.size();
    char buf[512];
    std::snprintf(buf, sizeof buf,
        "%d DOFs | %d tets | %d non-zeros | %d constrained DOFs | %d loaded nodes | "
        "E=%.0f MPa  ν=%.3f",
        model.n_dofs, (int)mesh.tets.size(), nnz,
        model.n_constrained_dofs, model.n_loaded_nodes,
        E, nu);
    model.summary = buf;
    model.valid   = (model.n_constrained_dofs > 0) && (model.n_loaded_nodes > 0) && (nnz > 0);
    return model;
}

} // namespace tpms::preprocess
