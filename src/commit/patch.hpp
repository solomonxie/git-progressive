#pragma once

#include <string>
#include <vector>

#include "diff/hunk.hpp"

namespace gitprogressive {

// Synthesizes a standalone unified-diff patch covering exactly
// `hunkIds`, in original diff order, emitting one file header per
// contiguous run of same-file hunks. Shared by the commit builder
// (applies it via `git apply`) and the visualizer (renders it as-is).
std::string synthesizePatch(const std::vector<Hunk>& hunks, const std::vector<std::string>& hunkIds);

} // namespace gitprogressive
