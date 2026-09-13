#include <iostream>

#include "audit/audit.hpp"
#include "cli/args.hpp"
#include "commit/builder.hpp"
#include "diff/parser.hpp"
#include "git/repo.hpp"
#include "llm/claude.hpp"
#include "llm/ollama.hpp"
#include "llm/openai.hpp"
#include "planner/planner.hpp"
#include "visualize/visualize.hpp"

#include <memory>

// git-progressive: reorders a branch/commit range's diff into a
// progressive, onion-layer commit sequence via a local LLM. See
// docs/design/git-progressive.md and docs/design/git-progressive-plan.md.
int main(int argc, char** argv) {
    using namespace gitprogressive;

    Args args = parseArgs(argc, argv);
    if (!args.valid) {
        std::cerr << "usage: git-progressive <branch-or-range> [--branch-name NAME] "
                     "[--provider ollama|openai|claude] [--model NAME] [--host URL] [--audit-dir DIR] [--dry-run] "
                     "[--visualize]\n";
        return 1;
    }

    Audit audit(resolveAuditPaths(args.auditDir));
    audit.printBanner();

    try {
        std::unique_ptr<Provider> provider;
        switch (args.provider) {
            case LlmBackend::Ollama:
                if (args.model.empty()) args.model = "qwen3:8b";
                provider = std::make_unique<OllamaProvider>(args.model, args.ollamaHost);
                break;
            case LlmBackend::OpenAI:
                if (args.model.empty()) args.model = "gpt-5.1";
                provider = std::make_unique<OpenAiProvider>(args.model);
                break;
            case LlmBackend::Claude:
                if (args.model.empty()) args.model = "claude-sonnet-5";
                provider = std::make_unique<ClaudeProvider>(args.model);
                break;
        }

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

        Plan plan = planPhases(hunks, repo, range, *provider, audit);
        if (plan.phases.empty()) {
            std::cerr << "git-progressive: planner did not produce a valid plan\n";
            return 1;
        }

        std::cerr << "git-progressive: " << plan.phases.size() << " phases:\n";
        for (size_t i = 0; i < plan.phases.size(); ++i) {
            const auto& phase = plan.phases[i];
            std::cerr << "  " << (i + 1) << ". " << phase.title << " (" << phase.hunkIds.size() << " hunks)\n";
        }

        if (args.visualize) {
            std::string vizPath = audit.paths().dir + "/progressive-view.html";
            writeVisualization(plan, hunks, vizPath);
            std::cerr << "git-progressive: visualization written to " << vizPath << "\n";
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
