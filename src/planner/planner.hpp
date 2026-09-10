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

// Groups and orders hunks into onion-layer phases via the LLM (T5.1),
// then validates coverage and dependency order (T5.2).
Plan planPhases(const std::vector<Hunk>& hunks, Provider& provider);

} // namespace gitprogressive
