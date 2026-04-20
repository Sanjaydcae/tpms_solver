#pragma once

#include <string>
#include "../geometry/meshing_engine.hpp"
#include "../geometry/tpms_field.hpp"
#include "../state/project_state.hpp"

namespace tpms::io {

struct ExportResult {
    bool        ok      = false;
    std::string message;   // human-readable summary or error
};

// ── Surface mesh exports ──────────────────────────────────────────────────────

// Binary STL — compact, most widely supported
ExportResult export_stl_binary(
    const geometry::SurfaceMeshData& mesh,
    const std::string& path
);

// ASCII STL — human-readable, larger file
ExportResult export_stl_ascii(
    const geometry::SurfaceMeshData& mesh,
    const std::string& path
);

// Wavefront OBJ — with normals, compatible with Blender/MeshLab/most CAD
ExportResult export_obj(
    const geometry::SurfaceMeshData& mesh,
    const std::string& path,
    const std::string& object_name = "TPMS"
);

// Stanford PLY — binary little-endian with normals
ExportResult export_ply(
    const geometry::SurfaceMeshData& mesh,
    const std::string& path
);

// ── Volume mesh exports ───────────────────────────────────────────────────────

// VTK legacy unstructured grid (.vtk) — compatible with ParaView / deal.II input
ExportResult export_vtk_ugrid(
    const geometry::VolumeMeshData& mesh,
    const std::string& path
);

ExportResult export_vtk_ugrid_with_point_scalar(
    const geometry::VolumeMeshData& mesh,
    const std::vector<float>& scalars,
    const std::string& scalar_name,
    const std::string& path
);

// ── Field / scalar data exports ───────────────────────────────────────────────

// Export a 2D slice of the scalar field as CSV (value per grid point)
ExportResult export_field_slice_csv(
    const geometry::FieldData& field,
    SliceAxis                  axis,
    int                        slice_index,
    const std::string&         path
);

// Export the full 3D field as a raw binary float array
// Header: nx(int32), ny(int32), nz(int32), then nx*ny*nz float32 values
ExportResult export_field_raw(
    const geometry::FieldData& field,
    const std::string&         path
);

} // namespace tpms::io
