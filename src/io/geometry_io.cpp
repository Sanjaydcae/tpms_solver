#include "geometry_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace tpms::io {

// ── Binary STL ────────────────────────────────────────────────────────────────

ExportResult export_stl_binary(
    const geometry::SurfaceMeshData& mesh,
    const std::string& path
) {
    if (mesh.triangles.empty())
        return {false, "Surface mesh has no triangles to export."};

    std::ofstream f(path, std::ios::binary);
    if (!f) return {false, "Cannot open file for writing: " + path};

    // 80-byte header
    char header[80] = {};
    std::snprintf(header, sizeof header, "TPMS Studio — binary STL export");
    f.write(header, 80);

    // Triangle count
    const auto n = static_cast<std::uint32_t>(mesh.triangles.size());
    f.write(reinterpret_cast<const char*>(&n), 4);

    const std::uint16_t attr = 0;
    for (const auto& tri : mesh.triangles) {
        const auto& va = mesh.vertices[tri.a];
        const auto& vb = mesh.vertices[tri.b];
        const auto& vc = mesh.vertices[tri.c];
        const auto& na = mesh.normals[tri.a];

        // Normal (face normal — use vertex a's precomputed flat normal)
        f.write(reinterpret_cast<const char*>(&na.x), 4);
        f.write(reinterpret_cast<const char*>(&na.y), 4);
        f.write(reinterpret_cast<const char*>(&na.z), 4);
        // Vertices
        for (const auto* v : {&va, &vb, &vc}) {
            f.write(reinterpret_cast<const char*>(&v->x), 4);
            f.write(reinterpret_cast<const char*>(&v->y), 4);
            f.write(reinterpret_cast<const char*>(&v->z), 4);
        }
        f.write(reinterpret_cast<const char*>(&attr), 2);
    }

    if (!f) return {false, "Write error while exporting STL: " + path};

    char msg[128];
    std::snprintf(msg, sizeof msg,
        "Exported %u triangles to binary STL: %s", n, path.c_str());
    return {true, msg};
}

// ── ASCII STL ─────────────────────────────────────────────────────────────────

ExportResult export_stl_ascii(
    const geometry::SurfaceMeshData& mesh,
    const std::string& path
) {
    if (mesh.triangles.empty())
        return {false, "Surface mesh has no triangles to export."};

    std::ofstream f(path);
    if (!f) return {false, "Cannot open file for writing: " + path};

    f << "solid TPMS\n";
    for (const auto& tri : mesh.triangles) {
        const auto& va = mesh.vertices[tri.a];
        const auto& vb = mesh.vertices[tri.b];
        const auto& vc = mesh.vertices[tri.c];
        const auto& n  = mesh.normals[tri.a];
        f << "  facet normal " << n.x  << ' ' << n.y  << ' ' << n.z  << '\n'
          << "    outer loop\n"
          << "      vertex "   << va.x << ' ' << va.y << ' ' << va.z << '\n'
          << "      vertex "   << vb.x << ' ' << vb.y << ' ' << vb.z << '\n'
          << "      vertex "   << vc.x << ' ' << vc.y << ' ' << vc.z << '\n'
          << "    endloop\n"
          << "  endfacet\n";
    }
    f << "endsolid TPMS\n";

    if (!f) return {false, "Write error while exporting ASCII STL: " + path};

    char msg[128];
    std::snprintf(msg, sizeof msg,
        "Exported %zu triangles to ASCII STL: %s",
        mesh.triangles.size(), path.c_str());
    return {true, msg};
}

// ── OBJ ───────────────────────────────────────────────────────────────────────

ExportResult export_obj(
    const geometry::SurfaceMeshData& mesh,
    const std::string& path,
    const std::string& object_name
) {
    if (mesh.triangles.empty())
        return {false, "Surface mesh has no triangles to export."};

    std::ofstream f(path);
    if (!f) return {false, "Cannot open file for writing: " + path};

    f << "# TPMS Studio — OBJ export\n";
    f << "o " << object_name << "\n\n";

    for (const auto& v : mesh.vertices)
        f << "v " << v.x << ' ' << v.y << ' ' << v.z << '\n';
    f << '\n';
    for (const auto& n : mesh.normals)
        f << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
    f << "\ng " << object_name << "\n";
    for (const auto& tri : mesh.triangles) {
        // OBJ indices are 1-based; format: f v//vn v//vn v//vn
        const int a = tri.a + 1;
        const int b = tri.b + 1;
        const int c = tri.c + 1;
        f << "f " << a << "//" << a << ' '
                  << b << "//" << b << ' '
                  << c << "//" << c << '\n';
    }

    if (!f) return {false, "Write error while exporting OBJ: " + path};

    char msg[128];
    std::snprintf(msg, sizeof msg,
        "Exported %zu vertices, %zu triangles to OBJ: %s",
        mesh.vertices.size(), mesh.triangles.size(), path.c_str());
    return {true, msg};
}

// ── PLY (binary little-endian) ────────────────────────────────────────────────

ExportResult export_ply(
    const geometry::SurfaceMeshData& mesh,
    const std::string& path
) {
    if (mesh.triangles.empty())
        return {false, "Surface mesh has no triangles to export."};

    std::ofstream f(path, std::ios::binary);
    if (!f) return {false, "Cannot open file for writing: " + path};

    const int nv = (int)mesh.vertices.size();
    const int nf = (int)mesh.triangles.size();

    // ASCII header
    std::ostringstream hdr;
    hdr << "ply\n"
        << "format binary_little_endian 1.0\n"
        << "comment TPMS Studio export\n"
        << "element vertex " << nv << "\n"
        << "property float x\nproperty float y\nproperty float z\n"
        << "property float nx\nproperty float ny\nproperty float nz\n"
        << "element face " << nf << "\n"
        << "property list uchar int vertex_indices\n"
        << "end_header\n";
    const std::string hdr_str = hdr.str();
    f.write(hdr_str.data(), (std::streamsize)hdr_str.size());

    // Vertices + normals interleaved
    for (int i = 0; i < nv; ++i) {
        const auto& v = mesh.vertices[i];
        const auto& n = mesh.normals[i];
        f.write(reinterpret_cast<const char*>(&v.x), 4);
        f.write(reinterpret_cast<const char*>(&v.y), 4);
        f.write(reinterpret_cast<const char*>(&v.z), 4);
        f.write(reinterpret_cast<const char*>(&n.x), 4);
        f.write(reinterpret_cast<const char*>(&n.y), 4);
        f.write(reinterpret_cast<const char*>(&n.z), 4);
    }
    // Faces: uchar count + 3×int indices
    const std::uint8_t cnt = 3;
    for (const auto& tri : mesh.triangles) {
        f.write(reinterpret_cast<const char*>(&cnt), 1);
        const std::int32_t ids[3] = {tri.a, tri.b, tri.c};
        f.write(reinterpret_cast<const char*>(ids), 12);
    }

    if (!f) return {false, "Write error while exporting PLY: " + path};

    char msg[128];
    std::snprintf(msg, sizeof msg,
        "Exported %d vertices, %d faces to PLY: %s", nv, nf, path.c_str());
    return {true, msg};
}

// ── VTK legacy unstructured grid (.vtk) ──────────────────────────────────────

ExportResult export_vtk_ugrid(
    const geometry::VolumeMeshData& mesh,
    const std::string& path
) {
    if (mesh.nodes.empty() || mesh.tets.empty())
        return {false, "Volume mesh is empty."};

    std::ofstream f(path);
    if (!f) return {false, "Cannot open file for writing: " + path};

    const int nn = (int)mesh.nodes.size();
    const int nt = (int)mesh.tets.size();

    f << "# vtk DataFile Version 3.0\n"
      << "TPMS Studio volume mesh\n"
      << "ASCII\n"
      << "DATASET UNSTRUCTURED_GRID\n\n";

    f << "POINTS " << nn << " float\n";
    for (const auto& n : mesh.nodes)
        f << n.x << ' ' << n.y << ' ' << n.z << '\n';
    f << '\n';

    // Cells: each tet = 5 values (4 + count)
    f << "CELLS " << nt << ' ' << (nt * 5) << '\n';
    for (const auto& t : mesh.tets)
        f << "4 " << t.a << ' ' << t.b << ' ' << t.c << ' ' << t.d << '\n';
    f << '\n';

    // Cell types: 10 = VTK_TETRA
    f << "CELL_TYPES " << nt << '\n';
    for (int i = 0; i < nt; ++i) f << "10\n";

    if (!f) return {false, "Write error while exporting VTK: " + path};

    char msg[128];
    std::snprintf(msg, sizeof msg,
        "Exported %d nodes, %d tetrahedra to VTK: %s", nn, nt, path.c_str());
    return {true, msg};
}

ExportResult export_vtk_ugrid_with_point_scalar(
    const geometry::VolumeMeshData& mesh,
    const std::vector<float>& scalars,
    const std::string& scalar_name,
    const std::string& path
) {
    if (mesh.nodes.empty() || mesh.tets.empty())
        return {false, "Volume mesh is empty."};
    if (scalars.size() != mesh.nodes.size())
        return {false, "Result scalar count does not match mesh node count."};

    std::ofstream f(path);
    if (!f) return {false, "Cannot open file for writing: " + path};

    const int nn = (int)mesh.nodes.size();
    const int nt = (int)mesh.tets.size();
    std::string safe_name = scalar_name.empty() ? "Result" : scalar_name;
    for (char& c : safe_name) {
        if (c == ' ' || c == '-' || c == '/' || c == '\\') c = '_';
    }

    f << "# vtk DataFile Version 3.0\n"
      << "TPMS Studio result export\n"
      << "ASCII\n"
      << "DATASET UNSTRUCTURED_GRID\n\n";

    f << "POINTS " << nn << " float\n";
    for (const auto& n : mesh.nodes)
        f << n.x << ' ' << n.y << ' ' << n.z << '\n';
    f << '\n';

    f << "CELLS " << nt << ' ' << (nt * 5) << '\n';
    for (const auto& t : mesh.tets)
        f << "4 " << t.a << ' ' << t.b << ' ' << t.c << ' ' << t.d << '\n';
    f << '\n';

    f << "CELL_TYPES " << nt << '\n';
    for (int i = 0; i < nt; ++i) f << "10\n";

    f << "\nPOINT_DATA " << nn << '\n';
    f << "SCALARS " << safe_name << " float 1\n";
    f << "LOOKUP_TABLE default\n";
    for (float v : scalars) f << v << '\n';

    if (!f) return {false, "Write error while exporting result VTK: " + path};

    char msg[160];
    std::snprintf(msg, sizeof msg,
        "Exported active result '%s' on %d nodes to VTK: %s",
        scalar_name.c_str(), nn, path.c_str());
    return {true, msg};
}

// ── Field slice CSV ───────────────────────────────────────────────────────────

ExportResult export_field_slice_csv(
    const geometry::FieldData& field,
    SliceAxis                  axis,
    int                        slice_index,
    const std::string&         path
) {
    if (field.empty()) return {false, "Field data is empty."};

    std::ofstream f(path);
    if (!f) return {false, "Cannot open file for writing: " + path};

    int W = 0, H = 0;
    std::vector<float> slice;

    switch (axis) {
        case SliceAxis::XY:
            slice_index = std::clamp(slice_index, 0, field.nz - 1);
            W = field.nx; H = field.ny;
            slice.resize(W * H);
            for (int iy = 0; iy < H; ++iy)
                for (int ix = 0; ix < W; ++ix)
                    slice[ix + W * iy] = field.at(ix, iy, slice_index);
            f << "# XY slice at iz=" << slice_index << ", cols=X, rows=Y\n";
            break;
        case SliceAxis::XZ:
            slice_index = std::clamp(slice_index, 0, field.ny - 1);
            W = field.nx; H = field.nz;
            slice.resize(W * H);
            for (int iz = 0; iz < H; ++iz)
                for (int ix = 0; ix < W; ++ix)
                    slice[ix + W * iz] = field.at(ix, slice_index, iz);
            f << "# XZ slice at iy=" << slice_index << ", cols=X, rows=Z\n";
            break;
        case SliceAxis::YZ:
            slice_index = std::clamp(slice_index, 0, field.nx - 1);
            W = field.ny; H = field.nz;
            slice.resize(W * H);
            for (int iz = 0; iz < H; ++iz)
                for (int iy = 0; iy < W; ++iy)
                    slice[iy + W * iz] = field.at(slice_index, iy, iz);
            f << "# YZ slice at ix=" << slice_index << ", cols=Y, rows=Z\n";
            break;
    }

    for (int row = 0; row < H; ++row) {
        for (int col = 0; col < W; ++col) {
            f << slice[col + W * row];
            if (col < W - 1) f << ',';
        }
        f << '\n';
    }

    if (!f) return {false, "Write error while exporting field CSV: " + path};

    char msg[128];
    std::snprintf(msg, sizeof msg,
        "Exported %dx%d field slice to CSV: %s", W, H, path.c_str());
    return {true, msg};
}

// ── Field raw binary ──────────────────────────────────────────────────────────

ExportResult export_field_raw(
    const geometry::FieldData& field,
    const std::string&         path
) {
    if (field.empty()) return {false, "Field data is empty."};

    std::ofstream f(path, std::ios::binary);
    if (!f) return {false, "Cannot open file for writing: " + path};

    // Header: nx, ny, nz as int32
    const std::int32_t dims[3] = {field.nx, field.ny, field.nz};
    f.write(reinterpret_cast<const char*>(dims), 12);
    f.write(reinterpret_cast<const char*>(field.values.data()),
            (std::streamsize)(field.values.size() * sizeof(float)));

    if (!f) return {false, "Write error while exporting field raw: " + path};

    char msg[128];
    std::snprintf(msg, sizeof msg,
        "Exported %dx%dx%d field (%zu floats) to raw binary: %s",
        field.nx, field.ny, field.nz, field.values.size(), path.c_str());
    return {true, msg};
}

} // namespace tpms::io
