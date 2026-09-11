#pragma once

#include <string>
#include <vector>

namespace gitprogressive {

struct CommitInfo {
    std::string hash;
    std::string subject;
    std::string body;
};

// Wraps the `git` binary. No libgit2 dependency for v1 (see design doc).
class Repository {
public:
    // Resolves a branch name or "A..B" range to base/head commits.
    struct Range {
        std::string base;
        std::string head;
    };

    Range resolveRange(const std::string& input) const;              // T2.1
    std::string diff(const Range& range) const;                      // T2.2
    std::vector<CommitInfo> commitMessages(const Range& range) const; // T2.2
    void createBranch(const std::string& name, const std::string& base) const; // T2.3

    // Reads `path` as it existed at `ref` (planner's read_file tool).
    std::string showFile(const std::string& ref, const std::string& path) const;

    // Applies a unified-diff patch to the index and commits it on the
    // current branch (commit builder, T7.1).
    void applyAndCommit(const std::string& patchText, const std::string& message) const;
};

} // namespace gitprogressive
