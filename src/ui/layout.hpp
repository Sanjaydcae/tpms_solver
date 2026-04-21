#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace tpms::ui {

//  ┌──────────────────────────────────────────────────────┐
//  │  Main menu bar                                       │
//  ├──────────┬───────────────────────────────┬───────────┤
//  │  Model   │       3D Viewport             │ Properties│
//  │  Tree    │                               │           │
//  ├──────────┴───────────────────────────────┴───────────┤
//  │  Console / Log                                       │
//  └──────────────────────────────────────────────────────┘

struct DockIDs {
    ImGuiID root   = 0;
    ImGuiID left   = 0;
    ImGuiID centre = 0;
    ImGuiID right  = 0;
    ImGuiID bottom = 0;
    bool    built  = false;
};

inline DockIDs& get_dock_ids() {
    static DockIDs ids;
    return ids;
}

// Returns the dockspace ID and (re)builds the layout on first launch.
// Call BEFORE drawing any panel windows each frame.
inline ImGuiID begin_dockspace(float top_offset = 0.f, float bottom_offset = 0.f) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->WorkPos.x, vp->WorkPos.y + top_offset});
    ImGui::SetNextWindowSize({vp->WorkSize.x, vp->WorkSize.y - top_offset - bottom_offset});
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoDocking
      | ImGuiWindowFlags_NoTitleBar
      | ImGuiWindowFlags_NoCollapse
      | ImGuiWindowFlags_NoResize
      | ImGuiWindowFlags_NoMove
      | ImGuiWindowFlags_NoBringToFrontOnFocus
      | ImGuiWindowFlags_NoNavFocus
      | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   {0, 0});
    ImGui::Begin("##dockspace_host", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    ImGuiID id = ImGui::GetID("##main_dockspace");
    ImGui::DockSpace(id, {0, 0}, ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();

    // ── Build default layout once per process lifetime ────────────────────
    DockIDs& ids = get_dock_ids();
    if (!ids.built) {
        ids.built = true;

        ImGuiDockNode* node = ImGui::DockBuilderGetNode(id);
        // Only rebuild if the dockspace has no children (first ever launch,
        // no valid ini). If ini gave us a split tree, keep it.
        if (!node || !node->IsSplitNode()) {
            ImGui::DockBuilderRemoveNode(id);
            ImGui::DockBuilderAddNode(id,
                ImGuiDockNodeFlags_DockSpace |
                ImGuiDockNodeFlags_NoWindowMenuButton);
            ImGui::DockBuilderSetNodeSize(id, vp->Size);

            ids.root = id;
            ImGuiID tmp = id;

            ImGui::DockBuilderSplitNode(tmp,      ImGuiDir_Down,  0.17f, &ids.bottom, &tmp);
            ImGui::DockBuilderSplitNode(tmp,      ImGuiDir_Left,  0.15f, &ids.left,   &tmp);
            ImGui::DockBuilderSplitNode(tmp,      ImGuiDir_Right, 0.28f, &ids.right,  &ids.centre);

            ImGui::DockBuilderDockWindow("Model Tree",                       ids.left);
            ImGui::DockBuilderDockWindow("3D Graphics Window",               ids.centre);
            ImGui::DockBuilderDockWindow("Properties",                       ids.right);
            ImGui::DockBuilderDockWindow("Messages / Solver Log / Progress", ids.bottom);
            ImGui::DockBuilderFinish(id);
        } else {
            // Recover IDs from the saved tree so SetNextWindowDockID works
            // even after loading from ini (best-effort; windows stay docked).
            ids.root = id;
        }
    }

    return id;
}

// Helper: call BEFORE each panel's ImGui::Begin() to guarantee it lands in
// the correct slot even if the ini is missing or corrupted.
inline void pin_window(const char* name, ImGuiID dock_id) {
    if (dock_id == 0) return;
    ImGuiWindow* win = ImGui::FindWindowByName(name);
    // Only force-dock if the window has never been docked (new session)
    if (!win || win->DockId == 0)
        ImGui::SetNextWindowDockID(dock_id, ImGuiCond_Always);
}

} // namespace tpms::ui
