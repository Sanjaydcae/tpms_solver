#include "preview_renderer.hpp"

#include "../geometry/meshing_engine.hpp"
#include "../geometry/tpms_field.hpp"

#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#  include <OpenGL/gl3.h>
#else
#  include <GL/gl.h>
#  include <GL/glext.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

namespace tpms::ui::preview {

namespace {

struct Vec3 {
    float x, y, z;
};

struct Mat4 {
    std::array<float, 16> m{};
};

static Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 operator*(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static Vec3 operator/(const Vec3& a, float s) { return {a.x / s, a.y / s, a.z / s}; }
static float dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
static float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
static Vec3 normalize(const Vec3& a) { return a / std::max(length(a), 1e-6f); }

static Vec3 interpolate_iso(const Vec3& p0, const Vec3& p1, float v0, float v1) {
    const float denom = (v1 - v0);
    if (std::abs(denom) < 1e-8f) return (p0 + p1) * 0.5f;
    const float t = std::clamp((-v0) / denom, 0.0f, 1.0f);
    return p0 + (p1 - p0) * t;
}

static Mat4 identity_mat4() {
    Mat4 out{};
    out.m = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    return out;
}

static Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            out.m[c * 4 + r] =
                a.m[0 * 4 + r] * b.m[c * 4 + 0] +
                a.m[1 * 4 + r] * b.m[c * 4 + 1] +
                a.m[2 * 4 + r] * b.m[c * 4 + 2] +
                a.m[3 * 4 + r] * b.m[c * 4 + 3];
        }
    }
    return out;
}

static Mat4 perspective(float fovy_rad, float aspect, float z_near, float z_far) {
    const float f = 1.0f / std::tan(fovy_rad * 0.5f);
    Mat4 out{};
    out.m = {
        f / aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (z_far + z_near) / (z_near - z_far), -1,
        0, 0, (2.0f * z_far * z_near) / (z_near - z_far), 0
    };
    return out;
}

static Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 f = normalize(target - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 out = identity_mat4();
    out.m = {
         s.x,  u.x, -f.x, 0,
         s.y,  u.y, -f.y, 0,
         s.z,  u.z, -f.z, 0,
        -dot(s, eye), -dot(u, eye), dot(f, eye), 1
    };
    return out;
}

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error: %s\n", log);
    }
    return shader;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error: %s\n", log);
    }
    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static Vec3 jet_color(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float r = std::clamp(1.5f - std::abs(4.0f * t - 3.0f), 0.0f, 1.0f);
    const float g = std::clamp(1.5f - std::abs(4.0f * t - 2.0f), 0.0f, 1.0f);
    const float b = std::clamp(1.5f - std::abs(4.0f * t - 1.0f), 0.0f, 1.0f);
    return {r, g, b};
}

std::vector<geometry::MeshVec3> decimate_line_vertices(
    const std::vector<geometry::MeshVec3>& input,
    std::size_t max_segments
) {
    if (input.size() <= max_segments * 2) return input;
    std::vector<geometry::MeshVec3> output;
    output.reserve(max_segments * 2);
    const std::size_t segment_count = input.size() / 2;
    const std::size_t stride = std::max<std::size_t>(1, segment_count / max_segments);
    for (std::size_t seg = 0; seg < segment_count; seg += stride) {
        const std::size_t i = seg * 2;
        if (i + 1 >= input.size()) break;
        output.push_back(input[i]);
        output.push_back(input[i + 1]);
        if (output.size() >= max_segments * 2) break;
    }
    return output;
}

} // namespace

void PreviewRenderer::init_gl() {
    ensure_gl_resources();
}

void PreviewRenderer::cleanup() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (tex_) glDeleteTextures(1, &tex_);
    if (rbo_) glDeleteRenderbuffers(1, &rbo_);
    if (mesh_vbo_) glDeleteBuffers(1, &mesh_vbo_);
    if (mesh_vao_) glDeleteVertexArrays(1, &mesh_vao_);
    if (line_vbo_) glDeleteBuffers(1, &line_vbo_);
    if (line_vao_) glDeleteVertexArrays(1, &line_vao_);
    if (result_vbo_) glDeleteBuffers(1, &result_vbo_);
    if (result_vao_) glDeleteVertexArrays(1, &result_vao_);
    if (shader_program_) glDeleteProgram(shader_program_);
    if (line_shader_program_) glDeleteProgram(line_shader_program_);
    if (result_shader_program_) glDeleteProgram(result_shader_program_);
}

void PreviewRenderer::resize_fbo(int width, int height) {
    if (width == vp_w_ && height == vp_h_) return;
    vp_w_ = width;
    vp_h_ = height;

    if (fbo_) { glDeleteFramebuffers(1, &fbo_); fbo_ = 0; }
    if (tex_) { glDeleteTextures(1, &tex_); tex_ = 0; }
    if (rbo_) { glDeleteRenderbuffers(1, &rbo_); rbo_ = 0; }

    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);

    glGenRenderbuffers(1, &rbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PreviewRenderer::ensure_gl_resources() {
    if (shader_program_ == 0) {
        const char* vs_src = R"(
            #version 330 core
            layout(location = 0) in vec3 a_pos;
            layout(location = 1) in vec3 a_normal;
            uniform mat4 u_mvp;
            uniform mat4 u_model;
            out vec3 v_normal;
            out vec3 v_world_pos;
            void main() {
                vec4 world = u_model * vec4(a_pos, 1.0);
                v_world_pos = world.xyz;
                v_normal = mat3(u_model) * a_normal;
                gl_Position = u_mvp * vec4(a_pos, 1.0);
            }
        )";
        const char* fs_src = R"(
            #version 330 core
            in vec3 v_normal;
            in vec3 v_world_pos;
            uniform vec3 u_light_dir;
            uniform vec3 u_base_color;
            uniform vec3 u_eye;
            out vec4 frag_color;
            void main() {
                vec3 n = normalize(v_normal);
                if (!gl_FrontFacing) n = -n;
                vec3 l = normalize(-u_light_dir);
                vec3 v = normalize(u_eye - v_world_pos);
                vec3 h = normalize(l + v);
                float diff = max(dot(n, l), 0.0);
                float spec = pow(max(dot(n, h), 0.0), 32.0);
                float fresnel = pow(1.0 - max(dot(n, v), 0.0), 3.0);
                vec3 color = u_base_color * (0.34 + 0.72 * diff)
                           + vec3(0.22) * spec
                           + vec3(0.08, 0.11, 0.16) * fresnel;
                frag_color = vec4(color, 1.0);
            }
        )";
        shader_program_ = link_program(
            compile_shader(GL_VERTEX_SHADER, vs_src),
            compile_shader(GL_FRAGMENT_SHADER, fs_src)
        );
    }

    if (line_shader_program_ == 0) {
        const char* line_vs = R"(
            #version 330 core
            layout(location = 0) in vec3 a_pos;
            uniform mat4 u_mvp;
            void main() {
                gl_Position = u_mvp * vec4(a_pos, 1.0);
            }
        )";
        const char* line_fs = R"(
            #version 330 core
            uniform vec3 u_color;
            out vec4 frag_color;
            void main() {
                frag_color = vec4(u_color, 1.0);
            }
        )";
        line_shader_program_ = link_program(
            compile_shader(GL_VERTEX_SHADER, line_vs),
            compile_shader(GL_FRAGMENT_SHADER, line_fs)
        );
    }

    if (result_shader_program_ == 0) {
        const char* result_vs = R"(
            #version 330 core
            layout(location = 0) in vec3 a_pos;
            layout(location = 1) in vec3 a_normal;
            layout(location = 2) in vec3 a_color;
            uniform mat4 u_mvp;
            uniform mat4 u_model;
            out vec3 v_normal;
            out vec3 v_color;
            void main() {
                v_normal = mat3(u_model) * a_normal;
                v_color = a_color;
                gl_Position = u_mvp * vec4(a_pos, 1.0);
            }
        )";
        const char* result_fs = R"(
            #version 330 core
            in vec3 v_normal;
            in vec3 v_color;
            out vec4 frag_color;
            void main() {
                vec3 n = normalize(v_normal);
                if (!gl_FrontFacing) n = -n;
                vec3 l = normalize(vec3(0.45, -0.65, 0.72));
                float diff = max(dot(n, l), 0.0);
                vec3 color = v_color * (0.42 + 0.72 * diff);
                frag_color = vec4(color, 1.0);
            }
        )";
        result_shader_program_ = link_program(
            compile_shader(GL_VERTEX_SHADER, result_vs),
            compile_shader(GL_FRAGMENT_SHADER, result_fs)
        );
    }

    if (mesh_vao_ == 0) glGenVertexArrays(1, &mesh_vao_);
    if (mesh_vbo_ == 0) glGenBuffers(1, &mesh_vbo_);
    if (line_vao_ == 0) glGenVertexArrays(1, &line_vao_);
    if (line_vbo_ == 0) glGenBuffers(1, &line_vbo_);
    if (result_vao_ == 0) glGenVertexArrays(1, &result_vao_);
    if (result_vbo_ == 0) glGenBuffers(1, &result_vbo_);
}

void PreviewRenderer::rebuild_preview_mesh(const ProjectState& state) {
    auto* field = static_cast<geometry::FieldData*>(state.field_data);
    preview_tris_.clear();
    cached_field_ = state.field_data;
    if (!field || field->empty()) return;

    const int step = field->nx <= 96 ? 1 : std::max(1, field->nx / 72);
    const float dx = state.size_x / std::max(1, field->nx - 1);
    const float dy = state.size_y / std::max(1, field->ny - 1);
    const float dz = state.size_z / std::max(1, field->nz - 1);
    const float hx = state.size_x * 0.5f;
    const float hy = state.size_y * 0.5f;
    const float hz = state.size_z * 0.5f;

    const int tetra[6][4] = {
        {0, 5, 1, 6}, {0, 5, 6, 4}, {0, 1, 2, 6},
        {0, 2, 3, 6}, {0, 3, 7, 6}, {0, 7, 4, 6},
    };

    for (int iz = 0; iz < field->nz - step; iz += step) {
        for (int iy = 0; iy < field->ny - step; iy += step) {
            for (int ix = 0; ix < field->nx - step; ix += step) {
                const int xs[2] = {ix, ix + step};
                const int ys[2] = {iy, iy + step};
                const int zs[2] = {iz, iz + step};

                Vec3 p[8];
                float v[8];
                int idx = 0;
                for (int zz = 0; zz < 2; ++zz) {
                    for (int yy = 0; yy < 2; ++yy) {
                        for (int xx = 0; xx < 2; ++xx) {
                            const int gx = xs[xx];
                            const int gy = ys[yy];
                            const int gz = zs[zz];
                            p[idx] = {-hx + gx * dx, -hy + gy * dy, -hz + gz * dz};
                            v[idx] = field->at(gx, gy, gz);
                            ++idx;
                        }
                    }
                }

                for (const auto& t : tetra) {
                    Vec3 tp[4] = { p[t[0]], p[t[1]], p[t[2]], p[t[3]] };
                    float tv[4] = { v[t[0]], v[t[1]], v[t[2]], v[t[3]] };
                    int inside[4], outside[4];
                    int ni = 0, no = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (tv[i] <= 0.f) inside[ni++] = i;
                        else outside[no++] = i;
                    }
                    if (ni == 0 || ni == 4) continue;

                    auto push_tri = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
                        Vec3 n = normalize(cross(b - a, c - a));
                        preview_tris_.push_back({{a.x, a.y, a.z}, {b.x, b.y, b.z}, {c.x, c.y, c.z}, {n.x, n.y, n.z}});
                    };

                    if (ni == 1) {
                        const int i0 = inside[0];
                        push_tri(
                            interpolate_iso(tp[i0], tp[outside[0]], tv[i0], tv[outside[0]]),
                            interpolate_iso(tp[i0], tp[outside[1]], tv[i0], tv[outside[1]]),
                            interpolate_iso(tp[i0], tp[outside[2]], tv[i0], tv[outside[2]])
                        );
                    } else if (ni == 3) {
                        const int o0 = outside[0];
                        push_tri(
                            interpolate_iso(tp[o0], tp[inside[0]], tv[o0], tv[inside[0]]),
                            interpolate_iso(tp[o0], tp[inside[2]], tv[o0], tv[inside[2]]),
                            interpolate_iso(tp[o0], tp[inside[1]], tv[o0], tv[inside[1]])
                        );
                    } else if (ni == 2) {
                        const int i0 = inside[0], i1 = inside[1];
                        const int o0 = outside[0], o1 = outside[1];
                        const Vec3 a = interpolate_iso(tp[i0], tp[o0], tv[i0], tv[o0]);
                        const Vec3 b = interpolate_iso(tp[i1], tp[o0], tv[i1], tv[o0]);
                        const Vec3 c = interpolate_iso(tp[i1], tp[o1], tv[i1], tv[o1]);
                        const Vec3 d = interpolate_iso(tp[i0], tp[o1], tv[i0], tv[o1]);
                        push_tri(a, b, c);
                        push_tri(a, c, d);
                    }
                }
            }
        }
    }
    upload_preview_mesh();
}

void PreviewRenderer::upload_preview_mesh() {
    ensure_gl_resources();
    struct GpuVertex { float px, py, pz, nx, ny, nz; };
    std::vector<GpuVertex> vertices;
    vertices.reserve(preview_tris_.size() * 3);
    for (const auto& tri : preview_tris_) {
        auto push = [&](const PreviewVec3& p, const PreviewVec3& n) {
            vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z});
        };
        push(tri.a, tri.normal);
        push(tri.b, tri.normal);
        push(tri.c, tri.normal);
    }
    glBindVertexArray(mesh_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(GpuVertex)), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    mesh_vertex_count_ = (int)vertices.size();
}

void PreviewRenderer::upload_surface_mesh(const void* surface_mesh_ptr) {
    auto* surface_mesh = static_cast<const geometry::SurfaceMeshData*>(surface_mesh_ptr);
    if (!surface_mesh || surface_mesh->vertices.empty()) return;
    ensure_gl_resources();

    struct GpuVertex { float px, py, pz, nx, ny, nz; };
    std::vector<GpuVertex> vertices;
    vertices.reserve(surface_mesh->vertices.size());
    for (size_t i = 0; i < surface_mesh->vertices.size(); ++i) {
        const auto& p = surface_mesh->vertices[i];
        const auto& n = surface_mesh->normals[i];
        vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z});
    }

    glBindVertexArray(mesh_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(GpuVertex)), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    mesh_vertex_count_ = (int)vertices.size();
}

void PreviewRenderer::upload_line_mesh(const void* surface_mesh_ptr, const void* volume_mesh_ptr) {
    ensure_gl_resources();
    std::vector<geometry::MeshVec3> lines;
    if (volume_mesh_ptr) {
        const auto* volume_mesh = static_cast<const geometry::VolumeMeshData*>(volume_mesh_ptr);
        lines = decimate_line_vertices(volume_mesh->line_vertices, 180000);
    } else if (surface_mesh_ptr) {
        const auto* surface_mesh = static_cast<const geometry::SurfaceMeshData*>(surface_mesh_ptr);
        lines = decimate_line_vertices(surface_mesh->line_vertices, 120000);
    } else {
        line_vertex_count_ = 0;
        return;
    }

    glBindVertexArray(line_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, line_vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(lines.size() * sizeof(geometry::MeshVec3)), lines.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(geometry::MeshVec3), (void*)0);
    glBindVertexArray(0);
    line_vertex_count_ = (int)lines.size();
}

void PreviewRenderer::upload_result_mesh(const ProjectState& state) {
    ensure_gl_resources();
    const auto* volume_mesh = static_cast<const geometry::VolumeMeshData*>(state.volume_mesh_data);
    if (!volume_mesh || volume_mesh->nodes.empty() || volume_mesh->tets.empty() ||
        state.result_scalars.size() != volume_mesh->nodes.size()) {
        result_vertex_count_ = 0;
        return;
    }

    struct Face {
        int a = 0, b = 0, c = 0;
        int count = 0;
    };
    std::map<std::array<int, 3>, Face> faces;
    auto add_face = [&](int a, int b, int c) {
        std::array<int, 3> key = {a, b, c};
        std::sort(key.begin(), key.end());
        auto& f = faces[key];
        if (f.count == 0) {
            f.a = a; f.b = b; f.c = c;
        }
        ++f.count;
    };

    for (const auto& t : volume_mesh->tets) {
        add_face(t.a, t.c, t.b);
        add_face(t.a, t.b, t.d);
        add_face(t.b, t.c, t.d);
        add_face(t.c, t.a, t.d);
    }

    float lo = state.result_scalars.front();
    float hi = lo;
    for (float v : state.result_scalars) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    const float denom = std::max(hi - lo, 1e-12f);

    struct GpuVertex {
        float px, py, pz;
        float nx, ny, nz;
        float r, g, b;
    };
    std::vector<GpuVertex> vertices;
    vertices.reserve(faces.size() * 3);

    auto push = [&](int idx, const Vec3& normal) {
        const auto& p = volume_mesh->nodes[(std::size_t)idx];
        const float s = (state.result_scalars[(std::size_t)idx] - lo) / denom;
        const Vec3 c = jet_color(s);
        vertices.push_back({p.x, p.y, p.z, normal.x, normal.y, normal.z, c.x, c.y, c.z});
    };

    for (const auto& [_, f] : faces) {
        if (f.count != 1) continue;
        const auto& pa = volume_mesh->nodes[(std::size_t)f.a];
        const auto& pb = volume_mesh->nodes[(std::size_t)f.b];
        const auto& pc = volume_mesh->nodes[(std::size_t)f.c];
        const Vec3 a{pa.x, pa.y, pa.z};
        const Vec3 b{pb.x, pb.y, pb.z};
        const Vec3 c{pc.x, pc.y, pc.z};
        const Vec3 n = normalize(cross(b - a, c - a));
        push(f.a, n);
        push(f.b, n);
        push(f.c, n);
    }

    glBindVertexArray(result_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, result_vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(GpuVertex)), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    result_vertex_count_ = (int)vertices.size();
}

void PreviewRenderer::render(
    const ProjectState& state,
    int width,
    int height,
    float azimuth,
    float elevation,
    float distance,
    float pan_x,
    float pan_y
) {
    ensure_gl_resources();
    resize_fbo(width, height);

    auto* field = static_cast<geometry::FieldData*>(state.field_data);
    auto* surface_mesh = static_cast<geometry::SurfaceMeshData*>(state.surface_mesh_data);
    const bool show_result = state.has_results && !state.result_scalars.empty() && state.volume_mesh_data;
    const std::string result_key = show_result
        ? state.active_result + "|" + state.active_component + "|" + std::to_string(state.result_scalars.size()) + "|" + std::to_string(state.result_max)
        : std::string{};

    if (show_result) {
        if (cached_result_key_ != result_key || cached_volume_mesh_ != state.volume_mesh_data) {
            upload_result_mesh(state);
            cached_result_key_ = result_key;
            cached_volume_mesh_ = state.volume_mesh_data;
        }
    } else if (!field || field->empty()) {
        cached_field_ = nullptr;
        cached_surface_mesh_ = nullptr;
        cached_volume_mesh_ = nullptr;
        mesh_vertex_count_ = 0;
        line_vertex_count_ = 0;
        result_vertex_count_ = 0;
    } else if (state.view_display_mode == ViewDisplayMode::Mesh) {
        if (cached_surface_mesh_ != state.surface_mesh_data || cached_volume_mesh_ != state.volume_mesh_data) {
            upload_line_mesh(state.surface_mesh_data, state.volume_mesh_data);
            cached_surface_mesh_ = state.surface_mesh_data;
            cached_volume_mesh_ = state.volume_mesh_data;
        }
        if (surface_mesh && mesh_vertex_count_ == 0) {
            upload_surface_mesh(state.surface_mesh_data);
            cached_surface_mesh_ = state.surface_mesh_data;
        } else if (!surface_mesh && (cached_field_ != state.field_data || preview_tris_.empty())) {
            rebuild_preview_mesh(state);
        }
    } else if (surface_mesh && cached_surface_mesh_ != state.surface_mesh_data) {
        upload_surface_mesh(state.surface_mesh_data);
        upload_line_mesh(state.surface_mesh_data, state.volume_mesh_data);
        cached_surface_mesh_ = state.surface_mesh_data;
        cached_volume_mesh_ = state.volume_mesh_data;
    } else if (!surface_mesh && (cached_field_ != state.field_data || preview_tris_.empty())) {
        rebuild_preview_mesh(state);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, vp_w_, vp_h_);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.965f, 0.975f, 0.988f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const bool has_result_view = show_result && result_vertex_count_ > 0;
    const bool has_mesh_view = !show_result && state.view_display_mode == ViewDisplayMode::Mesh && line_vertex_count_ > 0;
    const bool has_surface_view = state.view_display_mode != ViewDisplayMode::Mesh && mesh_vertex_count_ > 0;
    if (has_result_view || has_mesh_view || has_surface_view) {
        const float aspect = vp_h_ > 0 ? (float)vp_w_ / (float)vp_h_ : 1.0f;
        const float radius = 0.5f * std::max({state.size_x, state.size_y, state.size_z, 1.0f});
        const float az = azimuth * 3.14159265f / 180.0f;
        const float el = elevation * 3.14159265f / 180.0f;
        const Vec3 target {pan_x * 0.04f * radius, 0.0f, pan_y * 0.04f * radius};
        const Vec3 eye {
            target.x + distance * std::cos(el) * std::cos(az),
            target.y + distance * std::cos(el) * std::sin(az),
            target.z + distance * std::sin(el)
        };

        const Mat4 model = identity_mat4();
        const Mat4 view = look_at(eye, target, {0.f, 0.f, 1.f});
        const Mat4 proj = perspective(45.0f * 3.14159265f / 180.0f, aspect, 0.1f, 1000.0f);
        const Mat4 mvp = multiply(proj, multiply(view, model));

        if (has_result_view) {
            glUseProgram(result_shader_program_);
            glUniformMatrix4fv(glGetUniformLocation(result_shader_program_, "u_mvp"), 1, GL_FALSE, mvp.m.data());
            glUniformMatrix4fv(glGetUniformLocation(result_shader_program_, "u_model"), 1, GL_FALSE, model.m.data());
            glBindVertexArray(result_vao_);
            glDrawArrays(GL_TRIANGLES, 0, result_vertex_count_);
            glBindVertexArray(0);
            glUseProgram(0);
        } else if (state.view_display_mode == ViewDisplayMode::Mesh) {
            glUseProgram(line_shader_program_);
            glUniformMatrix4fv(glGetUniformLocation(line_shader_program_, "u_mvp"), 1, GL_FALSE, mvp.m.data());
            glUniform3f(glGetUniformLocation(line_shader_program_, "u_color"), 0.18f, 0.39f, 0.66f);
            glBindVertexArray(line_vao_);
            glLineWidth(state.has_volume_mesh ? 1.4f : 1.6f);
            glDrawArrays(GL_LINES, 0, line_vertex_count_);
            glBindVertexArray(0);
            glUseProgram(0);
        } else if (state.view_display_mode == ViewDisplayMode::Surface) {
            glUseProgram(shader_program_);
            glUniformMatrix4fv(glGetUniformLocation(shader_program_, "u_mvp"), 1, GL_FALSE, mvp.m.data());
            glUniformMatrix4fv(glGetUniformLocation(shader_program_, "u_model"), 1, GL_FALSE, model.m.data());
            glUniform3f(glGetUniformLocation(shader_program_, "u_light_dir"), -0.45f, 0.65f, 0.72f);
            glUniform3f(glGetUniformLocation(shader_program_, "u_eye"), eye.x, eye.y, eye.z);
            glBindVertexArray(mesh_vao_);
            glUniform3f(glGetUniformLocation(shader_program_, "u_base_color"), 0.64f, 0.77f, 0.90f);
            glDrawArrays(GL_TRIANGLES, 0, mesh_vertex_count_);
            glBindVertexArray(0);
            glUseProgram(0);
            if (line_vertex_count_ > 0) {
                glUseProgram(line_shader_program_);
                glUniformMatrix4fv(glGetUniformLocation(line_shader_program_, "u_mvp"), 1, GL_FALSE, mvp.m.data());
                glUniform3f(glGetUniformLocation(line_shader_program_, "u_color"), 0.21f, 0.41f, 0.60f);
                glBindVertexArray(line_vao_);
                glLineWidth(1.1f);
                glDrawArrays(GL_LINES, 0, line_vertex_count_);
                glBindVertexArray(0);
                glUseProgram(0);
            }
        } else {
            glUseProgram(shader_program_);
            glUniformMatrix4fv(glGetUniformLocation(shader_program_, "u_mvp"), 1, GL_FALSE, mvp.m.data());
            glUniformMatrix4fv(glGetUniformLocation(shader_program_, "u_model"), 1, GL_FALSE, model.m.data());
            glUniform3f(glGetUniformLocation(shader_program_, "u_light_dir"), -0.45f, 0.65f, 0.72f);
            glUniform3f(glGetUniformLocation(shader_program_, "u_eye"), eye.x, eye.y, eye.z);
            glBindVertexArray(mesh_vao_);
            glUniform3f(glGetUniformLocation(shader_program_, "u_base_color"), 0.45f, 0.67f, 0.88f);
            glDrawArrays(GL_TRIANGLES, 0, mesh_vertex_count_);
            glBindVertexArray(0);
            glUseProgram(0);
        }
    }

    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace tpms::ui::preview
