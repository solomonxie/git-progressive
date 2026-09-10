#pragma once

#include <string>

namespace gitprogressive {

// Wraps the `git` binary. No libgit2 dependency for v1 (see design doc).
class Repository {
public:
    // Resolves a branch name or "A..B" range to base/head commits.
    struct Range {
        std::string base;
        std::string head;
    };

    Range resolveRange(const std::string& input) const;   // TODO(T2.1)
    std::string diff(const Range& range) const;            // TODO(T2.2)
    void createBranch(const std::string& name, const std::string& base) const; // TODO(T2.3)
};

} // namespace gitprogressive
