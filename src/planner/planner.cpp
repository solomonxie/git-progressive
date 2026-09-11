#include "planner/planner.hpp"

#include <fstream>
#include <sstream>
#include <unordered_map>

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

// Empty string if the file doesn't exist yet (e.g. the grouping pass
// never produced a valid plan) — callers treat that the same as "no
// result" rather than crashing on a missing audit file.
std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string outlinePrompt(const Outline& outline, const std::vector<Hunk>& hunks, const Hunk& hunk) {
    std::ostringstream os;
    os << "Outline so far:\n\n"
       << renderOutline(outline, hunks) << "\nHunk to classify — file: " << hunk.file << "\n```diff\n" << hunk.text
       << "```\n"
       << "Call classify_hunk to place this hunk.";
    return os.str();
}

std::string groupingPrompt(const Outline& outline, const std::vector<Hunk>& hunks) {
    std::ostringstream os;
    os << "Group this outline into progressive phases:\n\n" << renderOutline(outline, hunks);
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
        std::string result = loop.run(outlinePrompt(outline, hunks, hunk), "classify_hunk", 5);

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
        audit.writeOutline(renderOutline(outline, hunks));
    }
    return outline;
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // namespace

std::string renderPlanMarkdown(const Plan& plan, const std::vector<Hunk>& hunks) {
    std::unordered_map<std::string, const Hunk*> byId;
    for (const auto& hunk : hunks) byId[hunk.id] = &hunk;

    std::ostringstream os;
    os << "# Progressive plan\n\n";
    for (size_t i = 0; i < plan.phases.size(); ++i) {
        const auto& phase = plan.phases[i];
        os << "## " << (i + 1) << ". " << phase.title << "\n\n" << phase.rationale << "\n\n";
        for (const auto& id : phase.hunkIds) {
            auto it = byId.find(id);
            os << "- " << id << " — ";
            if (it != byId.end()) {
                os << it->second->file << ":" << it->second->newStart << "-"
                   << (it->second->newStart + it->second->newLines - 1);
            } else {
                os << "(unknown hunk)";
            }
            os << "\n";
        }
        os << "\n";
    }
    return os.str();
}

Plan parsePlanMarkdown(const std::string& markdown) {
    Plan plan;
    std::istringstream stream(markdown);
    std::string line;
    Phase* current = nullptr;
    bool inRationale = false;

    while (std::getline(stream, line)) {
        if (line.rfind("## ", 0) == 0) {
            std::string rest = line.substr(3);
            size_t sep = rest.find(". ");
            Phase phase;
            phase.title = trim(sep == std::string::npos ? rest : rest.substr(sep + 2));
            plan.phases.push_back(std::move(phase));
            current = &plan.phases.back();
            inRationale = true;
            continue;
        }
        if (!current) continue;
        if (line.rfind("- ", 0) == 0) {
            inRationale = false;
            std::string rest = line.substr(2);
            size_t space = rest.find(' ');
            std::string hunkId = trim(space == std::string::npos ? rest : rest.substr(0, space));
            if (!hunkId.empty()) current->hunkIds.push_back(hunkId);
            continue;
        }
        std::string trimmed = trim(line);
        if (trimmed.empty() || !inRationale) continue;
        if (!current->rationale.empty()) current->rationale += " ";
        current->rationale += trimmed;
    }
    return plan;
}

Plan planPhases(const std::vector<Hunk>& hunks, const Repository& repo, const Repository::Range& range,
                 Provider& provider, Audit& audit) {
    audit.log("outline pass: classifying " + std::to_string(hunks.size()) + " hunks...");
    Outline outline = buildOutline(hunks, repo, range, provider, audit);
    audit.writeOutline(renderOutline(outline, hunks));

    // Round-trip through disk: the grouping pass reads outline.md back
    // off disk rather than reusing the in-memory Outline built above, so
    // the file is the actual hand-off contract between passes, not just
    // an audit snapshot of it.
    Outline outlineFromDisk = parseOutline(readFile(audit.paths().outlinePath));
    audit.log("outline pass done: " + std::to_string(outlineFromDisk.items.size()) + " items (re-read from " +
               audit.paths().outlinePath + "). grouping pass: ordering into phases...");

    auto plan = std::make_shared<Plan>();
    auto tools = buildGroupingTools(hunks, outlineFromDisk, plan, audit);
    AgentLogFn logger = [&audit](const std::string& line) { audit.log("[group] " + line); };
    AgentLoop loop(provider, kGroupingSystemPrompt, std::move(tools), logger);
    loop.run(groupingPrompt(outlineFromDisk, hunks), "submit_plan", 10);

    // Same round-trip for the plan: the commit builder gets what's on
    // disk in plan.md (written by submit_plan on success), not just
    // what the tool call captured in memory.
    Plan planFromDisk = parsePlanMarkdown(readFile(audit.paths().planPath));
    audit.log("grouping pass done: " + std::to_string(planFromDisk.phases.size()) + " phases (re-read from " +
               audit.paths().planPath + ")");
    return planFromDisk;
}

} // namespace gitprogressive
