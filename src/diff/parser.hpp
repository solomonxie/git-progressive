#pragma once

#include <string>
#include <vector>

#include "diff/hunk.hpp"

namespace gitprogressive {

// Parses `git diff` output into addressable hunks (T3.1).
std::vector<Hunk> parseDiff(const std::string& unifiedDiff);

} // namespace gitprogressive
