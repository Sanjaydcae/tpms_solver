#pragma once

#include <string>
#include <vector>

#include "../state/project_state.hpp"

namespace tpms::validation {

struct HealthCheckReport {
    bool ok = true;
    int passed = 0;
    int warnings = 0;
    int errors = 0;
    std::vector<std::string> lines;
    std::string summary;
};

HealthCheckReport run_model_health_checks(const ProjectState& state);

} // namespace tpms::validation
