#pragma once

#include <imgui.h>
#include "theme.hpp"
#include "../state/project_state.hpp"

namespace tpms::ui {

struct ConsolePanel {
    bool auto_scroll  = true;
    bool show_info    = true;
    bool show_warning = true;
    bool show_error   = true;
    int  active_tab   = 0; // 0=Messages, 1=Progress, 2=Errors

    void draw(const ProjectState& state) {
        if (!ImGui::Begin("Messages / Solver Log / Progress")) { ImGui::End(); return; }

        // Filter toggles
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4, 2});
        ImGui::Checkbox("Info", &show_info);
        ImGui::SameLine();
        ImGui::Checkbox("Warning", &show_warning);
        ImGui::SameLine();
        ImGui::Checkbox("Error", &show_error);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            // Note: state is const here; clearing happens through app
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &auto_scroll);
        ImGui::PopStyleVar();

        ImGui::Separator();

        // Tab bar
        if (ImGui::BeginTabBar("##console_tabs")) {
            if (ImGui::BeginTabItem("Messages")) {
                active_tab = 0;
                draw_messages(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Progress")) {
                active_tab = 1;
                draw_progress(state);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Errors")) {
                active_tab = 2;
                draw_errors(state);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }

private:
    void draw_messages(const ProjectState& state) {
        ImGui::BeginChild("##log_scroll", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);

        for (auto& entry : state.log) {
            if (entry.level == LogEntry::Level::Info    && !show_info)    continue;
            if (entry.level == LogEntry::Level::Warning && !show_warning) continue;
            if (entry.level == LogEntry::Level::Error   && !show_error)   continue;

            ImVec4 col  = {0.20f, 0.28f, 0.37f, 1.f};
            const char* prefix = "  ";
            if (entry.level == LogEntry::Level::Warning) { col = col_warning(); prefix = "! "; }
            if (entry.level == LogEntry::Level::Error)   { col = col_error();   prefix = "x "; }
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(prefix);
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted(entry.text.c_str());
            ImGui::PopStyleColor();
        }

        if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.f);

        ImGui::EndChild();
    }

    void draw_progress(const ProjectState& state) {
        ImGui::TextColored(col_accent(), "Workflow Progress");
        ImGui::Separator();

        auto status_line = [](const char* label, bool ok) {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine(170);
            ImGui::TextColored(ok ? col_success() : ImVec4{0.46f, 0.53f, 0.61f, 1.f},
                               "%s", ok ? "Ready" : "Pending");
        };

        status_line("Geometry", state.has_geometry);
        status_line("Surface Mesh", state.has_surface_mesh);
        status_line("Volume Mesh", state.has_volume_mesh);
        status_line("Material", state.has_material);
        status_line("Boundary Conditions", state.has_fixed_bc && state.has_load_bc);
        status_line("Validation", state.validation_ok);
        status_line("Results", state.has_results);

        if (state.solver_has_run || state.solver_running) {
            ImGui::Spacing();
            ImGui::TextColored(col_accent(), "Solver");
            ImGui::Separator();
            if (state.solver_running) {
                ImGui::ProgressBar(state.solver_progress, ImVec2(-1, 0),
                                   state.solver_status_text.empty() ? "Solving..." : state.solver_status_text.c_str());
            }
            ImGui::Text("State: %s", state.solver_running ? "Running" : "Finished");
            ImGui::Text("Convergence: %s", state.solver_converged ? "Converged" : "Approximate / stopped");
            ImGui::Text("Iteration: %d / %d", state.solver_current_iteration, state.solver_max_iter);
            ImGui::Text("Relative residual: %.3e", state.solver_current_relative_residual);
            ImGui::Text("Final iterations: %d", state.solver_iterations);
            ImGui::Text("Residual: %.3e -> %.3e",
                        state.solver_initial_residual,
                        state.solver_final_residual);
            if (!state.solver_residual_history.empty()) {
                ImGui::PlotLines("Residual History",
                                 state.solver_residual_history.data(),
                                 (int)state.solver_residual_history.size(),
                                 0, nullptr, 0.0f, 1.0f, ImVec2(-1, 72));
            }
        }

        if (!state.fem_summary.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(col_accent(), "FEM Core");
            ImGui::Separator();
            ImGui::PushTextWrapPos();
            ImGui::TextDisabled("%s", state.fem_summary.c_str());
            ImGui::PopTextWrapPos();
        }

        if (state.has_results) {
            ImGui::Spacing();
            ImGui::TextColored(col_accent(), "Active Result");
            ImGui::Separator();
            ImGui::Text("%s [%s]", state.active_result.c_str(), state.result_unit.c_str());
            ImGui::Text("Min %.4g at node %d", state.result_min, state.result_min_node);
            ImGui::Text("Max %.4g at node %d", state.result_max, state.result_max_node);
        }
    }

    void draw_errors(const ProjectState& state) {
        ImGui::BeginChild("##err_scroll");
        for (auto& entry : state.log) {
            if (entry.level != LogEntry::Level::Error) continue;
            ImGui::PushStyleColor(ImGuiCol_Text, col_error());
            ImGui::TextUnformatted(entry.text.c_str());
            ImGui::PopStyleColor();
        }
        if (auto_scroll) ImGui::SetScrollHereY(1.f);
        ImGui::EndChild();
    }
};

} // namespace tpms::ui
