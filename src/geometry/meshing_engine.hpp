#pragma once

#include <string>
#include <vector>

#include "../state/project_state.hpp"
#include "tpms_field.hpp"

namespace tpms::geometry {

struct MeshVec3 {
    float x, y, z;
};

struct SurfaceTriangle {
    int a = 0, b = 0, c = 0;
};

struct SurfaceMeshData {
    std::vector<MeshVec3> vertices;
    std::vector<MeshVec3> normals;
    std::vector<SurfaceTriangle> triangles;
    std::vector<MeshVec3> line_vertices;
};

struct TetElement {
    int a = 0, b = 0, c = 0, d = 0;
};

struct VolumeMeshData {
    std::vector<MeshVec3> nodes;
    std::vector<TetElement> tets;
    std::vector<MeshVec3> line_vertices;
};

struct MeshValidationReport {
    bool ok = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

struct SurfaceMeshResult {
    MeshValidationReport validation;
    SurfaceMeshData mesh;
    int node_count = 0;
    int triangle_count = 0;
    float min_quality = 0.f;
    float avg_quality = 0.f;
    float max_aspect_ratio = 0.f;
    std::string engine_name;
    std::string summary;
};

struct VolumeMeshResult {
    MeshValidationReport validation;
    VolumeMeshData mesh;
    int node_count = 0;
    int tet_count = 0;
    float min_quality = 0.f;
    float avg_quality = 0.f;
    float max_aspect_ratio = 0.f;
    std::string engine_name;
    std::string summary;
};

MeshValidationReport validate_surface_meshing(const ProjectState& state, const FieldData* field);
MeshValidationReport validate_volume_meshing(const ProjectState& state);
SurfaceMeshResult generate_surface_mesh(const ProjectState& state, const FieldData* field);
VolumeMeshResult generate_volume_mesh(const ProjectState& state, const FieldData* field);

} // namespace tpms::geometry
