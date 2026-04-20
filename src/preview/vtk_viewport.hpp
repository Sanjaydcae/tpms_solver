#pragma once

#include "../geometry/meshing_engine.hpp"
#include "../geometry/tpms_field.hpp"
#include "../state/project_state.hpp"

using GLuint = unsigned int;

namespace tpms::preview {

class VtkViewport {
public:
    VtkViewport();
    ~VtkViewport();

    void init();
    void cleanup();

    // Geometry
    void set_surface_mesh(const geometry::SurfaceMeshData* mesh);
    void set_volume_mesh(const geometry::VolumeMeshData*   mesh);

    // Results — pass empty scalars to clear
    void set_result(const geometry::VolumeMeshData* mesh,
                    const std::vector<float>&       per_node_scalars,
                    const std::string&              title,
                    const std::string&              unit,
                    float                           deform_scale = 1.f);
    void clear_result();

    // Overlay text shown top-left of viewport (PrePoMax-style info box)
    void set_result_info(const std::string& text);

    // Render → returns OpenGL texture ID owned by VTK's offscreen FBO
    GLuint render(int w, int h);

    void mouse_button(int btn, bool pressed, float x, float y);
    void mouse_move(float x, float y);
    void mouse_wheel(float delta);
    void reset_camera();

private:
    struct Impl;
    Impl* d_       = nullptr;
    int   last_w_  = 0, last_h_ = 0;
    bool  initialised_ = false;

    void ensure_fbo(int w, int h);
    GLuint fbo_ = 0, tex_ = 0, rbo_ = 0;
};

} // namespace tpms::preview
