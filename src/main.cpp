#include <iostream>

#include "cli/args.hpp"
#include "commit/builder.hpp"
#include "diff/parser.hpp"
#include "git/repo.hpp"
#include "llm/ollama.hpp"
#include "planner/planner.hpp"

// git-progressive: reorders a branch/commit range's diff into a
// progressive, onion-layer commit sequence via a local LLM. See
// docs/design/git-progressive.md and docs/design/git-progressive-plan.md.
int main(int argc, char** argv) {
    using namespace gitprogressive;

    Args args = parseArgs(argc, argv);
    if (!args.valid) {
        std::cerr << "usage: git-progressive <branch-or-range> [--branch-name NAME] "
                     "[--provider ollama] [--model NAME] [--host URL] [--dry-run]\n";
        return 1;
    }
    if (args.provider != LlmBackend::Ollama) {
        std::cerr << "git-progressive: only --provider ollama is implemented so far\n";
        return 1;
    }

    try {
        Repository repo;
        Repository::Range range = repo.resolveRange(args.range);
        std::cerr << "git-progressive: range " << range.base << ".." << range.head << "\n";

        std::vector<Hunk> hunks = parseDiff(repo.diff(range));
        detectDependencies(hunks);
        if (hunks.empty()) {
            std::cerr << "git-progressive: no changes in range, nothing to do\n";
            return 0;
        }
        std::cerr << "git-progressive: " << hunks.size() << " hunks, planning with " << args.model << "...\n";

        OllamaProvider provider(args.model, args.ollamaHost);
        Plan plan = planPhases(hunks, repo, range, provider);
        if (plan.phases.empty()) {
            std::cerr << "git-progressive: planner did not produce a valid plan\n";
            return 1;
        }

        std::cerr << "git-progressive: " << plan.phases.size() << " phases:\n";
        for (size_t i = 0; i < plan.phases.size(); ++i) {
            const auto& phase = plan.phases[i];
            std::cerr << "  " << (i + 1) << ". " << phase.title << " (" << phase.hunkIds.size() << " hunks)\n";
        }

        if (args.dryRun) {
            for (const auto& phase : plan.phases) {
                std::cout << "# " << phase.title << "\n" << phase.rationale << "\n";
                for (const auto& id : phase.hunkIds) std::cout << "  - " << id << "\n";
                std::cout << "\n";
            }
            return 0;
        }

        repo.createBranch(args.branchName, range.base);
        buildCommits(plan, hunks, repo, args.branchName);
        std::cerr << "git-progressive: created branch '" << args.branchName << "' with " << plan.phases.size()
                   << " progressive commits\n";
    } catch (const std::exception& e) {
        std::cerr << "git-progressive: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
