#include "commit/builder.hpp"

#include <stdexcept>
#include <unordered_set>

namespace gitprogressive {

namespace {

// Synthesizes a standalone patch for one phase's hunks, in original diff
// order, emitting one file header per contiguous run of same-file hunks.
std::string buildPatch(const std::vector<Hunk>& hunks, const std::vector<std::string>& hunkIds) {
    std::unordered_set<std::string> wanted(hunkIds.begin(), hunkIds.end());
    std::string patch;
    std::string currentFileKey;
    bool haveCurrent = false;

    for (const auto& hunk : hunks) {
        if (!wanted.count(hunk.id)) continue;
        std::string fileKey = hunk.oldPath + "\x1f" + hunk.newPath;
        if (!haveCurrent || fileKey != currentFileKey) {
            std::string aPath = hunk.oldPath == "/dev/null" ? hunk.newPath : hunk.oldPath;
            std::string bPath = hunk.newPath == "/dev/null" ? hunk.oldPath : hunk.newPath;
            patch += "diff --git a/" + aPath + " b/" + bPath + "\n";
            // Without an explicit new/deleted file mode line, git apply
            // path-strips "/dev/null" too (-p1 strips its leading empty
            // component, yielding the bogus path "dev/null").
            if (hunk.oldPath == "/dev/null") {
                patch += "new file mode 100644\n";
            } else if (hunk.newPath == "/dev/null") {
                patch += "deleted file mode 100644\n";
            }
            patch += "--- " + (hunk.oldPath == "/dev/null" ? std::string("/dev/null") : "a/" + hunk.oldPath) + "\n";
            patch += "+++ " + (hunk.newPath == "/dev/null" ? std::string("/dev/null") : "b/" + hunk.newPath) + "\n";
            currentFileKey = fileKey;
            haveCurrent = true;
        }
        patch += hunk.text;
    }
    return patch;
}

} // namespace

void buildCommits(const Plan& plan, const std::vector<Hunk>& hunks, const Repository& repo,
                   const std::string& branch) {
    for (const auto& phase : plan.phases) {
        std::string patch = buildPatch(hunks, phase.hunkIds);
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
