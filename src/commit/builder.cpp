#include "commit/builder.hpp"

namespace gitprogressive {

// TODO(T7.1, T7.2): apply each phase's hunks via `git apply --cached`,
// commit, stop with a clear error (partial branch left) on failure.
void buildCommits(const Plan& plan, const Repository& repo, const std::string& branch) {
    (void)plan;
    (void)repo;
    (void)branch;
}

} // namespace gitprogressive
