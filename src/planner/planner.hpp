#pragma once

#include <string>
#include <vector>

#include "diff/hunk.hpp"
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
// tools over `hunks` (list/read hunk, read file, read commit messages;
// T6.2) and a submit_plan tool that validates coverage + dependency
// order locally (T6.3). Returns the validated, self-corrected plan.
Plan planPhases(const std::vector<Hunk>& hunks, Provider& provider);

} // namespace gitprogressive
