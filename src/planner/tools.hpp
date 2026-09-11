#pragma once

#include <memory>
#include <vector>

#include "agent/tool.hpp"
#include "diff/hunk.hpp"
#include "git/repo.hpp"
#include "planner/planner.hpp"

namespace gitprogressive {

// Builds the planner's tool set (T6.2, T6.3): read-side tools over
// `hunks`/`repo`, plus submit_plan. On a valid submit_plan call, the
// validated Plan is written into `outPlan` for the caller to read once
// the agent loop terminates (the loop itself only returns the name of
// the terminal tool, not tool-owned state — see agent/loop.hpp).
std::vector<std::unique_ptr<Tool>> buildPlannerTools(const std::vector<Hunk>& hunks,
                                                      const Repository& repo,
                                                      const Repository::Range& range,
                                                      std::shared_ptr<Plan> outPlan);

} // namespace gitprogressive
