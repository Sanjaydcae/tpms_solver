#pragma once

#include <imgui.h>

namespace tpms::ui {

// Light professional CAE theme
inline void apply_theme() {
    ImGui::StyleColorsLight();
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding    = 5.f;
    s.ChildRounding     = 5.f;
    s.FrameRounding     = 5.f;
    s.PopupRounding     = 4.f;
    s.ScrollbarRounding = 6.f;
    s.GrabRounding      = 4.f;
    s.TabRounding       = 5.f;

    s.WindowPadding     = {10, 9};
    s.FramePadding      = {10, 6};
    s.ItemSpacing       = {8, 7};
    s.ItemInnerSpacing  = {6, 5};
    s.CellPadding       = {7, 5};
    s.ScrollbarSize     = 13.f;
    s.GrabMinSize       = 10.f;
    s.WindowBorderSize  = 1.f;
    s.FrameBorderSize   = 1.f;
    s.TabBorderSize     = 1.f;

    auto* c = s.Colors;
    c[ImGuiCol_Text]                 = {0.13f, 0.16f, 0.22f, 1.f};
    c[ImGuiCol_TextDisabled]         = {0.38f, 0.45f, 0.54f, 1.f};
    c[ImGuiCol_WindowBg]             = {0.94f, 0.96f, 0.98f, 1.f};
    c[ImGuiCol_ChildBg]              = {0.97f, 0.98f, 0.995f, 1.f};
    c[ImGuiCol_PopupBg]              = {1.00f, 1.00f, 1.00f, 0.98f};

    c[ImGuiCol_Border]               = {0.73f, 0.79f, 0.86f, 1.f};
    c[ImGuiCol_BorderShadow]         = {0.f, 0.f, 0.f, 0.f};

    c[ImGuiCol_FrameBg]              = {1.00f, 1.00f, 1.00f, 1.f};
    c[ImGuiCol_FrameBgHovered]       = {0.92f, 0.96f, 1.00f, 1.f};
    c[ImGuiCol_FrameBgActive]        = {0.87f, 0.92f, 0.98f, 1.f};

    c[ImGuiCol_TitleBg]              = {0.90f, 0.94f, 0.98f, 1.f};
    c[ImGuiCol_TitleBgActive]        = {0.90f, 0.94f, 0.98f, 1.f};
    c[ImGuiCol_TitleBgCollapsed]     = {0.90f, 0.94f, 0.98f, 0.8f};

    c[ImGuiCol_MenuBarBg]            = {0.92f, 0.95f, 0.985f, 1.f};

    c[ImGuiCol_ScrollbarBg]          = {0.92f, 0.95f, 0.98f, 1.f};
    c[ImGuiCol_ScrollbarGrab]        = {0.67f, 0.75f, 0.84f, 1.f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.43f, 0.59f, 0.79f, 1.f};
    c[ImGuiCol_ScrollbarGrabActive]  = {0.27f, 0.47f, 0.69f, 1.f};

    c[ImGuiCol_CheckMark]            = {0.14f, 0.42f, 0.72f, 1.f};
    c[ImGuiCol_SliderGrab]           = {0.14f, 0.42f, 0.72f, 1.f};
    c[ImGuiCol_SliderGrabActive]     = {0.10f, 0.34f, 0.60f, 1.f};

    c[ImGuiCol_Button]               = {0.93f, 0.96f, 0.995f, 1.f};
    c[ImGuiCol_ButtonHovered]        = {0.86f, 0.92f, 0.98f, 1.f};
    c[ImGuiCol_ButtonActive]         = {0.77f, 0.87f, 0.97f, 1.f};

    c[ImGuiCol_Header]               = {0.88f, 0.93f, 0.98f, 1.f};
    c[ImGuiCol_HeaderHovered]        = {0.80f, 0.89f, 0.98f, 1.f};
    c[ImGuiCol_HeaderActive]         = {0.72f, 0.84f, 0.96f, 1.f};

    c[ImGuiCol_Separator]            = {0.78f, 0.83f, 0.88f, 1.f};
    c[ImGuiCol_SeparatorHovered]     = {0.46f, 0.64f, 0.84f, 1.f};
    c[ImGuiCol_SeparatorActive]      = {0.33f, 0.54f, 0.76f, 1.f};

    c[ImGuiCol_ResizeGrip]           = {0.33f, 0.54f, 0.76f, 0.25f};
    c[ImGuiCol_ResizeGripHovered]    = {0.33f, 0.54f, 0.76f, 0.60f};
    c[ImGuiCol_ResizeGripActive]     = {0.33f, 0.54f, 0.76f, 0.95f};

    c[ImGuiCol_Tab]                  = {0.90f, 0.94f, 0.985f, 1.f};
    c[ImGuiCol_TabHovered]           = {0.83f, 0.90f, 0.98f, 1.f};
    c[ImGuiCol_TabActive]            = {1.00f, 1.00f, 1.00f, 1.f};
    c[ImGuiCol_TabUnfocused]         = {0.92f, 0.95f, 0.98f, 1.f};
    c[ImGuiCol_TabUnfocusedActive]   = {0.965f, 0.98f, 0.995f, 1.f};

    c[ImGuiCol_DockingPreview]       = {0.11f, 0.49f, 0.79f, 0.22f};
    c[ImGuiCol_DockingEmptyBg]       = {0.95f, 0.97f, 0.99f, 1.f};

    c[ImGuiCol_TableHeaderBg]        = {0.92f, 0.95f, 0.98f, 1.f};
    c[ImGuiCol_TableBorderStrong]    = {0.77f, 0.82f, 0.88f, 1.f};
    c[ImGuiCol_TableBorderLight]     = {0.85f, 0.89f, 0.94f, 1.f};
    c[ImGuiCol_TableRowBg]           = {1.f, 1.f, 1.f, 0.f};
    c[ImGuiCol_TableRowBgAlt]        = {0.97f, 0.98f, 1.00f, 1.f};

    c[ImGuiCol_TextSelectedBg]       = {0.33f, 0.54f, 0.76f, 0.22f};
    c[ImGuiCol_DragDropTarget]       = {0.11f, 0.49f, 0.79f, 0.90f};

    c[ImGuiCol_NavHighlight]         = {0.11f, 0.49f, 0.79f, 1.f};
    c[ImGuiCol_NavWindowingHighlight]= {0.11f, 0.49f, 0.79f, 0.70f};
    c[ImGuiCol_NavWindowingDimBg]    = {0.50f, 0.56f, 0.65f, 0.20f};
    c[ImGuiCol_ModalWindowDimBg]     = {0.50f, 0.56f, 0.65f, 0.25f};
}

inline ImVec4 col_accent()   { return {0.14f, 0.42f, 0.72f, 1.f}; }
inline ImVec4 col_success()  { return {0.14f, 0.54f, 0.31f, 1.f}; }
inline ImVec4 col_warning()  { return {0.78f, 0.53f, 0.12f, 1.f}; }
inline ImVec4 col_error()    { return {0.73f, 0.27f, 0.25f, 1.f}; }
inline ImVec4 col_disabled() { return {0.48f, 0.54f, 0.63f, 1.f}; }

} // namespace tpms::ui
