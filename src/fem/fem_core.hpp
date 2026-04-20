#pragma once

#include <string>
#include <vector>

#include "../geometry/meshing_engine.hpp"

namespace tpms::fem {

struct StressRecoveryResult {
    bool ok = false;
    int elements_evaluated = 0;
    int degenerate_elements = 0;
    float min_von_mises = 0.f;
    float max_von_mises = 0.f;
    std::string summary;
    std::vector<float> element_von_mises;
    std::vector<float> nodal_von_mises;
    std::vector<float> element_equivalent_strain;
    std::vector<float> nodal_equivalent_strain;
};

StressRecoveryResult recover_tet4_von_mises(
    const geometry::VolumeMeshData& mesh,
    const std::vector<double>& displacement,
    double young_modulus,
    double poisson_ratio
);

} // namespace tpms::fem
