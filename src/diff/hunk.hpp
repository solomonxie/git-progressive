#pragma once

#include <string>
#include <vector>

namespace gitprogressive {

struct Hunk {
    std::string id;
    std::string file;
    int oldStart = 0;
    int oldLines = 0;
    int newStart = 0;
    int newLines = 0;
    std::string text;

    // IDs of hunks (in the same file) this one's context depends on.
    // Populated by dependency detection (T3.2); empty until then.
    std::vector<std::string> dependsOn;
};

} // namespace gitprogressive
