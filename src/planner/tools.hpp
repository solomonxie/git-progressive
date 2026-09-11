#pragma once

#include <memory>
#include <vector>

#include "agent/tool.hpp"
#include "audit/audit.hpp"
#include "diff/hunk.hpp"
#include "git/repo.hpp"
#include "planner/outline.hpp"
#include "planner/planner.hpp"

namespace gitprogressive {

// Legacy single-session tool set (T6.2, T6.3): read-side tools over
// `hunks`/`repo`, plus submit_plan over raw hunk ids. Superseded by the
// outline-then-plan pipeline below for the final grouping decision (see
// design doc) but kept working/available. On a valid submit_plan call,
// the validated Plan is written into `outPlan` for the caller to read
// once the agent loop terminates (the loop itself only returns the name
// of the terminal tool, not tool-owned state — see agent/loop.hpp).
std::vector<std::unique_ptr<Tool>> buildPlannerTools(const std::vector<Hunk>& hunks,
                                                      const Repository& repo,
                                                      const Repository::Range& range,
                                                      std::shared_ptr<Plan> outPlan);

// Outline pass tools (T6b.2): classify_hunk (mutates `outline` in place)
// plus read_file/read_commit_messages for extra context. Rebuilt fresh
// for each hunk since `hunk` is the one currently being classified.
std::vector<std::unique_ptr<Tool>> buildOutlineTools(Outline& outline, const Hunk& hunk, const Repository& repo,
                                                      const Repository::Range& range);

// Grouping pass tool (T6b.3): submit_plan over outline item_ids instead
// of hunk ids. Expands item_ids -> hunk_ids via `outline` and validates
// with validatePlan (T6b.4) before accepting; on success also writes the
// plan.md audit snapshot immediately.
std::vector<std::unique_ptr<Tool>> buildGroupingTools(const std::vector<Hunk>& hunks, const Outline& outline,
                                                       std::shared_ptr<Plan> outPlan, Audit& audit);

} // namespace gitprogressive
