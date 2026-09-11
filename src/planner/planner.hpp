#pragma once

#include <string>
#include <vector>

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

// Runs the progressive-planner agent (T6.4): an AgentLoop with read-side
// tools over `hunks` and `repo`/`range` (list/read hunk, read file, read
// commit messages; T6.2) and a submit_plan tool that validates coverage +
// dependency order locally (T6.3). Returns the validated, self-corrected
// plan, or an empty Plan if the loop hit its iteration cap without one.
Plan planPhases(const std::vector<Hunk>& hunks, const Repository& repo, const Repository::Range& range,
                 Provider& provider);

} // namespace gitprogressive
