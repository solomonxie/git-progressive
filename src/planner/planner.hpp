#pragma once

#include <string>
#include <vector>

#include "audit/audit.hpp"
#include "diff/hunk.hpp"
#include "git/repo.hpp"
#include "llm/provider.hpp"

namespace gitprogressive {

struct Phase {
    std::string title;
    std::string rationale;
    std::vector<std::string> hunkIds;
};

struct Plan {
    std::vector<Phase> phases;
};

// Runs the two-pass outline-then-plan pipeline (T6b, see design doc):
// (1) classify every hunk into a running outline, one bounded LLM call
// per hunk; (2) group/order the completed outline (not raw hunks) into
// phases; (3) deterministically expand outline items back to hunk ids,
// re-validating coverage + dependency order (T6.3/T6b.4). `audit` gets
// the outline/plan snapshots and every agent decision as the run
// progresses. Returns an empty Plan if either pass hit its iteration cap
// without a valid result.
Plan planPhases(const std::vector<Hunk>& hunks, const Repository& repo, const Repository::Range& range,
                 Provider& provider, Audit& audit);

// Renders a plan as markdown (phases in order, title + rationale + hunk
// ids) — used for the plan.md audit snapshot.
std::string renderPlanMarkdown(const Plan& plan);

} // namespace gitprogressive
