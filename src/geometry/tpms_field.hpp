#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <stdexcept>
#include "../state/project_state.hpp"

namespace tpms::geometry {

// ── Scalar field data ─────────────────────────────────────────────────────────

struct FieldData {
    int nx = 0, ny = 0, nz = 0;
    float size_x = 0, size_y = 0, size_z = 0;

    std::vector<float> values;   // [ix + nx*(iy + ny*iz)]

    float& at(int ix, int iy, int iz)       { return values[ix + nx*(iy + ny*iz)]; }
    float  at(int ix, int iy, int iz) const { return values[ix + nx*(iy + ny*iz)]; }

    bool empty() const { return values.empty(); }

    // XY mid-slice (z = nz/2)
    std::vector<float> slice_xy() const {
        int iz = nz / 2;
        std::vector<float> s(nx * ny);
        for (int iy = 0; iy < ny; ++iy)
            for (int ix = 0; ix < nx; ++ix)
                s[ix + nx * iy] = at(ix, iy, iz);
        return s;
    }

    std::vector<float> slice_xz() const {
        int iy = ny / 2;
        std::vector<float> s(nx * nz);
        for (int iz = 0; iz < nz; ++iz)
            for (int ix = 0; ix < nx; ++ix)
                s[ix + nx * iz] = at(ix, iy, iz);
        return s;
    }

    std::vector<float> slice_yz() const {
        int ix = nx / 2;
        std::vector<float> s(ny * nz);
        for (int iz = 0; iz < nz; ++iz)
            for (int iy = 0; iy < ny; ++iy)
                s[iy + ny * iz] = at(ix, iy, iz);
        return s;
    }

    float field_min() const {
        float m = values[0];
        for (float v : values) m = std::min(m, v);
        return m;
    }
    float field_max() const {
        float m = values[0];
        for (float v : values) m = std::max(m, v);
        return m;
    }
};

// ── TPMS base equations ───────────────────────────────────────────────────────

inline float eval_tpms(TPMSDesign design, float X, float Y, float Z) {
    switch (design) {
    case TPMSDesign::Gyroid:
        return std::cos(X)*std::sin(Y) + std::cos(Y)*std::sin(Z) + std::cos(Z)*std::sin(X);

    case TPMSDesign::Diamond:
        return std::sin(X)*std::sin(Y)*std::sin(Z)
             + std::sin(X)*std::cos(Y)*std::cos(Z)
             + std::cos(X)*std::sin(Y)*std::cos(Z)
             + std::cos(X)*std::cos(Y)*std::sin(Z);

    case TPMSDesign::SchwarzP:
        return std::cos(X) + std::cos(Y) + std::cos(Z);

    case TPMSDesign::Lidinoid:
        return std::sin(2*X)*std::cos(Y)*std::sin(Z)
             + std::sin(X)*std::sin(2*Y)*std::cos(Z)
             + std::cos(X)*std::sin(Y)*std::sin(2*Z)
             - std::cos(2*X)*std::cos(2*Y)
             - std::cos(2*Y)*std::cos(2*Z)
             - std::cos(2*Z)*std::cos(2*X)
             + 0.3f;

    case TPMSDesign::Neovius:
        return 3.0f * (std::cos(X) + std::cos(Y) + std::cos(Z))
             + 4.0f * std::cos(X) * std::cos(Y) * std::cos(Z);
    }
    return 0.f;
}

inline float eval_tpms_physical(
    TPMSDesign design,
    float x,
    float y,
    float z,
    float kx,
    float ky,
    float kz
) {
    return eval_tpms(design, kx * x, ky * y, kz * z);
}

inline float eval_shell_distance(
    const ProjectState& state,
    float x,
    float y,
    float z,
    float kx,
    float ky,
    float kz
) {
    const float base = eval_tpms_physical(state.design, x, y, z, kx, ky, kz);

    // Approximate a physical wall thickness by locally normalizing the field
    // with its gradient magnitude before offsetting by the requested thickness.
    const float min_cell = std::min(state.cell_x, std::min(state.cell_y, state.cell_z));
    const float eps = 0.25f * min_cell;
    const float fx1 = eval_tpms_physical(state.design, x + eps, y, z, kx, ky, kz);
    const float fx0 = eval_tpms_physical(state.design, x - eps, y, z, kx, ky, kz);
    const float fy1 = eval_tpms_physical(state.design, x, y + eps, z, kx, ky, kz);
    const float fy0 = eval_tpms_physical(state.design, x, y - eps, z, kx, ky, kz);
    const float fz1 = eval_tpms_physical(state.design, x, y, z + eps, kx, ky, kz);
    const float fz0 = eval_tpms_physical(state.design, x, y, z - eps, kx, ky, kz);

    const float gx = (fx1 - fx0) / (2.0f * eps);
    const float gy = (fy1 - fy0) / (2.0f * eps);
    const float gz = (fz1 - fz0) / (2.0f * eps);
    const float grad_mag = std::max(1e-5f, std::sqrt(gx * gx + gy * gy + gz * gz));

    return std::abs(base) / grad_mag - 0.5f * state.wall_thickness;
}

// ── SDF of axis-aligned box centred at origin ─────────────────────────────────

inline float sdf_box(float x, float y, float z, float hx, float hy, float hz) {
    float qx = std::abs(x) - hx;
    float qy = std::abs(y) - hy;
    float qz = std::abs(z) - hz;
    float outside = std::sqrt(std::max(qx,0.f)*std::max(qx,0.f)
                            + std::max(qy,0.f)*std::max(qy,0.f)
                            + std::max(qz,0.f)*std::max(qz,0.f));
    float inside  = std::min(std::max(qx, std::max(qy, qz)), 0.f);
    return outside + inside;
}

inline float sdf_cylinder_z(float x, float y, float z, float radius, float half_height) {
    const float radial = std::sqrt(x * x + y * y) - radius;
    const float axial = std::abs(z) - half_height;
    const float outside = std::sqrt(std::max(radial, 0.f) * std::max(radial, 0.f)
                                  + std::max(axial, 0.f) * std::max(axial, 0.f));
    const float inside = std::min(std::max(radial, axial), 0.f);
    return outside + inside;
}

inline float sdf_sphere(float x, float y, float z, float radius) {
    return std::sqrt(x * x + y * y + z * z) - radius;
}

inline float domain_sdf(const ProjectState& state, float x, float y, float z, float hx, float hy, float hz) {
    switch (state.domain) {
        case DomainType::Cuboid:
            return sdf_box(x, y, z, hx, hy, hz);
        case DomainType::Cylinder:
            return sdf_cylinder_z(x, y, z, std::min(hx, hy), hz);
        case DomainType::Sphere:
            return sdf_sphere(x, y, z, std::min({hx, hy, hz}));
    }
    return sdf_box(x, y, z, hx, hy, hz);
}

// ── Field sampler ─────────────────────────────────────────────────────────────

inline FieldData sample_field(const ProjectState& state, int border_voxels = 0) {
    const int n = state.resolution;
    const float sx = state.size_x, sy = state.size_y, sz = state.size_z;
    const float cx = state.cell_x, cy = state.cell_y, cz = state.cell_z;
    const float ox = state.origin_x, oy = state.origin_y, oz = state.origin_z;

    const float kx = 2.f * 3.14159265f / cx;
    const float ky = 2.f * 3.14159265f / cy;
    const float kz = 2.f * 3.14159265f / cz;

    const float dx = sx / std::max(1, n - 1);
    const float dy = sy / std::max(1, n - 1);
    const float dz = sz / std::max(1, n - 1);

    const int b  = border_voxels;
    const int nx = n + 2*b;
    const int ny = n + 2*b;
    const int nz = n + 2*b;

    FieldData fd;
    fd.nx = nx; fd.ny = ny; fd.nz = nz;
    fd.size_x = sx; fd.size_y = sy; fd.size_z = sz;
    fd.values.resize(nx * ny * nz);

    const float hx = sx / 2, hy = sy / 2, hz = sz / 2;

    for (int iz = 0; iz < nz; ++iz) {
        float z = -hz - b*dz + iz*dz + oz;
        for (int iy = 0; iy < ny; ++iy) {
            float y = -hy - b*dy + iy*dy + oy;
            for (int ix = 0; ix < nx; ++ix) {
                float x = -hx - b*dx + ix*dx + ox;

                const float shell = eval_shell_distance(state, x, y, z, kx, ky, kz);

                const float base = eval_tpms_physical(state.design, x, y, z, kx, ky, kz);
                const float sdf = domain_sdf(state, x, y, z, hx, hy, hz);
                if (state.geometry_mode == GeometryMode::Solid) {
                    fd.at(ix, iy, iz) = std::max(base, sdf);
                } else {
                    fd.at(ix, iy, iz) = std::max(shell, sdf);
                }
            }
        }
    }
    return fd;
}

} // namespace tpms::geometry
