#pragma once

#include <string>

namespace gitprogressive {

// Git's well-known empty-tree object hash — constant across every repo,
// so diffing against it needs no history walk at all. Used as the base
// for whole-codebase mode (T2.1): "point it at the default branch" diffs
// this against HEAD instead of resolving a real root/merge-base commit.
inline constexpr const char* kEmptyTreeHash = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";

// Wraps the `git` binary. No libgit2 dependency for v1 (see design doc).
class Repository {
public:
    // Resolves a branch name or "A..B" range to base/head commits.
    struct Range {
        std::string base;
        std::string head;
    };

    Range resolveRange(const std::string& input) const; // T2.1
    std::string diff(const Range& range) const;          // T2.2

    // Creates `name` rooted at `base`, or — when `base` is
    // kEmptyTreeHash — an orphan branch with an empty index (no real
    // commit to check out from; whole-codebase mode starts from
    // nothing, not from git history).
    void createBranch(const std::string& name, const std::string& base) const; // T2.3

    // Reads `path` as it existed at `ref` (planner's read_file tool).
    std::string showFile(const std::string& ref, const std::string& path) const;

    // Applies a unified-diff patch to the index and commits it on the
    // current branch (commit builder, T7.1).
    void applyAndCommit(const std::string& patchText, const std::string& message) const;
};

} // namespace gitprogressive
