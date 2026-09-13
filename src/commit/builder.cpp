#include "commit/builder.hpp"

#include <stdexcept>

#include "commit/patch.hpp"

namespace gitprogressive {

void buildCommits(const Plan& plan, const std::vector<Hunk>& hunks, const Repository& repo,
                   const std::string& branch) {
    for (const auto& phase : plan.phases) {
        std::string patch = synthesizePatch(hunks, phase.hunkIds);
        if (patch.empty()) continue; // empty phase: nothing to commit

        std::string message = phase.title + "\n\n" + phase.rationale;
        try {
            repo.applyAndCommit(patch, message);
        } catch (const std::exception& e) {
            throw std::runtime_error("git-progressive: phase '" + phase.title + "' failed to apply on branch '" +
                                      branch + "' (partial branch left for inspection):\n" + e.what());
        }
    }
}

} // namespace gitprogressive
