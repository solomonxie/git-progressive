#include "planner/tools.hpp"

#include <nlohmann/json.hpp>

#include <unordered_map>

namespace gitprogressive {

using json = nlohmann::json;

namespace {

class ListHunksTool : public Tool {
public:
    explicit ListHunksTool(const std::vector<Hunk>& hunks) : hunks_(hunks) {}

    ToolSpec spec() const override {
        return {"list_hunks",
                "List every hunk in the diff with id, file, line ranges, and which other "
                "hunk ids it depends on (must land in the same or an earlier phase). Does "
                "not include hunk text — use read_hunk for that.",
                R"({"type":"object","properties":{}})"};
    }

    std::string invoke(const std::string&) override {
        json out = json::array();
        for (const auto& hunk : hunks_) {
            out.push_back({
                {"id", hunk.id},
                {"file", hunk.file},
                {"old_start", hunk.oldStart},
                {"old_lines", hunk.oldLines},
                {"new_start", hunk.newStart},
                {"new_lines", hunk.newLines},
                {"depends_on", hunk.dependsOn},
            });
        }
        return out.dump();
    }

private:
    const std::vector<Hunk>& hunks_;
};

class ReadHunkTool : public Tool {
public:
    explicit ReadHunkTool(const std::vector<Hunk>& hunks) : hunks_(hunks) {}

    ToolSpec spec() const override {
        return {"read_hunk", "Read the full diff text of one hunk by id.",
                R"({"type":"object","properties":{"hunk_id":{"type":"string"}},"required":["hunk_id"]})"};
    }

    std::string invoke(const std::string& argumentsJson) override {
        std::string hunkId;
        try {
            hunkId = json::parse(argumentsJson).at("hunk_id").get<std::string>();
        } catch (const json::exception& e) {
            return json{{"ok", false}, {"error", e.what()}}.dump();
        }
        for (const auto& hunk : hunks_) {
            if (hunk.id == hunkId) {
                return json{{"id", hunk.id}, {"file", hunk.file}, {"text", hunk.text}}.dump();
            }
        }
        return json{{"ok", false}, {"error", "unknown hunk_id: " + hunkId}}.dump();
    }

private:
    const std::vector<Hunk>& hunks_;
};

class ReadFileTool : public Tool {
public:
    ReadFileTool(const Repository& repo, Repository::Range range) : repo_(repo), range_(std::move(range)) {}

    ToolSpec spec() const override {
        return {"read_file",
                "Read a file's full content at the base (pre-change) or head (post-change) "
                "commit, for context beyond a single hunk's diff.",
                R"({"type":"object","properties":{"path":{"type":"string"},)"
                R"("ref":{"type":"string","enum":["base","head"]}},"required":["path","ref"]})"};
    }

    std::string invoke(const std::string& argumentsJson) override {
        try {
            json args = json::parse(argumentsJson);
            std::string path = args.at("path").get<std::string>();
            std::string ref = args.value("ref", "head");
            std::string commit = ref == "base" ? range_.base : range_.head;
            return json{{"content", repo_.showFile(commit, path)}}.dump();
        } catch (const std::exception& e) {
            return json{{"ok", false}, {"error", e.what()}}.dump();
        }
    }

private:
    const Repository& repo_;
    Repository::Range range_;
};

class ReadCommitMessagesTool : public Tool {
public:
    ReadCommitMessagesTool(const Repository& repo, Repository::Range range)
        : repo_(repo), range_(std::move(range)) {}

    ToolSpec spec() const override {
        return {"read_commit_messages",
                "List the original commits in the range with hash, subject, and body — "
                "useful context for why a change was made.",
                R"({"type":"object","properties":{}})"};
    }

    std::string invoke(const std::string&) override {
        try {
            json out = json::array();
            for (const auto& commit : repo_.commitMessages(range_)) {
                out.push_back({{"hash", commit.hash}, {"subject", commit.subject}, {"body", commit.body}});
            }
            return out.dump();
        } catch (const std::exception& e) {
            return json{{"ok", false}, {"error", e.what()}}.dump();
        }
    }

private:
    const Repository& repo_;
    Repository::Range range_;
};

// T6.3: full coverage (every hunk assigned exactly once) + dependency
// order (a hunk's dependsOn ids must land in the same or an earlier
// phase). Returns a list of human-readable errors; empty means valid.
std::vector<std::string> validate(const std::vector<Hunk>& hunks, const Plan& plan) {
    std::vector<std::string> errors;
    std::unordered_map<std::string, const Hunk*> byId;
    for (const auto& hunk : hunks) byId[hunk.id] = &hunk;

    std::unordered_map<std::string, int> phaseOf;
    for (size_t p = 0; p < plan.phases.size(); ++p) {
        for (const auto& id : plan.phases[p].hunkIds) {
            if (!byId.count(id)) {
                errors.push_back("phase '" + plan.phases[p].title + "' references unknown hunk id: " + id);
                continue;
            }
            if (phaseOf.count(id)) {
                errors.push_back("hunk " + id + " assigned to more than one phase");
                continue;
            }
            phaseOf[id] = static_cast<int>(p);
        }
    }
    for (const auto& hunk : hunks) {
        if (!phaseOf.count(hunk.id)) {
            errors.push_back("hunk " + hunk.id + " not assigned to any phase");
        }
    }
    for (const auto& hunk : hunks) {
        auto it = phaseOf.find(hunk.id);
        if (it == phaseOf.end()) continue;
        for (const auto& depId : hunk.dependsOn) {
            auto depIt = phaseOf.find(depId);
            if (depIt == phaseOf.end()) continue; // already reported as missing above
            if (depIt->second > it->second) {
                errors.push_back("hunk " + hunk.id + " depends on " + depId +
                                  " but is scheduled in an earlier phase (" + std::to_string(it->second) +
                                  " < " + std::to_string(depIt->second) + ")");
            }
        }
    }
    return errors;
}

class SubmitPlanTool : public Tool {
public:
    SubmitPlanTool(const std::vector<Hunk>& hunks, std::shared_ptr<Plan> outPlan)
        : hunks_(hunks), outPlan_(std::move(outPlan)) {}

    ToolSpec spec() const override {
        return {"submit_plan",
                "Submit the final progressive plan: an ordered list of phases, each with a "
                "title, a one-paragraph rationale, and the hunk ids it carries. Every hunk "
                "must appear in exactly one phase. Validation errors come back as the tool "
                "result — fix and resubmit.",
                R"({"type":"object","properties":{"phases":{"type":"array","items":{)"
                R"("type":"object","properties":{"title":{"type":"string"},)"
                R"("rationale":{"type":"string"},)"
                R"("hunk_ids":{"type":"array","items":{"type":"string"}}},)"
                R"("required":["title","rationale","hunk_ids"]}}},"required":["phases"]})"};
    }

    std::string invoke(const std::string& argumentsJson) override {
        Plan plan;
        try {
            json args = json::parse(argumentsJson);
            for (const auto& p : args.at("phases")) {
                Phase phase;
                phase.title = p.at("title").get<std::string>();
                phase.rationale = p.at("rationale").get<std::string>();
                phase.hunkIds = p.at("hunk_ids").get<std::vector<std::string>>();
                plan.phases.push_back(std::move(phase));
            }
        } catch (const json::exception& e) {
            return json{{"ok", false}, {"errors", json::array({std::string("invalid submit_plan arguments: ") + e.what()})}}.dump();
        }

        std::vector<std::string> errors = validate(hunks_, plan);
        if (!errors.empty()) {
            return json{{"ok", false}, {"errors", errors}}.dump();
        }
        *outPlan_ = std::move(plan);
        return json{{"ok", true}}.dump();
    }

private:
    const std::vector<Hunk>& hunks_;
    std::shared_ptr<Plan> outPlan_;
};

} // namespace

std::vector<std::unique_ptr<Tool>> buildPlannerTools(const std::vector<Hunk>& hunks,
                                                      const Repository& repo,
                                                      const Repository::Range& range,
                                                      std::shared_ptr<Plan> outPlan) {
    std::vector<std::unique_ptr<Tool>> tools;
    tools.push_back(std::make_unique<ListHunksTool>(hunks));
    tools.push_back(std::make_unique<ReadHunkTool>(hunks));
    tools.push_back(std::make_unique<ReadFileTool>(repo, range));
    tools.push_back(std::make_unique<ReadCommitMessagesTool>(repo, range));
    tools.push_back(std::make_unique<SubmitPlanTool>(hunks, std::move(outPlan)));
    return tools;
}

} // namespace gitprogressive
