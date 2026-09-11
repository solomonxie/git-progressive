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

// Renders a plan as markdown: one heading per phase (title + rationale)
// and one bullet per hunk with its file:line-range looked up from
// `hunks` — self-describing, same pattern as outline.hpp's renderOutline.
std::string renderPlanMarkdown(const Plan& plan, const std::vector<Hunk>& hunks);

// Reconstructs phases (title/rationale/hunk-id membership) from
// renderPlanMarkdown's output — the commit builder is handed the plan
// read back off plan.md, not the in-memory Plan the grouping pass built.
Plan parsePlanMarkdown(const std::string& markdown);

} // namespace gitprogressive
