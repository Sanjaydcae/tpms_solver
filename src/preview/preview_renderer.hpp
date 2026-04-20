#pragma once

#include <vector>

#include "../state/project_state.hpp"

using GLuint = unsigned int;

namespace tpms::ui::preview {

class PreviewRenderer {
public:
    void init_gl();
    void cleanup();
    void render(
        const ProjectState& state,
        int width,
        int height,
        float azimuth,
        float elevation,
        float distance,
        float pan_x,
        float pan_y
    );

    GLuint texture_id() const { return tex_; }

private:
    struct PreviewVec3 {
        float x, y, z;
    };
    struct PreviewTriangle {
        PreviewVec3 a, b, c;
        PreviewVec3 normal;
    };

    GLuint fbo_ = 0;
    GLuint tex_ = 0;
    GLuint rbo_ = 0;
    GLuint mesh_vao_ = 0;
    GLuint mesh_vbo_ = 0;
    GLuint shader_program_ = 0;
    GLuint line_vao_ = 0;
    GLuint line_vbo_ = 0;
    GLuint line_shader_program_ = 0;
    int vp_w_ = 0;
    int vp_h_ = 0;
    int mesh_vertex_count_ = 0;
    int line_vertex_count_ = 0;
    const void* cached_field_ = nullptr;
    const void* cached_surface_mesh_ = nullptr;
    const void* cached_volume_mesh_ = nullptr;
    std::vector<PreviewTriangle> preview_tris_;

    void resize_fbo(int width, int height);
    void ensure_gl_resources();
    void rebuild_preview_mesh(const ProjectState& state);
    void upload_preview_mesh();
    void upload_surface_mesh(const void* surface_mesh_ptr);
    void upload_line_mesh(const void* surface_mesh_ptr, const void* volume_mesh_ptr);
};

} // namespace tpms::ui::preview
