#include "vtk_viewport.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

// VTK
#include <vtkActor.h>
#include <vtkAxesActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCaptionActor2D.h>
#include <vtkDataSetMapper.h>
#include <vtkFloatArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkGeometryFilter.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkOpenGLFramebufferObject.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkSmartPointer.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkTextureObject.h>
#include <vtkTransform.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace tpms::preview {

// ── Pimpl ─────────────────────────────────────────────────────────────────────

struct VtkViewport::Impl {
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> win;

    // Layer 0 – main 3-D scene
    vtkSmartPointer<vtkRenderer> ren;

    // Layer 1 – 2-D overlays (scalar bar, info text) — transparent bg
    vtkSmartPointer<vtkRenderer> ren2d;

    // Layer 2 – corner axes mini-view
    vtkSmartPointer<vtkRenderer> ren_axes;

    // Geometry actors
    vtkSmartPointer<vtkActor> surf_actor;
    vtkSmartPointer<vtkActor> vol_wire_actor;
    vtkSmartPointer<vtkActor> result_actor;
    vtkSmartPointer<vtkActor> result_edge_actor;

    // 2-D overlays
    vtkSmartPointer<vtkScalarBarActor> scalar_bar;
    vtkSmartPointer<vtkTextActor>      info_text;

    // Axes (drawn in corner renderer)
    vtkSmartPointer<vtkAxesActor> axes_actor;

    bool win_initialized = false;
    int  vp_h = 1;

    bool  lmb_down = false, rmb_down = false;
    float last_x = 0.f, last_y = 0.f;
};

// ── Geometry helpers ──────────────────────────────────────────────────────────

static vtkSmartPointer<vtkPolyData> surf_to_pd(const geometry::SurfaceMeshData& m) {
    auto pts = vtkSmartPointer<vtkPoints>::New();
    pts->SetNumberOfPoints((vtkIdType)m.vertices.size());
    for (vtkIdType i = 0; i < (vtkIdType)m.vertices.size(); ++i)
        pts->SetPoint(i, m.vertices[i].x, m.vertices[i].y, m.vertices[i].z);
    auto cells = vtkSmartPointer<vtkCellArray>::New();
    for (const auto& t : m.triangles) {
        vtkIdType ids[3] = {t.a, t.b, t.c};
        cells->InsertNextCell(3, ids);
    }
    auto norms = vtkSmartPointer<vtkFloatArray>::New();
    norms->SetNumberOfComponents(3);
    norms->SetNumberOfTuples((vtkIdType)m.normals.size());
    for (vtkIdType i = 0; i < (vtkIdType)m.normals.size(); ++i)
        norms->SetTuple3(i, m.normals[i].x, m.normals[i].y, m.normals[i].z);
    auto pd = vtkSmartPointer<vtkPolyData>::New();
    pd->SetPoints(pts); pd->SetPolys(cells);
    pd->GetPointData()->SetNormals(norms);
    return pd;
}

static vtkSmartPointer<vtkUnstructuredGrid> vol_to_ug(const geometry::VolumeMeshData& m) {
    auto pts = vtkSmartPointer<vtkPoints>::New();
    pts->SetNumberOfPoints((vtkIdType)m.nodes.size());
    for (vtkIdType i = 0; i < (vtkIdType)m.nodes.size(); ++i)
        pts->SetPoint(i, m.nodes[i].x, m.nodes[i].y, m.nodes[i].z);
    auto ug = vtkSmartPointer<vtkUnstructuredGrid>::New();
    ug->SetPoints(pts);
    ug->Allocate((vtkIdType)m.tets.size());
    for (const auto& t : m.tets) {
        vtkIdType ids[4] = {t.a, t.b, t.c, t.d};
        ug->InsertNextCell(VTK_TETRA, 4, ids);
    }
    return ug;
}

static vtkSmartPointer<vtkLookupTable> rainbow_lut(double lo, double hi) {
    if (std::abs(hi - lo) < 1e-12) {
        const double pad = std::max(std::abs(lo) * 0.05, 1e-9);
        lo -= pad;
        hi += pad;
    }
    auto lut = vtkSmartPointer<vtkLookupTable>::New();
    lut->SetRange(lo, hi);
    lut->SetHueRange(0.667, 0.0);
    lut->SetSaturationRange(1, 1);
    lut->SetValueRange(1, 1);
    lut->SetNumberOfColors(512);
    lut->Build();
    return lut;
}

static vtkSmartPointer<vtkPolyData> boundary_surface(vtkUnstructuredGrid* ug) {
    auto geom = vtkSmartPointer<vtkGeometryFilter>::New();
    geom->SetInputData(ug);
    geom->Update();
    auto pd = vtkSmartPointer<vtkPolyData>::New();
    pd->ShallowCopy(geom->GetOutput());
    return pd;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

VtkViewport::VtkViewport()  = default;
VtkViewport::~VtkViewport() { cleanup(); }

void VtkViewport::init() {
    if (initialised_) return;
    d_ = new Impl();

    d_->win = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    d_->win->SetOffScreenRendering(1);
    d_->win->SetSize(4, 4);
    d_->win->SetNumberOfLayers(3);

    // ── Layer 0: main 3-D scene ───────────────────────────────────────────────
    d_->ren = vtkSmartPointer<vtkRenderer>::New();
    d_->ren->SetLayer(0);
    d_->ren->SetBackground(0.12, 0.14, 0.18);
    d_->ren->SetBackground2(0.04, 0.05, 0.07);
    d_->ren->GradientBackgroundOn();
    d_->win->AddRenderer(d_->ren);

    // ── Layer 1: 2-D overlays (transparent background) ───────────────────────
    d_->ren2d = vtkSmartPointer<vtkRenderer>::New();
    d_->ren2d->SetLayer(1);
    d_->ren2d->InteractiveOff();
    d_->ren2d->SetBackground(0, 0, 0);
    d_->ren2d->SetBackgroundAlpha(0.0);
    d_->win->AddRenderer(d_->ren2d);

    // ── Layer 2: corner axes ──────────────────────────────────────────────────
    d_->ren_axes = vtkSmartPointer<vtkRenderer>::New();
    d_->ren_axes->SetLayer(2);
    d_->ren_axes->InteractiveOff();
    d_->ren_axes->SetBackground(0, 0, 0);
    d_->ren_axes->SetBackgroundAlpha(0.0);
    d_->ren_axes->SetViewport(0.80, 0.0, 1.0, 0.20);  // bottom-right 20%
    d_->win->AddRenderer(d_->ren_axes);

    // ── Result info text (top-left) ───────────────────────────────────────────
    d_->info_text = vtkSmartPointer<vtkTextActor>::New();
    d_->info_text->SetInput("");
    d_->info_text->GetPositionCoordinate()
        ->SetCoordinateSystemToNormalizedViewport();
    d_->info_text->GetPositionCoordinate()->SetValue(0.01, 0.80);
    auto* tp = d_->info_text->GetTextProperty();
    tp->SetFontFamilyToArial();
    tp->SetFontSize(12);
    tp->SetColor(1, 1, 1);
    tp->SetBackgroundColor(0.06, 0.06, 0.06);
    tp->SetBackgroundOpacity(0.70);
    tp->SetLineSpacing(1.5);
    d_->ren2d->AddActor2D(d_->info_text);

    // ── Axes actor in corner renderer ─────────────────────────────────────────
    d_->axes_actor = vtkSmartPointer<vtkAxesActor>::New();
    d_->axes_actor->SetTotalLength(1.5, 1.5, 1.5);
    d_->axes_actor->SetShaftTypeToCylinder();
    d_->axes_actor->SetCylinderRadius(0.05);
    d_->axes_actor->GetXAxisCaptionActor2D()->GetTextActor()
        ->GetTextProperty()->SetFontSize(14);
    d_->axes_actor->GetYAxisCaptionActor2D()->GetTextActor()
        ->GetTextProperty()->SetFontSize(14);
    d_->axes_actor->GetZAxisCaptionActor2D()->GetTextActor()
        ->GetTextProperty()->SetFontSize(14);
    d_->ren_axes->AddActor(d_->axes_actor);
    d_->ren_axes->ResetCamera();

    initialised_ = true;
}

void VtkViewport::cleanup() {
    if (!initialised_) return;
    delete d_; d_ = nullptr;
    initialised_ = false;
}

// ── Geometry setters ──────────────────────────────────────────────────────────

void VtkViewport::set_surface_mesh(const geometry::SurfaceMeshData* mesh) {
    if (!initialised_) return;
    if (d_->surf_actor) { d_->ren->RemoveActor(d_->surf_actor); d_->surf_actor = nullptr; }
    if (!mesh) return;

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(surf_to_pd(*mesh));

    d_->surf_actor = vtkSmartPointer<vtkActor>::New();
    d_->surf_actor->SetMapper(mapper);
    d_->surf_actor->GetProperty()->SetColor(0.72, 0.80, 0.92);
    d_->surf_actor->GetProperty()->SetAmbient(0.20);
    d_->surf_actor->GetProperty()->SetDiffuse(0.72);
    d_->surf_actor->GetProperty()->SetSpecular(0.12);
    d_->ren->AddActor(d_->surf_actor);
    d_->ren->ResetCamera();
}

void VtkViewport::set_volume_mesh(const geometry::VolumeMeshData* mesh) {
    if (!initialised_) return;
    if (d_->vol_wire_actor) { d_->ren->RemoveActor(d_->vol_wire_actor); d_->vol_wire_actor = nullptr; }
    if (!mesh) return;

    auto ug = vol_to_ug(*mesh);
    auto mapper = vtkSmartPointer<vtkDataSetMapper>::New();
    mapper->SetInputData(ug);
    mapper->ScalarVisibilityOff();

    d_->vol_wire_actor = vtkSmartPointer<vtkActor>::New();
    d_->vol_wire_actor->SetMapper(mapper);
    d_->vol_wire_actor->GetProperty()->SetRepresentationToWireframe();
    d_->vol_wire_actor->GetProperty()->SetColor(0.10, 0.32, 0.62);
    d_->vol_wire_actor->GetProperty()->SetLineWidth(0.45f);
    d_->vol_wire_actor->GetProperty()->SetOpacity(0.92);
    d_->vol_wire_actor->GetProperty()->SetAmbient(0.22);
    d_->vol_wire_actor->GetProperty()->SetDiffuse(0.78);
    d_->vol_wire_actor->GetProperty()->SetSpecular(0.10);
    d_->ren->AddActor(d_->vol_wire_actor);
    d_->ren->ResetCamera();
}

// ── Results ───────────────────────────────────────────────────────────────────

void VtkViewport::clear_result() {
    if (!initialised_) return;
    if (d_->result_actor)      { d_->ren->RemoveActor(d_->result_actor);       d_->result_actor      = nullptr; }
    if (d_->result_edge_actor) { d_->ren->RemoveActor(d_->result_edge_actor);  d_->result_edge_actor = nullptr; }
    if (d_->scalar_bar)        { d_->ren2d->RemoveActor2D(d_->scalar_bar);     d_->scalar_bar        = nullptr; }
    if (d_->vol_wire_actor) d_->vol_wire_actor->SetVisibility(true);
    d_->info_text->SetInput("");
}

void VtkViewport::set_result(
    const geometry::VolumeMeshData* mesh,
    const std::vector<float>&       scalars,
    const std::string&              title,
    const std::string&              unit,
    float                           /*deform_scale*/)
{
    clear_result();
    if (!initialised_ || !mesh || scalars.empty()) return;

    auto ug = vol_to_ug(*mesh);
    float lo = scalars[0], hi = scalars[0];
    for (float v : scalars) { lo = std::min(lo,v); hi = std::max(hi,v); }

    auto arr = vtkSmartPointer<vtkFloatArray>::New();
    arr->SetName(title.c_str());
    arr->SetNumberOfValues((vtkIdType)scalars.size());
    for (vtkIdType i = 0; i < (vtkIdType)scalars.size(); ++i)
        arr->SetValue(i, scalars[i]);
    ug->GetPointData()->SetScalars(arr);

    auto lut = rainbow_lut(lo, hi);

    // Solid colored result
    auto boundary = boundary_surface(ug);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(boundary);
    mapper->SetLookupTable(lut);
    mapper->SetScalarRange(lo, hi);
    mapper->SetScalarModeToUsePointData();
    d_->result_actor = vtkSmartPointer<vtkActor>::New();
    d_->result_actor->SetMapper(mapper);
    d_->result_actor->GetProperty()->SetAmbient(0.25);
    d_->result_actor->GetProperty()->SetDiffuse(0.75);
    d_->result_actor->GetProperty()->SetSpecular(0.08);
    d_->ren->AddActor(d_->result_actor);

    if (d_->vol_wire_actor) d_->vol_wire_actor->SetVisibility(false);

    // Boundary mesh edges only, not all internal tetrahedra.
    auto emapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    emapper->SetInputData(boundary);
    emapper->ScalarVisibilityOff();
    d_->result_edge_actor = vtkSmartPointer<vtkActor>::New();
    d_->result_edge_actor->SetMapper(emapper);
    d_->result_edge_actor->GetProperty()->SetRepresentationToWireframe();
    d_->result_edge_actor->GetProperty()->SetColor(0.02, 0.03, 0.04);
    d_->result_edge_actor->GetProperty()->SetLineWidth(0.25f);
    d_->result_edge_actor->GetProperty()->SetOpacity(0.18);
    d_->ren->AddActor(d_->result_edge_actor);

    // Scalar bar — left side, like PrePoMax
    d_->scalar_bar = vtkSmartPointer<vtkScalarBarActor>::New();
    d_->scalar_bar->SetLookupTable(lut);
    d_->scalar_bar->SetTitle((title + "\n" + unit).c_str());
    d_->scalar_bar->SetNumberOfLabels(10);
    d_->scalar_bar->SetOrientationToVertical();
    d_->scalar_bar->SetWidth(0.09);
    d_->scalar_bar->SetHeight(0.62);
    d_->scalar_bar->GetPositionCoordinate()
        ->SetCoordinateSystemToNormalizedViewport();
    d_->scalar_bar->GetPositionCoordinate()->SetValue(0.01, 0.16);
    d_->scalar_bar->GetTitleTextProperty()->SetFontSize(11);
    d_->scalar_bar->GetTitleTextProperty()->SetColor(1,1,1);
    d_->scalar_bar->GetLabelTextProperty()->SetFontSize(10);
    d_->scalar_bar->GetLabelTextProperty()->SetColor(1,1,1);
    d_->ren2d->AddActor2D(d_->scalar_bar);

    d_->ren->ResetCamera();
}

void VtkViewport::set_result_info(const std::string& text) {
    if (!initialised_) return;
    d_->info_text->SetInput(text.c_str());
}

// ── FBO (no-op — texture owned by VTK) ───────────────────────────────────────

void VtkViewport::ensure_fbo(int w, int h) {
    last_w_ = w; last_h_ = h;
    if (d_) d_->vp_h = h;
}

// ── Render ────────────────────────────────────────────────────────────────────

GLuint VtkViewport::render(int w, int h) {
    if (!initialised_ || w < 1 || h < 1) return 0;

    d_->win->SetSize(w, h);
    if (d_) d_->vp_h = h;
    last_w_ = w; last_h_ = h;

    // Sync axes camera to match main camera orientation
    if (d_->win_initialized && d_->ren->GetActiveCamera()) {
        auto* main_cam  = d_->ren->GetActiveCamera();
        auto* axes_cam  = d_->ren_axes->GetActiveCamera();
        axes_cam->SetPosition(main_cam->GetPosition());
        axes_cam->SetFocalPoint(main_cam->GetFocalPoint());
        axes_cam->SetViewUp(main_cam->GetViewUp());
        d_->ren_axes->ResetCamera();
    }

    if (!d_->win_initialized) {
        d_->win->Initialize();  // No interactor — just initialize the window
        d_->win_initialized = true;
    }

    // Save & restore ImGui's FBO binding around VTK render
    GLint prev_draw = 0, prev_read = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read);

    d_->win->Render();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read);

    // Return VTK's offscreen colour attachment directly — zero-copy
    vtkOpenGLFramebufferObject* fbo = d_->win->GetRenderFramebuffer();
    if (!fbo) return 0;
    vtkTextureObject* tex = fbo->GetColorAttachmentAsTextureObject(0);
    if (!tex) return 0;
    return static_cast<GLuint>(tex->GetHandle());
}

void VtkViewport::reset_camera() {
    if (initialised_) d_->ren->ResetCamera();
}

// ── Camera (no interactor — direct camera control) ───────────────────────────

void VtkViewport::mouse_button(int btn, bool pressed, float x, float y) {
    if (!initialised_) return;
    if (btn == 0) d_->lmb_down = pressed;
    if (btn == 1) d_->rmb_down = pressed;
    d_->last_x = x; d_->last_y = y;
}

void VtkViewport::mouse_move(float x, float y) {
    if (!initialised_) return;
    float dx = x - d_->last_x, dy = y - d_->last_y;
    d_->last_x = x; d_->last_y = y;
    auto* cam = d_->ren->GetActiveCamera();
    if (!cam) return;
    if (d_->lmb_down) {
        cam->Azimuth(-dx * 0.4);
        cam->Elevation( dy * 0.3);
        cam->OrthogonalizeViewUp();
    } else if (d_->rmb_down) {
        double bd[6] = {0,1,0,1,0,1};
        if (d_->result_actor)   d_->result_actor->GetBounds(bd);
        else if (d_->surf_actor) d_->surf_actor->GetBounds(bd);
        double span = std::max({bd[1]-bd[0], bd[3]-bd[2], bd[5]-bd[4]});
        double s = span / std::max(1, d_->vp_h) * 0.5;
        cam->SetFocalPoint(cam->GetFocalPoint()[0]-dx*s,
                           cam->GetFocalPoint()[1]+dy*s,
                           cam->GetFocalPoint()[2]);
        cam->SetPosition   (cam->GetPosition()[0]-dx*s,
                            cam->GetPosition()[1]+dy*s,
                            cam->GetPosition()[2]);
    }
    d_->ren->ResetCameraClippingRange();
}

void VtkViewport::mouse_wheel(float delta) {
    if (!initialised_) return;
    auto* cam = d_->ren->GetActiveCamera();
    if (!cam) return;
    cam->Dolly(delta > 0 ? 0.85 : 1.15);
    d_->ren->ResetCameraClippingRange();
}

} // namespace tpms::preview
