#pragma once

#include <string>

#include "diff/hunk.hpp"
#include "git/repo.hpp"
#include "planner/planner.hpp"

namespace gitprogressive {

// Applies a validated plan's phases as sequential commits (T7.1, T7.2).
// Stops with an exception (leaving the partial branch checked out for
// inspection) on the first phase that fails to apply.
void buildCommits(const Plan& plan, const std::vector<Hunk>& hunks, const Repository& repo,
                   const std::string& branch);

} // namespace gitprogressive
