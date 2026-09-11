#pragma once

#include <string>
#include <vector>

#include "diff/hunk.hpp"

namespace gitprogressive {

// Parses `git diff` output into addressable hunks (T3.1).
std::vector<Hunk> parseDiff(const std::string& unifiedDiff);

// Flags same-file hunk dependencies (T3.2): each hunk depends on the
// previous hunk in the same file, so a later phase can never schedule an
// earlier same-file hunk after a later one.
void detectDependencies(std::vector<Hunk>& hunks);

} // namespace gitprogressive
