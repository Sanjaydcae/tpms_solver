#include "fem_core.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace tpms::fem {
namespace {

bool tet4_shape_gradients(
    const geometry::MeshVec3& p0,
    const geometry::MeshVec3& p1,
    const geometry::MeshVec3& p2,
    const geometry::MeshVec3& p3,
    double dNdx[4][3]
) {
    const double J[3][3] = {
        {p1.x - p0.x, p2.x - p0.x, p3.x - p0.x},
        {p1.y - p0.y, p2.y - p0.y, p3.y - p0.y},
        {p1.z - p0.z, p2.z - p0.z, p3.z - p0.z},
    };

    const double detJ =
        J[0][0] * (J[1][1] * J[2][2] - J[1][2] * J[2][1])
      - J[0][1] * (J[1][0] * J[2][2] - J[1][2] * J[2][0])
      + J[0][2] * (J[1][0] * J[2][1] - J[1][1] * J[2][0]);
    if (!std::isfinite(detJ) || std::abs(detJ) < 1e-20) {
        return false;
    }

    const double id = 1.0 / detJ;
    double iJ[3][3];
    iJ[0][0] =  (J[1][1] * J[2][2] - J[1][2] * J[2][1]) * id;
    iJ[0][1] = -(J[0][1] * J[2][2] - J[0][2] * J[2][1]) * id;
    iJ[0][2] =  (J[0][1] * J[1][2] - J[0][2] * J[1][1]) * id;
    iJ[1][0] = -(J[1][0] * J[2][2] - J[1][2] * J[2][0]) * id;
    iJ[1][1] =  (J[0][0] * J[2][2] - J[0][2] * J[2][0]) * id;
    iJ[1][2] = -(J[0][0] * J[1][2] - J[0][2] * J[1][0]) * id;
    iJ[2][0] =  (J[1][0] * J[2][1] - J[1][1] * J[2][0]) * id;
    iJ[2][1] = -(J[0][0] * J[2][1] - J[0][1] * J[2][0]) * id;
    iJ[2][2] =  (J[0][0] * J[1][1] - J[0][1] * J[1][0]) * id;

    const double dNdxi[4][3] = {
        {-1.0, -1.0, -1.0},
        { 1.0,  0.0,  0.0},
        { 0.0,  1.0,  0.0},
        { 0.0,  0.0,  1.0},
    };

    for (int a = 0; a < 4; ++a) {
        for (int j = 0; j < 3; ++j) {
            dNdx[a][j] = 0.0;
            for (int k = 0; k < 3; ++k) {
                dNdx[a][j] += dNdxi[a][k] * iJ[k][j];
            }
        }
    }
    return true;
}

double von_mises_from_stress(
    double sx,
    double sy,
    double sz,
    double txy,
    double tyz,
    double txz
) {
    const double normal =
        (sx - sy) * (sx - sy) +
        (sy - sz) * (sy - sz) +
        (sz - sx) * (sz - sx);
    const double shear = 6.0 * (txy * txy + tyz * tyz + txz * txz);
    return std::sqrt(std::max(0.0, 0.5 * (normal + shear)));
}

double equivalent_strain_from_components(
    double ex,
    double ey,
    double ez,
    double gxy,
    double gyz,
    double gxz
) {
    const double normal =
        (ex - ey) * (ex - ey) +
        (ey - ez) * (ey - ez) +
        (ez - ex) * (ez - ex);
    const double shear = 1.5 * (gxy * gxy + gyz * gyz + gxz * gxz);
    return std::sqrt(std::max(0.0, (2.0 / 3.0) * normal + shear));
}

} // namespace

StressRecoveryResult recover_tet4_von_mises(
    const geometry::VolumeMeshData& mesh,
    const std::vector<double>& displacement,
    double young_modulus,
    double poisson_ratio
) {
    StressRecoveryResult out;
    const int n_nodes = (int)mesh.nodes.size();
    const int n_tets = (int)mesh.tets.size();

    if (n_nodes <= 0 || n_tets <= 0) {
        out.summary = "No volume mesh available for stress recovery.";
        return out;
    }
    if ((int)displacement.size() != n_nodes * 3) {
        out.summary = "Displacement vector does not match mesh node count.";
        return out;
    }
    if (young_modulus <= 0.0 || poisson_ratio < 0.0 || poisson_ratio >= 0.5) {
        out.summary = "Invalid isotropic elastic material constants.";
        return out;
    }

    out.element_von_mises.assign((std::size_t)n_tets, 0.0f);
    out.nodal_von_mises.assign((std::size_t)n_nodes, 0.0f);
    out.element_equivalent_strain.assign((std::size_t)n_tets, 0.0f);
    out.nodal_equivalent_strain.assign((std::size_t)n_nodes, 0.0f);
    std::vector<int> node_hits((std::size_t)n_nodes, 0);

    const double E = young_modulus;
    const double nu = poisson_ratio;
    const double lam = E * nu / ((1.0 + nu) * (1.0 - 2.0 * nu));
    const double mu = E / (2.0 * (1.0 + nu));

    double min_vm = std::numeric_limits<double>::max();
    double max_vm = 0.0;

    for (int ti = 0; ti < n_tets; ++ti) {
        const auto& tet = mesh.tets[(std::size_t)ti];
        const int ids[4] = {tet.a, tet.b, tet.c, tet.d};
        bool valid_ids = true;
        for (int id : ids) {
            if (id < 0 || id >= n_nodes) valid_ids = false;
        }
        if (!valid_ids) {
            ++out.degenerate_elements;
            continue;
        }

        double dNdx[4][3];
        if (!tet4_shape_gradients(
                mesh.nodes[(std::size_t)ids[0]],
                mesh.nodes[(std::size_t)ids[1]],
                mesh.nodes[(std::size_t)ids[2]],
                mesh.nodes[(std::size_t)ids[3]],
                dNdx)) {
            ++out.degenerate_elements;
            continue;
        }

        double strain[6] = {};
        for (int a = 0; a < 4; ++a) {
            const int base = ids[a] * 3;
            const double ux = displacement[(std::size_t)base + 0];
            const double uy = displacement[(std::size_t)base + 1];
            const double uz = displacement[(std::size_t)base + 2];

            strain[0] += dNdx[a][0] * ux;
            strain[1] += dNdx[a][1] * uy;
            strain[2] += dNdx[a][2] * uz;
            strain[3] += dNdx[a][1] * ux + dNdx[a][0] * uy;
            strain[4] += dNdx[a][2] * uy + dNdx[a][1] * uz;
            strain[5] += dNdx[a][2] * ux + dNdx[a][0] * uz;
        }

        const double trace = strain[0] + strain[1] + strain[2];
        const double sx  = lam * trace + 2.0 * mu * strain[0];
        const double sy  = lam * trace + 2.0 * mu * strain[1];
        const double sz  = lam * trace + 2.0 * mu * strain[2];
        const double txy = mu * strain[3];
        const double tyz = mu * strain[4];
        const double txz = mu * strain[5];
        const double vm = von_mises_from_stress(sx, sy, sz, txy, tyz, txz);
        const double eq_strain = equivalent_strain_from_components(
            strain[0], strain[1], strain[2], strain[3], strain[4], strain[5]);

        if (!std::isfinite(vm) || !std::isfinite(eq_strain)) {
            ++out.degenerate_elements;
            continue;
        }

        out.element_von_mises[(std::size_t)ti] = static_cast<float>(vm);
        out.element_equivalent_strain[(std::size_t)ti] = static_cast<float>(eq_strain);
        for (int id : ids) {
            out.nodal_von_mises[(std::size_t)id] += static_cast<float>(vm);
            out.nodal_equivalent_strain[(std::size_t)id] += static_cast<float>(eq_strain);
            node_hits[(std::size_t)id] += 1;
        }
        min_vm = std::min(min_vm, vm);
        max_vm = std::max(max_vm, vm);
        ++out.elements_evaluated;
    }

    for (int n = 0; n < n_nodes; ++n) {
        const int hits = node_hits[(std::size_t)n];
        if (hits > 0) {
            out.nodal_von_mises[(std::size_t)n] /= static_cast<float>(hits);
            out.nodal_equivalent_strain[(std::size_t)n] /= static_cast<float>(hits);
        }
    }

    out.ok = out.elements_evaluated > 0;
    out.min_von_mises = out.ok ? static_cast<float>(min_vm) : 0.f;
    out.max_von_mises = out.ok ? static_cast<float>(max_vm) : 0.f;

    char buf[256];
    std::snprintf(buf, sizeof buf,
        "C3D4 stress recovery: %d/%d tetrahedra evaluated | degenerate %d | von Mises %.4g to %.4g MPa",
        out.elements_evaluated,
        n_tets,
        out.degenerate_elements,
        out.min_von_mises,
        out.max_von_mises);
    out.summary = buf;
    return out;
}

} // namespace tpms::fem
