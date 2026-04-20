#pragma once

#include <array>
#include <cstdio>
#include <string>

// Native OS file dialogs via zenity (GNOME) or kdialog (KDE).
// Falls back to an empty string if neither is available.
// All functions block until the dialog is closed.

namespace tpms::io {

namespace detail {

inline std::string run_cmd(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    std::array<char, 4096> buf{};
    std::string result;
    while (fgets(buf.data(), (int)buf.size(), pipe))
        result += buf.data();
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

// Returns "zenity", "kdialog", or "" if neither is found.
inline std::string detect_dialog_backend() {
    if (system("which zenity > /dev/null 2>&1") == 0)  return "zenity";
    if (system("which kdialog > /dev/null 2>&1") == 0) return "kdialog";
    return {};
}

} // namespace detail

// Open a file-open dialog. Returns the chosen path, or "" on cancel.
// filters: semicolon-separated glob patterns, e.g. "*.json;*.tpms"
inline std::string open_dialog(
    const std::string& title   = "Open File",
    const std::string& filters = "*"
) {
    const auto backend = detail::detect_dialog_backend();
    if (backend == "zenity") {
        std::string cmd = "zenity --file-selection"
                          " --title=\"" + title + "\"";
        // zenity filter: space-separated globs
        std::string f = filters;
        for (auto& c : f) if (c == ';') c = ' ';
        if (f != "*") cmd += " --file-filter=\"" + f + "\"";
        return detail::run_cmd(cmd);
    }
    if (backend == "kdialog") {
        std::string cmd = "kdialog --getopenfilename . \"" + filters + "\" --title \"" + title + "\"";
        return detail::run_cmd(cmd);
    }
    return {};
}

// Open a file-save dialog. Returns the chosen path, or "" on cancel.
inline std::string save_dialog(
    const std::string& title        = "Save File",
    const std::string& default_name = "output",
    const std::string& filters      = "*"
) {
    const auto backend = detail::detect_dialog_backend();
    if (backend == "zenity") {
        std::string cmd = "zenity --file-selection --save --confirm-overwrite"
                          " --title=\"" + title + "\""
                          " --filename=\"" + default_name + "\"";
        std::string f = filters;
        for (auto& c : f) if (c == ';') c = ' ';
        if (f != "*") cmd += " --file-filter=\"" + f + "\"";
        return detail::run_cmd(cmd);
    }
    if (backend == "kdialog") {
        std::string cmd = "kdialog --getsavefilename \"" + default_name + "\" \"" +
                          filters + "\" --title \"" + title + "\"";
        return detail::run_cmd(cmd);
    }
    return {};
}

// Open a directory chooser. Returns the chosen directory, or "".
inline std::string dir_dialog(const std::string& title = "Select Folder") {
    const auto backend = detail::detect_dialog_backend();
    if (backend == "zenity") {
        return detail::run_cmd("zenity --file-selection --directory --title=\"" + title + "\"");
    }
    if (backend == "kdialog") {
        return detail::run_cmd("kdialog --getexistingdirectory . --title \"" + title + "\"");
    }
    return {};
}

} // namespace tpms::io
