#pragma once

#include <string>

#include "git/repo.hpp"
#include "planner/planner.hpp"

namespace gitprogressive {

// Applies a validated plan's phases as sequential commits (T6.1, T6.2).
void buildCommits(const Plan& plan, const Repository& repo, const std::string& branch);

} // namespace gitprogressive
