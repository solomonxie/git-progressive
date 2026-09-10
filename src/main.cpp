#include <iostream>

#include "cli/args.hpp"

// git-progressive: reorders a branch/commit range's diff into a
// progressive, onion-layer commit sequence via an LLM. See
// docs/design/git-progressive.md and docs/design/git-progressive-plan.md.
//
// Skeleton only — full flow lands in T7.2, once Phases 2-6 are built.
int main(int argc, char** argv) {
    using namespace gitprogressive;

    Args args = parseArgs(argc, argv);
    if (!args.valid) {
        std::cerr << "usage: git-progressive <branch-or-range>\n";
        return 1;
    }

    std::cerr << "git-progressive: not yet implemented (range: " << args.range << ")\n";
    return 0;
}
