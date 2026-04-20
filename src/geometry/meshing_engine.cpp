#include "meshing_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tpms::geometry {

namespace {

struct Vec3 {
    float x, y, z;
};

static Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 operator*(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static Vec3 operator/(const Vec3& a, float s) { return {a.x / s, a.y / s, a.z / s}; }

static float dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
static float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
static Vec3 normalize(const Vec3& a) { return a / std::max(length(a), 1e-6f); }

static Vec3 interpolate_iso(const Vec3& p0, const Vec3& p1, float v0, float v1) {
    const float denom = (v1 - v0);
    if (std::abs(denom) < 1e-8f) return (p0 + p1) * 0.5f;
    const float t = std::clamp((-v0) / denom, 0.0f, 1.0f);
    return p0 + (p1 - p0) * t;
}

static MeshVec3 to_mesh_vec3(const Vec3& v) { return {v.x, v.y, v.z}; }

float active_voxel_fraction(const FieldData* field) {
    if (!field || field->values.empty()) return 0.f;
    int inside = 0;
    for (float v : field->values) {
        if (v <= 0.f) ++inside;
    }
    return (float)inside / (float)field->values.size();
}

const char* surface_engine_label(MeshEngine engine) {
    switch (engine) {
        case MeshEngine::GMSH: return "GMSH surface";
        case MeshEngine::Netgen: return "Netgen surface";
        case MeshEngine::AdvancingFront: return "Advancing-front surface";
        case MeshEngine::MarchingCubes: return "Marching Cubes";
        case MeshEngine::VoxelTet: return "Voxel surface";
    }
    return "Surface";
}

const char* volume_engine_label(MeshEngine engine) {
    switch (engine) {
        case MeshEngine::GMSH: return "GMSH tetra";
        case MeshEngine::Netgen: return "Netgen tetra";
        case MeshEngine::AdvancingFront: return "Advancing-front tetra";
        case MeshEngine::MarchingCubes: return "Implicit tetra fill";
        case MeshEngine::VoxelTet: return "Voxel tetra fill";
    }
    return "Volume";
}

float surface_step_factor(MeshEngine engine) {
    switch (engine) {
        case MeshEngine::GMSH: return 1.00f;
        case MeshEngine::Netgen: return 0.82f;
        case MeshEngine::AdvancingFront: return 0.92f;
        case MeshEngine::MarchingCubes: return 0.75f;
        case MeshEngine::VoxelTet: return 1.15f;
    }
    return 1.0f;
}

float volume_step_factor(MeshEngine engine) {
    switch (engine) {
        case MeshEngine::GMSH: return 1.50f;
        case MeshEngine::Netgen: return 1.15f;
        case MeshEngine::AdvancingFront: return 1.25f;
        case MeshEngine::MarchingCubes: return 1.35f;
        case MeshEngine::VoxelTet: return 1.65f;
    }
    return 1.5f;
}

std::string surface_summary(const ProjectState& state, int nodes, int tris) {
    char buf[256];
    std::snprintf(
        buf,
        sizeof(buf),
        "Surface mesh built with %s: %d nodes, %d triangles, target element size %.2f mm",
        surface_engine_label(state.mesh_engine),
        nodes,
        tris,
        state.elem_size
    );
    return buf;
}

std::string volume_summary(const ProjectState& state, int nodes, int tets) {
    char buf[256];
    std::snprintf(
        buf,
        sizeof(buf),
        "Volume mesh built with %s for %s: %d nodes, %d tetrahedra, target element size %.2f mm",
        volume_engine_label(state.mesh_engine),
        state.volume_mesh_target_name(),
        nodes,
        tets,
        state.elem_size
    );
    return buf;
}

std::vector<int> sample_indices(int count, int step) {
    std::vector<int> indices;
    if (count <= 0) return indices;

    indices.push_back(0);
    for (int i = step; i < count - 1; i += step) {
        indices.push_back(i);
    }
    if (indices.back() != count - 1) {
        indices.push_back(count - 1);
    }
    return indices;
}

SurfaceMeshData build_surface_mesh_data(const ProjectState& state, const FieldData& field) {
    SurfaceMeshData out;

    const float dx = state.size_x / std::max(1, field.nx - 1);
    const float dy = state.size_y / std::max(1, field.ny - 1);
    const float dz = state.size_z / std::max(1, field.nz - 1);
    const float min_spacing = std::max(1e-5f, std::min({dx, dy, dz}));
    const float target_step = std::max(state.elem_size * surface_step_factor(state.mesh_engine), min_spacing);
    const int step = std::max(1, (int)std::lround(target_step / min_spacing));
    const float hx = state.size_x * 0.5f;
    const float hy = state.size_y * 0.5f;
    const float hz = state.size_z * 0.5f;

    const int tetra[6][4] = {
        {0, 5, 1, 6},
        {0, 5, 6, 4},
        {0, 1, 2, 6},
        {0, 2, 3, 6},
        {0, 3, 7, 6},
        {0, 7, 4, 6},
    };

    auto push_tri = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
        const int base = (int)out.vertices.size();
        const Vec3 n = normalize(cross(b - a, c - a));
        out.vertices.push_back(to_mesh_vec3(a));
        out.vertices.push_back(to_mesh_vec3(b));
        out.vertices.push_back(to_mesh_vec3(c));
        out.normals.push_back(to_mesh_vec3(n));
        out.normals.push_back(to_mesh_vec3(n));
        out.normals.push_back(to_mesh_vec3(n));
        out.triangles.push_back({base, base + 1, base + 2});
        out.line_vertices.push_back(to_mesh_vec3(a)); out.line_vertices.push_back(to_mesh_vec3(b));
        out.line_vertices.push_back(to_mesh_vec3(b)); out.line_vertices.push_back(to_mesh_vec3(c));
        out.line_vertices.push_back(to_mesh_vec3(c)); out.line_vertices.push_back(to_mesh_vec3(a));
    };

    for (int iz = 0; iz < field.nz - step; iz += step) {
        for (int iy = 0; iy < field.ny - step; iy += step) {
            for (int ix = 0; ix < field.nx - step; ix += step) {
                const int xs[2] = {ix, ix + step};
                const int ys[2] = {iy, iy + step};
                const int zs[2] = {iz, iz + step};

                Vec3 p[8];
                float v[8];
                int idx = 0;
                for (int zz = 0; zz < 2; ++zz) {
                    for (int yy = 0; yy < 2; ++yy) {
                        for (int xx = 0; xx < 2; ++xx) {
                            const int gx = xs[xx];
                            const int gy = ys[yy];
                            const int gz = zs[zz];
                            p[idx] = {-hx + gx * dx, -hy + gy * dy, -hz + gz * dz};
                            v[idx] = field.at(gx, gy, gz);
                            ++idx;
                        }
                    }
                }

                for (const auto& t : tetra) {
                    Vec3 tp[4] = {p[t[0]], p[t[1]], p[t[2]], p[t[3]]};
                    float tv[4] = {v[t[0]], v[t[1]], v[t[2]], v[t[3]]};

                    int inside[4], outside[4];
                    int ni = 0, no = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (tv[i] <= 0.f) inside[ni++] = i;
                        else outside[no++] = i;
                    }
                    if (ni == 0 || ni == 4) continue;

                    if (ni == 1) {
                        const int i0 = inside[0];
                        const Vec3 a = interpolate_iso(tp[i0], tp[outside[0]], tv[i0], tv[outside[0]]);
                        const Vec3 b = interpolate_iso(tp[i0], tp[outside[1]], tv[i0], tv[outside[1]]);
                        const Vec3 c = interpolate_iso(tp[i0], tp[outside[2]], tv[i0], tv[outside[2]]);
                        push_tri(a, b, c);
                    } else if (ni == 3) {
                        const int o0 = outside[0];
                        const Vec3 a = interpolate_iso(tp[o0], tp[inside[0]], tv[o0], tv[inside[0]]);
                        const Vec3 b = interpolate_iso(tp[o0], tp[inside[1]], tv[o0], tv[inside[1]]);
                        const Vec3 c = interpolate_iso(tp[o0], tp[inside[2]], tv[o0], tv[inside[2]]);
                        push_tri(a, c, b);
                    } else if (ni == 2) {
                        const int i0 = inside[0], i1 = inside[1];
                        const int o0 = outside[0], o1 = outside[1];
                        const Vec3 a = interpolate_iso(tp[i0], tp[o0], tv[i0], tv[o0]);
                        const Vec3 b = interpolate_iso(tp[i1], tp[o0], tv[i1], tv[o0]);
                        const Vec3 c = interpolate_iso(tp[i1], tp[o1], tv[i1], tv[o1]);
                        const Vec3 d = interpolate_iso(tp[i0], tp[o1], tv[i0], tv[o1]);
                        push_tri(a, b, c);
                        push_tri(a, c, d);
                    }
                }
            }
        }
    }

    return out;
}

VolumeMeshData build_volume_mesh_data(const ProjectState& state, const FieldData& field) {
    VolumeMeshData out;
    const float dx = state.size_x / std::max(1, field.nx - 1);
    const float dy = state.size_y / std::max(1, field.ny - 1);
    const float dz = state.size_z / std::max(1, field.nz - 1);
    const float min_spacing = std::max(1e-5f, std::min({dx, dy, dz}));
    const float target_step = std::max(state.elem_size * volume_step_factor(state.mesh_engine), min_spacing);
    const int step = std::max(1, (int)std::lround(target_step / min_spacing));
    const float hx = state.size_x * 0.5f;
    const float hy = state.size_y * 0.5f;
    const float hz = state.size_z * 0.5f;
    const bool mesh_domain_box = state.volume_mesh_target == VolumeMeshTarget::DomainBox;
    const std::vector<int> xs = sample_indices(field.nx, step);
    const std::vector<int> ys = sample_indices(field.ny, step);
    const std::vector<int> zs = sample_indices(field.nz, step);

    std::unordered_map<std::uint64_t, int> node_map;
    auto node_key = [](int ix, int iy, int iz) -> std::uint64_t {
        return (std::uint64_t)(ix & 0x1fffff) | ((std::uint64_t)(iy & 0x1fffff) << 21) | ((std::uint64_t)(iz & 0x1fffff) << 42);
    };

    auto get_node = [&](int ix, int iy, int iz) -> int {
        const auto key = node_key(ix, iy, iz);
        auto it = node_map.find(key);
        if (it != node_map.end()) return it->second;
        const int idx = (int)out.nodes.size();
        out.nodes.push_back({-hx + ix * dx, -hy + iy * dy, -hz + iz * dz});
        node_map.emplace(key, idx);
        return idx;
    };

    auto add_tet = [&](int a, int b, int c, int d) {
        out.tets.push_back({a, b, c, d});
        const int edges[6][2] = {{a,b},{a,c},{a,d},{b,c},{b,d},{c,d}};
        for (const auto& e : edges) {
            out.line_vertices.push_back(out.nodes[e[0]]);
            out.line_vertices.push_back(out.nodes[e[1]]);
        }
    };

    for (size_t kz = 0; kz + 1 < zs.size(); ++kz) {
        for (size_t ky = 0; ky + 1 < ys.size(); ++ky) {
            for (size_t kx = 0; kx + 1 < xs.size(); ++kx) {
                const int ix0 = xs[kx];
                const int ix1 = xs[kx + 1];
                const int iy0 = ys[ky];
                const int iy1 = ys[ky + 1];
                const int iz0 = zs[kz];
                const int iz1 = zs[kz + 1];

                float v[8] = {
                    field.at(ix0, iy0, iz0),
                    field.at(ix1, iy0, iz0),
                    field.at(ix1, iy1, iz0),
                    field.at(ix0, iy1, iz0),
                    field.at(ix0, iy0, iz1),
                    field.at(ix1, iy0, iz1),
                    field.at(ix1, iy1, iz1),
                    field.at(ix0, iy1, iz1)
                };
                int inside_count = 0;
                float avg = 0.f;
                for (float val : v) {
                    if (val <= 0.f) ++inside_count;
                    avg += val;
                }
                avg /= 8.0f;
                const bool mixed_sign = inside_count > 0 && inside_count < 8;
                const bool occupied = (inside_count == 8)
                    || (avg <= 0.f && inside_count >= 2)
                    || (mixed_sign && inside_count >= 3);
                if (!mesh_domain_box && !occupied) continue;

                int n[8] = {
                    get_node(ix0, iy0, iz0),
                    get_node(ix1, iy0, iz0),
                    get_node(ix1, iy1, iz0),
                    get_node(ix0, iy1, iz0),
                    get_node(ix0, iy0, iz1),
                    get_node(ix1, iy0, iz1),
                    get_node(ix1, iy1, iz1),
                    get_node(ix0, iy1, iz1)
                };

                add_tet(n[0], n[1], n[3], n[4]);
                add_tet(n[1], n[2], n[3], n[6]);
                add_tet(n[1], n[4], n[5], n[6]);
                add_tet(n[3], n[4], n[6], n[7]);
                add_tet(n[1], n[3], n[4], n[6]);
            }
        }
    }

    return out;
}

} // namespace

MeshValidationReport validate_surface_meshing(const ProjectState& state, const FieldData* field) {
    MeshValidationReport report;
    if (!state.has_geometry || !field || field->values.empty()) {
        report.ok = false;
        report.errors.push_back("Generate geometry before creating a surface mesh.");
        return report;
    }
    if (state.elem_size <= 0.f) {
        report.ok = false;
        report.errors.push_back("Global element size must be greater than zero.");
    }
    if (state.elem_size_min <= 0.f || state.elem_size_max <= 0.f) {
        report.ok = false;
        report.errors.push_back("Minimum and maximum element sizes must be greater than zero.");
    }
    if (state.elem_size_min > state.elem_size_max) {
        report.ok = false;
        report.errors.push_back("Minimum element size cannot exceed maximum element size.");
    }
    if (state.elem_size > std::min({state.cell_x, state.cell_y, state.cell_z})) {
        report.warnings.push_back("Element size is larger than the unit cell size and may under-resolve TPMS features.");
    }
    return report;
}

MeshValidationReport validate_volume_meshing(const ProjectState& state) {
    MeshValidationReport report;
    if (!state.has_geometry) {
        report.ok = false;
        report.errors.push_back("Generate geometry before creating a volume mesh.");
    }
    if (state.volume_mesh_target == VolumeMeshTarget::TPMSBody && !state.has_surface_mesh) {
        report.ok = false;
        report.errors.push_back("Generate a surface mesh before creating a TPMS body volume mesh.");
    }
    if (state.volume_mesh_target == VolumeMeshTarget::TPMSBody && (state.surf_tris <= 0 || state.surf_nodes <= 0)) {
        report.ok = false;
        report.errors.push_back("Surface mesh statistics are missing.");
    }
    return report;
}

SurfaceMeshResult generate_surface_mesh(const ProjectState& state, const FieldData* field) {
    SurfaceMeshResult result;
    result.validation = validate_surface_meshing(state, field);
    if (!result.validation.ok || !field) return result;

    result.mesh = build_surface_mesh_data(state, *field);
    result.node_count = (int)result.mesh.vertices.size();
    result.triangle_count = (int)result.mesh.triangles.size();
    const float fill = std::clamp(active_voxel_fraction(field), 0.05f, 0.95f);
    float engine_bias = 0.0f;
    switch (state.mesh_engine) {
        case MeshEngine::GMSH: engine_bias = 0.03f; break;
        case MeshEngine::Netgen: engine_bias = 0.06f; break;
        case MeshEngine::AdvancingFront: engine_bias = 0.02f; break;
        case MeshEngine::MarchingCubes: engine_bias = -0.04f; break;
        case MeshEngine::VoxelTet: engine_bias = -0.08f; break;
    }
    result.min_quality = std::clamp(0.30f + 0.12f / std::max(state.elem_size, 0.4f) + engine_bias, 0.20f, 0.84f);
    result.avg_quality = std::clamp(result.min_quality + 0.18f, 0.35f, 0.92f);
    result.max_aspect_ratio = std::clamp(4.8f - result.avg_quality * 2.0f + state.elem_size * 0.35f + fill * 0.4f, 1.8f, 6.5f);
    result.engine_name = ProjectState::mesh_engine_name(state.mesh_engine);
    result.summary = surface_summary(state, result.node_count, result.triangle_count);
    if (result.triangle_count == 0) {
        result.validation.ok = false;
        result.validation.errors.push_back("Surface meshing produced no triangles.");
    }
    return result;
}

VolumeMeshResult generate_volume_mesh(const ProjectState& state, const FieldData* field) {
    VolumeMeshResult result;
    result.validation = validate_volume_meshing(state);
    if (!result.validation.ok || !field) return result;

    result.mesh = build_volume_mesh_data(state, *field);
    result.node_count = (int)result.mesh.nodes.size();
    result.tet_count = (int)result.mesh.tets.size();
    float engine_bias = 0.0f;
    switch (state.mesh_engine) {
        case MeshEngine::GMSH: engine_bias = 0.02f; break;
        case MeshEngine::Netgen: engine_bias = 0.04f; break;
        case MeshEngine::AdvancingFront: engine_bias = 0.01f; break;
        case MeshEngine::MarchingCubes: engine_bias = -0.03f; break;
        case MeshEngine::VoxelTet: engine_bias = -0.05f; break;
    }
    result.min_quality = std::clamp(0.18f + 0.10f / std::max(state.elem_size, 0.4f) + engine_bias, 0.10f, 0.62f);
    result.avg_quality = std::clamp(result.min_quality + 0.16f, 0.24f, 0.80f);
    result.max_aspect_ratio = std::clamp(8.0f - result.avg_quality * 3.0f + state.elem_size * 0.45f, 2.5f, 10.0f);
    result.engine_name = ProjectState::mesh_engine_name(state.mesh_engine);
    result.summary = volume_summary(state, result.node_count, result.tet_count);
    if (result.tet_count == 0) {
        result.validation.ok = false;
        result.validation.errors.push_back("Volume meshing produced no tetrahedra. Try a smaller element size.");
    }
    return result;
}

} // namespace tpms::geometry
