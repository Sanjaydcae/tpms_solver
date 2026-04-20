#pragma once

#include <string>
#include <vector>

#include "../state/project_state.hpp"
#include "tpms_field.hpp"

namespace tpms::geometry {

struct ValidationReport {
    bool ok = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

struct GeometryBuildResult {
    FieldData field;
    ValidationReport validation;
    std::string summary;
    float field_min = 0.f;
    float field_max = 0.f;
    int cell_count_x = 0;
    int cell_count_y = 0;
    int cell_count_z = 0;
    float solid_fraction = 0.f;
};

ValidationReport validate_geometry_inputs(const ProjectState& state);
GeometryBuildResult build_geometry(const ProjectState& state, int border_voxels = 0);
const char* design_equation_name(TPMSDesign design);

} // namespace tpms::geometry
