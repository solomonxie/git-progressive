#pragma once

#include <string>
#include <vector>

#include "diff/hunk.hpp"
#include "planner/planner.hpp"

namespace gitprogressive {

// Renders the plan as a self-contained, single-file HTML page (B1): a
// clickable list of phases in the sidebar, each showing its rationale
// and diff (colorized by +/- line prefix) in the main pane. No external
// assets — opens straight from disk, no server needed.
std::string renderVisualizationHtml(const Plan& plan, const std::vector<Hunk>& hunks);

// Renders and writes to `outPath`.
void writeVisualization(const Plan& plan, const std::vector<Hunk>& hunks, const std::string& outPath);

} // namespace gitprogressive
