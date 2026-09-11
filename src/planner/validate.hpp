#pragma once

#include <string>
#include <vector>

#include "diff/hunk.hpp"
#include "planner/planner.hpp"

namespace gitprogressive {

// T6.3 / T6b.4: full coverage (every hunk assigned exactly once) plus
// dependency order (a hunk's dependsOn ids must land in the same or an
// earlier phase). Returns human-readable errors; empty means valid.
// Shared by the legacy single-session submit_plan and the outline-then-
// plan grouping pass's deterministic expansion.
std::vector<std::string> validatePlan(const std::vector<Hunk>& hunks, const Plan& plan);

} // namespace gitprogressive
