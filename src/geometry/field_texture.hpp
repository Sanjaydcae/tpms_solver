#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

#define GL_GLEXT_PROTOTYPES
#ifdef __APPLE__
#  include <OpenGL/gl3.h>
#else
#  include <GL/gl.h>
#  include <GL/glext.h>
#endif

#include "tpms_field.hpp"

namespace tpms::geometry {

// ── Colormaps ─────────────────────────────────────────────────────────────────

// Viridis-like dark-to-bright colormap (5-stop approximation)
inline void viridis(float t, uint8_t& r, uint8_t& g, uint8_t& b) {
    t = std::clamp(t, 0.f, 1.f);
    // Stops: deep purple → blue → teal → green → yellow
    const float stops[5][3] = {
        {0.267f, 0.005f, 0.329f},
        {0.283f, 0.141f, 0.558f},
        {0.129f, 0.566f, 0.551f},
        {0.369f, 0.788f, 0.384f},
        {0.993f, 0.906f, 0.144f},
    };
    float s = t * 4.f;
    int   i = (int)s;
    i = std::clamp(i, 0, 3);
    float f = s - i;
    r = (uint8_t)((stops[i][0]*(1-f) + stops[i+1][0]*f) * 255.f);
    g = (uint8_t)((stops[i][1]*(1-f) + stops[i+1][1]*f) * 255.f);
    b = (uint8_t)((stops[i][2]*(1-f) + stops[i+1][2]*f) * 255.f);
}

// Gaussian glow: dark background, bright band near zero-crossing
inline void glow_colormap(float field_val, float f_min, float f_max,
                           uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
    float range  = std::max(f_max - f_min, 1e-6f);
    float n      = field_val / (range * 0.5f);       // normalised to [-1, 1]
    float glow   = std::exp(-10.f * n * n);

    // Light engineering palette: pale background with a crisp blue iso-band
    float base_r = 0.95f;
    float base_g = 0.97f;
    float base_b = 0.99f;
    float band_r = 0.14f;
    float band_g = 0.46f;
    float band_b = 0.78f;

    r = (uint8_t)((base_r * (1.f - glow) + band_r * glow) * 255.f);
    g = (uint8_t)((base_g * (1.f - glow) + band_g * glow) * 255.f);
    b = (uint8_t)((base_b * (1.f - glow) + band_b * glow) * 255.f);
    a = 255;
    r = std::min(r, (uint8_t)255);
    g = std::min(g, (uint8_t)255);
    b = std::min(b, (uint8_t)255);
}

// ── Texture management ────────────────────────────────────────────────────────

struct FieldTexture {
    GLuint tex_id  = 0;
    int    width   = 0;
    int    height  = 0;
    SliceAxis cached_axis = SliceAxis::XY;
    int cached_index = -1;

    void upload_slice(const FieldData& fd, SliceAxis axis = SliceAxis::XY, int slice_index = -1) {
        if (fd.empty()) return;

        float f_min = fd.field_min();
        float f_max = fd.field_max();
        int W = 0;
        int H = 0;
        std::vector<float> slice;

        switch (axis) {
            case SliceAxis::XY: {
                if (slice_index < 0) slice_index = fd.nz / 2;
                slice_index = std::clamp(slice_index, 0, fd.nz - 1);
                W = fd.nx;
                H = fd.ny;
                slice.resize(W * H);
                for (int iy = 0; iy < H; ++iy)
                    for (int ix = 0; ix < W; ++ix)
                        slice[ix + W * iy] = fd.at(ix, iy, slice_index);
                break;
            }
            case SliceAxis::XZ: {
                if (slice_index < 0) slice_index = fd.ny / 2;
                slice_index = std::clamp(slice_index, 0, fd.ny - 1);
                W = fd.nx;
                H = fd.nz;
                slice.resize(W * H);
                for (int iz = 0; iz < H; ++iz)
                    for (int ix = 0; ix < W; ++ix)
                        slice[ix + W * iz] = fd.at(ix, slice_index, iz);
                break;
            }
            case SliceAxis::YZ: {
                if (slice_index < 0) slice_index = fd.nx / 2;
                slice_index = std::clamp(slice_index, 0, fd.nx - 1);
                W = fd.ny;
                H = fd.nz;
                slice.resize(W * H);
                for (int iz = 0; iz < H; ++iz)
                    for (int iy = 0; iy < W; ++iy)
                        slice[iy + W * iz] = fd.at(slice_index, iy, iz);
                break;
            }
        }

        std::vector<uint8_t> pixels(W * H * 4);
        for (int iy = 0; iy < H; ++iy) {
            for (int ix = 0; ix < W; ++ix) {
                float v = slice[ix + W * iy];
                uint8_t r, g, b, a;
                glow_colormap(v, f_min, f_max, r, g, b, a);
                int idx = (iy * W + ix) * 4;
                pixels[idx+0] = r;
                pixels[idx+1] = g;
                pixels[idx+2] = b;
                pixels[idx+3] = a;
            }
        }

        if (!tex_id) glGenTextures(1, &tex_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        width  = W;
        height = H;
        cached_axis = axis;
        cached_index = slice_index;
    }

    void free() {
        if (tex_id) { glDeleteTextures(1, &tex_id); tex_id = 0; }
    }

    bool valid() const { return tex_id != 0; }
};

} // namespace tpms::geometry
