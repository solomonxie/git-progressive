#pragma once

#include <string>
#include <vector>

namespace gitprogressive {

struct Hunk {
    std::string id;
    std::string file;
    // "a/<path>" side and "b/<path>" side from the diff header, or
    // "/dev/null" for a new/deleted file — needed to reconstruct a
    // standalone patch for `git apply` (commit builder, T7.1).
    std::string oldPath;
    std::string newPath;
    int oldStart = 0;
    int oldLines = 0;
    int newStart = 0;
    int newLines = 0;
    // The hunk's own "@@ ... @@" header line plus its body lines,
    // newline-terminated — everything git apply needs after the file
    // header (oldPath/newPath) is prepended.
    std::string text;

    // IDs of hunks (in the same file) this one's context depends on.
    // Populated by dependency detection (T3.2); empty until then.
    std::vector<std::string> dependsOn;
};

} // namespace gitprogressive
