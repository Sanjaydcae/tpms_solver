#pragma once

#include <string>
#include "../state/project_state.hpp"

namespace tpms::io {

struct IoResult {
    bool        ok      = false;
    std::string message;
};

// Save all serialisable ProjectState fields to a JSON file.
IoResult save_project(const ProjectState& state, const std::string& path);

// Load a previously saved JSON project file into state.
// Only geometry/mesh/BC/solver parameters are restored;
// computed data (field, meshes, preprocessed model) is NOT restored —
// the user must regenerate after loading.
IoResult load_project(ProjectState& state, const std::string& path);

} // namespace tpms::io
