#pragma once

#include <string>

#include "../state/project_state.hpp"

namespace tpms::io {

struct ReportResult {
    bool ok = false;
    std::string message;
};

ReportResult export_markdown_report(
    const ProjectState& state,
    const std::string& path
);

} // namespace tpms::io
