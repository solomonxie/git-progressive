#include "planner/planner.hpp"

#include <sstream>

#include "agent/loop.hpp"
#include "planner/outline.hpp"
#include "planner/tools.hpp"

namespace gitprogressive {

namespace {

const char* kOutlineSystemPrompt =
    "You are the outline-building stage of git-progressive, a tool that turns a flat diff into "
    "a progressive, onion-layer sequence of commits for review.\n\n"
    "You are shown one hunk at a time, in original diff order, along with the outline built so "
    "far. Decide where this hunk belongs: if it's the same concern as an existing outline item, "
    "append it there; otherwise create a new item with a short category-style title, e.g. "
    "'skeleton building', 'feature: add search endpoint', 'infra: switch to postgres', 'robust: "
    "more logging', 'clean: remove dead code'.\n\n"
    "Rules:\n"
    "- Check the existing outline first — reuse a matching item rather than creating a near-"
    "duplicate.\n"
    "- You never write or rewrite code; you only classify.\n"
    "- Call classify_hunk exactly once to finish this hunk. Use read_file/read_commit_messages "
    "first if the hunk's diff alone isn't enough context to classify it.";

const char* kGroupingSystemPrompt =
    "You are the grouping stage of git-progressive. You're given a completed outline — every "
    "hunk in the diff has already been classified into categorized items. Group and order the "
    "outline's items (not raw hunks) into phases: core change first, then side effects, then "
    "additional features — each phase should read like one step of a lesson, reviewable on its "
    "own and building on the phases before it.\n\n"
    "Rules:\n"
    "- Every outline item must end up in exactly one phase.\n"
    "- You never write or rewrite code. You only decide grouping and order.\n\n"
    "Call submit_plan with each phase's title, rationale, and the item_ids it carries. If "
    "submit_plan returns errors, fix the plan and call it again.";

std::string outlinePrompt(const Outline& outline, const Hunk& hunk) {
    std::ostringstream os;
    os << "Outline so far:\n\n"
       << outline.toMarkdown() << "\nHunk to classify — file: " << hunk.file << "\n```diff\n"
       << hunk.text << "```\n"
       << "Call classify_hunk to place this hunk.";
    return os.str();
}

std::string groupingPrompt(const Outline& outline) {
    std::ostringstream os;
    os << "Group this outline into progressive phases:\n\n" << outline.toMarkdown();
    return os.str();
}

// T6b.2: iterate hunks in original diff order, one bounded LLM call per
// hunk (outline size + one hunk, never the whole diff at once).
Outline buildOutline(const std::vector<Hunk>& hunks, const Repository& repo, const Repository::Range& range,
                     Provider& provider, Audit& audit) {
    Outline outline;
    AgentLogFn logger = [&audit](const std::string& line) { audit.log("[outline] " + line); };

    for (size_t i = 0; i < hunks.size(); ++i) {
        const Hunk& hunk = hunks[i];
        audit.log("[outline] hunk " + std::to_string(i + 1) + "/" + std::to_string(hunks.size()) + ": " + hunk.id +
                   " (" + hunk.file + ")");

        auto tools = buildOutlineTools(outline, hunk, repo, range);
        AgentLoop loop(provider, kOutlineSystemPrompt, std::move(tools), logger);
        std::string result = loop.run(outlinePrompt(outline, hunk), "classify_hunk", 5);

        if (result.empty()) {
            // classify_hunk cap hit without a valid call — file it alone
            // rather than silently dropping the hunk from the outline.
            OutlineItem fallback;
            fallback.id = "item-" + std::to_string(outline.items.size() + 1);
            fallback.title = "unclassified: " + hunk.file;
            fallback.hunkIds = {hunk.id};
            outline.items.push_back(std::move(fallback));
            audit.log("[outline] hunk " + hunk.id + " fell back to its own item (classify_hunk cap hit)");
        }
        audit.writeOutline(outline.toMarkdown());
    }
    return outline;
}

} // namespace

std::string renderPlanMarkdown(const Plan& plan) {
    std::ostringstream os;
    os << "# Progressive plan\n\n";
    for (size_t i = 0; i < plan.phases.size(); ++i) {
        const auto& phase = plan.phases[i];
        os << "## " << (i + 1) << ". " << phase.title << "\n\n" << phase.rationale << "\n\n";
        for (const auto& id : phase.hunkIds) os << "- " << id << "\n";
        os << "\n";
    }
    return os.str();
}

Plan planPhases(const std::vector<Hunk>& hunks, const Repository& repo, const Repository::Range& range,
                 Provider& provider, Audit& audit) {
    audit.log("outline pass: classifying " + std::to_string(hunks.size()) + " hunks...");
    Outline outline = buildOutline(hunks, repo, range, provider, audit);
    audit.writeOutline(outline.toMarkdown());
    audit.log("outline pass done: " + std::to_string(outline.items.size()) + " items. grouping pass: ordering into phases...");

    auto plan = std::make_shared<Plan>();
    auto tools = buildGroupingTools(hunks, outline, plan, audit);
    AgentLogFn logger = [&audit](const std::string& line) { audit.log("[group] " + line); };
    AgentLoop loop(provider, kGroupingSystemPrompt, std::move(tools), logger);
    loop.run(groupingPrompt(outline), "submit_plan", 10);

    audit.writePlan(renderPlanMarkdown(*plan));
    return *plan;
}

} // namespace gitprogressive
