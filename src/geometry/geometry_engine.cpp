#include "geometry_engine.hpp"

#include <algorithm>
#include <cstdio>

namespace tpms::geometry {

namespace {

std::string build_summary(const ProjectState& state, const FieldData& field) {
    char buf[256];
    std::snprintf(
        buf,
        sizeof(buf),
        "%s %s in %s domain %.0f x %.0f x %.0f mm, cell %.1f x %.1f x %.1f mm, thickness %.2f mm, grid %d x %d x %d",
        state.design_name(),
        state.geometry_mode_name(),
        state.domain_name(),
        state.size_x, state.size_y, state.size_z,
        state.cell_x, state.cell_y, state.cell_z,
        state.wall_thickness,
        field.nx, field.ny, field.nz
    );
    return buf;
}

} // namespace

const char* design_equation_name(TPMSDesign design) {
    switch (design) {
        case TPMSDesign::Gyroid:   return "Gyroid";
        case TPMSDesign::Diamond:  return "Diamond";
        case TPMSDesign::SchwarzP: return "Schwarz-P";
        case TPMSDesign::Lidinoid: return "Lidinoid";
        case TPMSDesign::Neovius:  return "Neovius";
    }
    return "Unknown";
}

ValidationReport validate_geometry_inputs(const ProjectState& state) {
    ValidationReport report;

    auto require_positive = [&](float value, const char* label) {
        if (value <= 0.f) {
            report.ok = false;
            report.errors.push_back(std::string(label) + " must be greater than zero.");
        }
    };

    require_positive(state.size_x, "Length X");
    require_positive(state.size_y, "Width Y");
    require_positive(state.size_z, "Height Z");
    require_positive(state.cell_x, "Unit cell X");
    require_positive(state.cell_y, "Unit cell Y");
    require_positive(state.cell_z, "Unit cell Z");
    require_positive(state.wall_thickness, "Wall thickness");

    if (state.resolution < 20) {
        report.ok = false;
        report.errors.push_back("Resolution must be at least 20 for a usable TPMS preview.");
    }
    if (state.resolution > 256) {
        report.warnings.push_back("Resolution above 256 may make preview generation slow.");
    }

    const float min_dim = std::min({state.size_x, state.size_y, state.size_z});
    const float max_cell = std::max({state.cell_x, state.cell_y, state.cell_z});
    if (max_cell > min_dim) {
        report.warnings.push_back("One or more unit-cell dimensions are larger than the domain size.");
    }

    const float min_cell = std::min({state.cell_x, state.cell_y, state.cell_z});
    if (state.wall_thickness >= min_cell * 0.9f) {
        report.warnings.push_back("Wall thickness is close to the unit-cell size and may collapse open channels.");
    }
    if (state.geometry_mode == GeometryMode::Solid && state.wall_thickness > min_cell * 0.5f) {
        report.warnings.push_back("Wall thickness is ignored in solid mode.");
    }
    if (state.domain == DomainType::Cylinder && std::abs(state.size_x - state.size_y) > 1e-3f) {
        report.warnings.push_back("Cylinder preview uses the smaller of X and Y as the diameter control.");
    }
    if (state.domain == DomainType::Sphere &&
        (std::abs(state.size_x - state.size_y) > 1e-3f || std::abs(state.size_y - state.size_z) > 1e-3f)) {
        report.warnings.push_back("Sphere preview uses the smallest of X, Y, and Z as the diameter control.");
    }

    return report;
}

GeometryBuildResult build_geometry(const ProjectState& state, int border_voxels) {
    GeometryBuildResult result;
    result.validation = validate_geometry_inputs(state);
    if (!result.validation.ok) {
        return result;
    }

    result.field = sample_field(state, border_voxels);
    if (!result.field.empty()) {
        result.field_min = result.field.field_min();
        result.field_max = result.field.field_max();
        result.cell_count_x = std::max(1, (int)std::lround(state.size_x / state.cell_x));
        result.cell_count_y = std::max(1, (int)std::lround(state.size_y / state.cell_y));
        result.cell_count_z = std::max(1, (int)std::lround(state.size_z / state.cell_z));
        int solid_count = 0;
        for (float v : result.field.values) {
            if (v <= 0.f) ++solid_count;
        }
        result.solid_fraction = result.field.values.empty() ? 0.f
            : (float)solid_count / (float)result.field.values.size();
    }
    result.summary = build_summary(state, result.field);
    if (result.field.values.empty()) {
        result.validation.ok = false;
        result.validation.errors.push_back("Geometry field generation returned an empty dataset.");
    }
    return result;
}

} // namespace tpms::geometry
