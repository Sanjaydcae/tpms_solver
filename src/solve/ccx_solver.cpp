#include "ccx_solver.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>

#include "../preprocess/preprocess_engine.hpp"

namespace tpms::solve {

namespace fs = std::filesystem;

// ── Binary discovery ──────────────────────────────────────────────────────────

static std::string try_path(const fs::path& p) {
    std::error_code ec;
    if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) {
        const auto perms = fs::status(p, ec).permissions();
        const bool executable =
            (perms & fs::perms::owner_exec) != fs::perms::none ||
            (perms & fs::perms::group_exec) != fs::perms::none ||
            (perms & fs::perms::others_exec) != fs::perms::none;
        if (executable) return fs::canonical(p, ec).string();
    }
    return {};
}

std::string find_ccx_binary() {
    const std::vector<std::string> command_names = {
        "ccx",
        "ccx_2.23",
        "ccx_2.22",
        "ccx_2.21",
        "ccx_2.20"
    };

    if (const char* path_env = std::getenv("PATH")) {
        std::stringstream ss(path_env);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            if (dir.empty()) continue;
            for (const auto& name : command_names) {
                auto found = try_path(fs::path(dir) / name);
                if (!found.empty()) return found;
            }
        }
    }

    const std::vector<std::string> system_paths = {
        "/usr/bin/ccx",
        "/usr/local/bin/ccx",
        "/opt/calculix/bin/ccx"
    };
    for (const auto& path : system_paths) {
        auto found = try_path(path);
        if (!found.empty()) return found;
    }

    // Developer fallback only. Do not commit this folder into the proprietary
    // application repository; .gitignore excludes src/ccx_*.
    const std::vector<std::string> rel_paths = {
        "src/ccx_2.23/src/ccx_2.23",
        "../src/ccx_2.23/src/ccx_2.23",
        "ccx_2.23/src/ccx_2.23",
        "../ccx_2.23/src/ccx_2.23",
    };

    for (const auto& r : rel_paths) {
        auto s = try_path(r);
        if (!s.empty()) return s;
    }

    // Try relative to the running executable
    char exe_buf[4096] = {};
    if (readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1) > 0) {
        fs::path exe_dir = fs::path(exe_buf).parent_path();
        for (const auto& r : rel_paths) {
            auto s = try_path(exe_dir / r);
            if (!s.empty()) return s;
            s = try_path(exe_dir.parent_path() / r);
            if (!s.empty()) return s;
        }
    }
    return {};
}

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string ccx_lib_dir(const std::string& ccx_binary) {
    // Binary lives at  .../ccx_2.23/src/ccx_2.23
    // Lib lives at     .../ccx_2.23/lib
    fs::path p(ccx_binary);
    const fs::path candidate = p.parent_path().parent_path() / "lib";
    std::error_code ec;
    return fs::exists(candidate, ec) ? candidate.string() : std::string{};
}

static std::string shell_quote(const std::string& value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

static std::string sanitize_label(const std::string& s) {
    std::string out;
    for (unsigned char c : s)
        out += (std::isalnum(c) ? static_cast<char>(c) : '_');
    return out;
}

// ── INP writer ────────────────────────────────────────────────────────────────

static void write_inp(
    const fs::path& inp_path,
    const geometry::VolumeMeshData& mesh,
    const ProjectState& state
) {
    const auto face_sets = tpms::preprocess::extract_face_sets(mesh);

    auto find_nset = [&](const std::string& label) -> const preprocess::NodeSet* {
        for (const auto& ns : face_sets)
            if (ns.label == label) return &ns;
        return nullptr;
    };

    std::ofstream f(inp_path);
    f.precision(10);
    f << std::scientific;

    f << "** TPMS Studio — CalculiX Linear Static\n";

    // ── Nodes (1-based) ──────────────────────────────────────────────────────
    f << "*NODE\n";
    for (int i = 0; i < (int)mesh.nodes.size(); ++i) {
        const auto& n = mesh.nodes[i];
        f << (i + 1) << ", " << n.x << ", " << n.y << ", " << n.z << "\n";
    }

    // ── Elements C3D4 (1-based) ──────────────────────────────────────────────
    f << "*ELEMENT, TYPE=C3D4, ELSET=EALL\n";
    for (int i = 0; i < (int)mesh.tets.size(); ++i) {
        const auto& t = mesh.tets[i];
        f << (i + 1) << ", "
          << (t.a + 1) << ", " << (t.b + 1) << ", "
          << (t.c + 1) << ", " << (t.d + 1) << "\n";
    }

    // ── Material ─────────────────────────────────────────────────────────────
    f << "*MATERIAL, NAME=TPMSMAT\n";
    f << "*ELASTIC\n";
    f << state.material.young_modulus << ", " << state.material.poisson_ratio << "\n";
    f << "*SOLID SECTION, MATERIAL=TPMSMAT, ELSET=EALL\n";

    // ── Node sets for BCs ────────────────────────────────────────────────────
    std::set<std::string> bc_face_labels;
    for (const auto& s : state.fixed_supports)     bc_face_labels.insert(s.face_label);
    for (const auto& l : state.displacement_loads) bc_face_labels.insert(l.face_label);
    for (const auto& l : state.force_loads)        bc_face_labels.insert(l.face_label);

    for (const auto& lbl : bc_face_labels) {
        auto* ns = find_nset(lbl);
        if (!ns || ns->node_ids.empty()) continue;
        f << "*NSET, NSET=BC_" << sanitize_label(lbl) << "\n";
        int col = 0;
        for (int id : ns->node_ids) {
            f << (id + 1);
            ++col;
            if (col == 10) { f << "\n"; col = 0; }
            else            f << ", ";
        }
        if (col > 0) f << "\n";
    }

    // ── Step ─────────────────────────────────────────────────────────────────
    f << "*STEP\n*STATIC\n";

    // Fixed supports — zero all 3 DOFs
    for (const auto& s : state.fixed_supports) {
        if (!find_nset(s.face_label)) continue;
        f << "*BOUNDARY\nBC_" << sanitize_label(s.face_label) << ", 1, 3, 0.0\n";
    }

    // Displacement loads — prescribe UX, UY, UZ separately
    for (const auto& dl : state.displacement_loads) {
        auto* ns = find_nset(dl.face_label);
        if (!ns || ns->node_ids.empty()) continue;
        const std::string nset = "BC_" + sanitize_label(dl.face_label);
        f << "*BOUNDARY\n";
        f << nset << ", 1, 1, " << dl.ux << "\n";
        f << nset << ", 2, 2, " << dl.uy << "\n";
        f << nset << ", 3, 3, " << dl.uz << "\n";
    }

    // Force loads — distributed CLOAD (N per node)
    for (const auto& fl : state.force_loads) {
        auto* ns = find_nset(fl.face_label);
        if (!ns || ns->node_ids.empty()) continue;
        const int n = (int)ns->node_ids.size();
        const std::string nset = "BC_" + sanitize_label(fl.face_label);
        f << "*CLOAD\n";
        if (fl.fx != 0.f) f << nset << ", 1, " << (fl.fx / n) << "\n";
        if (fl.fy != 0.f) f << nset << ", 2, " << (fl.fy / n) << "\n";
        if (fl.fz != 0.f) f << nset << ", 3, " << (fl.fz / n) << "\n";
    }

    // Output — nodal displacements and reaction forces to .frd.
    // Element stress is requested for future direct CalculiX result import;
    // the current app still performs its own nodal stress recovery.
    f << "*NODE FILE\nU, RF\n";
    f << "*EL FILE\nS\n";
    f << "*END STEP\n";
}

// ── FRD parser — extract displacements from CalculiX result file ──────────────
//
// ASCII FRD structure (relevant parts):
//   100CL  101 ...         ← result block header
//    -4  DISP   4   1      ← identifies displacement block
//    -5  D1 ...            ← component labels (ignored here)
//    -1  node_id  ux uy uz ← data records (3 floats for C3D4)
//    -3                    ← end of block

static bool parse_frd_displacements(
    const fs::path& frd_path,
    int n_nodes,
    std::vector<double>& disp   // length 3*n_nodes, UX/UY/UZ per node
) {
    std::ifstream f(frd_path);
    if (!f.is_open()) return false;

    disp.assign(3 * (size_t)n_nodes, 0.0);

    std::string line;
    bool in_disp = false;
    int found = 0;

    while (std::getline(f, line)) {
        // Detect start of displacement result block: line contains "-4" and "DISP"
        if (!in_disp) {
            if (line.size() >= 4 &&
                line.substr(0, 4) == " -4 " &&
                line.find("DISP") != std::string::npos) {
                in_disp = true;
            }
            continue;
        }

        // Skip component-label lines (start with " -5")
        if (line.size() >= 3 && line[1] == '-' && line[2] == '5') continue;

        // End of block
        if (line.size() >= 3 && line[1] == '-' && line[2] == '3') break;

        // Data record: " -1" + 10-char node_id + three 12-char floats
        // Fixed-width format handles concatenated negative values (e.g. "1.0E+00-2.0E-01")
        if (line.size() >= 3 && line[1] == '-' && line[2] == '1') {
            if (line.size() < 3 + 10 + 36) continue; // too short
            try {
                const int node_id = std::stoi(line.substr(3, 10));
                const double ux   = std::stod(line.substr(13, 12));
                const double uy   = std::stod(line.substr(25, 12));
                const double uz   = std::stod(line.substr(37, 12));
                const int ni = node_id - 1;
                if (ni >= 0 && ni < n_nodes) {
                    disp[3*ni+0] = ux;
                    disp[3*ni+1] = uy;
                    disp[3*ni+2] = uz;
                    ++found;
                }
            } catch (...) {}
        }
    }

    return found > 0;
}

// ── FRD parser — extract reaction forces (FORC block) ────────────────────────

static void parse_frd_reaction_forces(
    const fs::path& frd_path,
    int n_nodes,
    std::vector<double>& rf   // length 3*n_nodes, RFX/RFY/RFZ per node
) {
    std::ifstream f(frd_path);
    if (!f.is_open()) return;

    rf.assign(3 * (size_t)n_nodes, 0.0);

    std::string line;
    bool in_rf = false;

    while (std::getline(f, line)) {
        if (!in_rf) {
            if (line.size() >= 4 &&
                line.substr(0, 4) == " -4 " &&
                line.find("FORC") != std::string::npos) {
                in_rf = true;
            }
            continue;
        }

        if (line.size() >= 3 && line[1] == '-' && line[2] == '5') continue;
        if (line.size() >= 3 && line[1] == '-' && line[2] == '3') break;

        if (line.size() >= 3 && line[1] == '-' && line[2] == '1') {
            if (line.size() < 3 + 10 + 36) continue;
            try {
                const int node_id = std::stoi(line.substr(3, 10));
                const double rx   = std::stod(line.substr(13, 12));
                const double ry   = std::stod(line.substr(25, 12));
                const double rz   = std::stod(line.substr(37, 12));
                const int ni = node_id - 1;
                if (ni >= 0 && ni < n_nodes) {
                    rf[3*ni+0] = rx;
                    rf[3*ni+1] = ry;
                    rf[3*ni+2] = rz;
                }
            } catch (...) {}
        }
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

LinearSolveResult solve_via_ccx(
    const geometry::VolumeMeshData& mesh,
    const ProjectState& state,
    const std::string& ccx_binary,
    ProgressCallback progress_callback
) {
    LinearSolveResult out;

    if (mesh.nodes.empty() || mesh.tets.empty()) {
        out.message = "CalculiX: empty mesh.";
        return out;
    }

    // ── 1. Create temporary working directory ─────────────────────────────────
    char tmpdir_buf[] = "/tmp/tpms_ccx_XXXXXX";
    if (!mkdtemp(tmpdir_buf)) {
        out.message = "CalculiX: could not create temp dir.";
        return out;
    }
    const fs::path tmpdir(tmpdir_buf);
    const fs::path inp_path  = tmpdir / "job.inp";
    const fs::path frd_path  = tmpdir / "job.frd";
    const fs::path log_path  = tmpdir / "ccx.log";

    // ── 2. Write .inp ─────────────────────────────────────────────────────────
    if (progress_callback) progress_callback(0, 1.0);
    try {
        write_inp(inp_path, mesh, state);
    } catch (const std::exception& e) {
        out.message = std::string("CalculiX: INP write error: ") + e.what();
        return out;
    }

    // ── 3. Run CalculiX ───────────────────────────────────────────────────────
    if (progress_callback) progress_callback(1, 0.5);

    const std::string lib_dir = ccx_lib_dir(ccx_binary);
    // Build shell command: cd to tmpdir, set LD_LIBRARY_PATH only for local
    // developer drops, and run ccx. Keep all paths quoted for portability.
    std::ostringstream cmd;
    cmd << "cd " << shell_quote(tmpdir.string()) << " && ";
    if (!lib_dir.empty()) {
        cmd << "LD_LIBRARY_PATH=" << shell_quote(lib_dir) << " ";
    }
    cmd << shell_quote(ccx_binary)
        << " -i job"
        << " > " << shell_quote(log_path.string())
        << " 2>&1";

    const int ret = std::system(cmd.str().c_str());

    if (progress_callback) progress_callback(2, 0.1);

    // ── 4. Parse .frd displacements ───────────────────────────────────────────
    const int n_nodes = (int)mesh.nodes.size();
    std::vector<double> disp;

    if (!parse_frd_displacements(frd_path, n_nodes, disp)) {
        // Read log for diagnostics
        std::string log_text;
        if (std::ifstream lf(log_path); lf.is_open()) {
            std::ostringstream ss; ss << lf.rdbuf();
            log_text = ss.str().substr(0, 400);
        }
        out.message = "CalculiX: no displacement data in output"
                      + (log_text.empty() ? "" : (" — " + log_text));
        (void)ret;
        return out;
    }

    // ── 5. Fill LinearSolveResult ─────────────────────────────────────────────
    out.ok        = true;
    out.converged = true;
    out.iterations = 1;
    out.initial_residual = 1.0;
    out.final_residual   = 0.0;
    out.message = "CalculiX linear static: converged.";
    out.residual_history = {1.0f, 0.0f};

    std::vector<double> rf;
    parse_frd_reaction_forces(frd_path, n_nodes, rf);

    out.displacement = disp;
    out.displacement_magnitude.resize((size_t)n_nodes, 0.f);
    out.displacement_x.resize((size_t)n_nodes, 0.f);
    out.displacement_y.resize((size_t)n_nodes, 0.f);
    out.displacement_z.resize((size_t)n_nodes, 0.f);
    out.reaction_force_magnitude.resize((size_t)n_nodes, 0.f);

    for (int i = 0; i < n_nodes; ++i) {
        const double ux = disp[3*i+0];
        const double uy = disp[3*i+1];
        const double uz = disp[3*i+2];
        out.displacement_x[i] = static_cast<float>(ux);
        out.displacement_y[i] = static_cast<float>(uy);
        out.displacement_z[i] = static_cast<float>(uz);
        out.displacement_magnitude[i] =
            static_cast<float>(std::sqrt(ux*ux + uy*uy + uz*uz));

        if (!rf.empty()) {
            const double rx = rf[3*i+0];
            const double ry = rf[3*i+1];
            const double rz = rf[3*i+2];
            out.reaction_force_magnitude[i] =
                static_cast<float>(std::sqrt(rx*rx + ry*ry + rz*rz));
        }
    }

    if (progress_callback) progress_callback(3, 0.0);
    return out;
}

} // namespace tpms::solve
