#include "planner/tools.hpp"

#include <nlohmann/json.hpp>

#include <unordered_map>

#include "planner/validate.hpp"

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

        std::vector<std::string> errors = validatePlan(hunks_, plan);
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

// Outline pass (T6b.2): the model either appends this hunk to an
// existing item (by id) or creates a new one with a short title. State
// lives in the shared `Outline`, mutated in place.
class ClassifyHunkTool : public Tool {
public:
    ClassifyHunkTool(Outline& outline, const Hunk& hunk) : outline_(outline), hunk_(hunk) {}

    ToolSpec spec() const override {
        return {"classify_hunk",
                "Decide where this hunk belongs in the outline: append it to an existing item "
                "by item_id, or create a new one with a short category title (e.g. 'skeleton "
                "building', 'feature: add search endpoint', 'infra: switch to postgres').",
                R"({"type":"object","properties":{)"
                R"("action":{"type":"string","enum":["append","new"]},)"
                R"("item_id":{"type":"string"},)"
                R"("title":{"type":"string"}},"required":["action"]})"};
    }

    std::string invoke(const std::string& argumentsJson) override {
        json args;
        try {
            args = json::parse(argumentsJson);
        } catch (const json::exception& e) {
            return json{{"ok", false}, {"error", e.what()}}.dump();
        }
        std::string action = args.value("action", "");
        if (action == "append") {
            std::string itemId = args.value("item_id", "");
            for (auto& item : outline_.items) {
                if (item.id == itemId) {
                    item.hunkIds.push_back(hunk_.id);
                    return json{{"ok", true}}.dump();
                }
            }
            json existingIds = json::array();
            for (const auto& item : outline_.items) existingIds.push_back(item.id);
            return json{{"ok", false}, {"error", "unknown item_id: " + itemId}, {"existing_item_ids", existingIds}}
                .dump();
        }
        if (action == "new") {
            std::string title = args.value("title", "");
            if (title.empty()) {
                return json{{"ok", false}, {"error", "a new item requires a non-empty title"}}.dump();
            }
            OutlineItem item;
            item.id = "item-" + std::to_string(outline_.items.size() + 1);
            item.title = title;
            item.hunkIds = {hunk_.id};
            outline_.items.push_back(std::move(item));
            return json{{"ok", true}, {"item_id", outline_.items.back().id}}.dump();
        }
        return json{{"ok", false}, {"error", "action must be 'append' or 'new'"}}.dump();
    }

private:
    Outline& outline_;
    const Hunk& hunk_;
};

// Grouping pass (T6b.3/T6b.4): submit_plan over outline item_ids. Every
// item's hunk ids are expanded into the phase before validating, so
// hunk-level validatePlan (coverage + dependency order) doubles as
// item-level coverage validation too — an unreferenced or
// doubly-referenced item shows up as its hunks being unassigned/
// duplicated.
class SubmitOutlinePlanTool : public Tool {
public:
    SubmitOutlinePlanTool(const std::vector<Hunk>& hunks, const Outline& outline, std::shared_ptr<Plan> outPlan,
                           Audit& audit)
        : hunks_(hunks), outline_(outline), outPlan_(std::move(outPlan)), audit_(audit) {}

    ToolSpec spec() const override {
        return {"submit_plan",
                "Submit the final progressive plan: an ordered list of phases, each with a "
                "title, a one-paragraph rationale, and the outline item_ids it carries. Every "
                "outline item must appear in exactly one phase. Validation errors come back as "
                "the tool result — fix and resubmit.",
                R"({"type":"object","properties":{"phases":{"type":"array","items":{)"
                R"("type":"object","properties":{"title":{"type":"string"},)"
                R"("rationale":{"type":"string"},)"
                R"("item_ids":{"type":"array","items":{"type":"string"}}},)"
                R"("required":["title","rationale","item_ids"]}}},"required":["phases"]})"};
    }

    std::string invoke(const std::string& argumentsJson) override {
        struct RawPhase {
            std::string title;
            std::string rationale;
            std::vector<std::string> itemIds;
        };
        std::vector<RawPhase> raw;
        try {
            json args = json::parse(argumentsJson);
            for (const auto& p : args.at("phases")) {
                raw.push_back({p.at("title").get<std::string>(), p.at("rationale").get<std::string>(),
                                p.at("item_ids").get<std::vector<std::string>>()});
            }
        } catch (const json::exception& e) {
            return json{{"ok", false},
                        {"errors", json::array({std::string("invalid submit_plan arguments: ") + e.what()})}}
                .dump();
        }

        std::unordered_map<std::string, const OutlineItem*> byId;
        for (const auto& item : outline_.items) byId[item.id] = &item;

        Plan plan;
        std::vector<std::string> errors;
        for (const auto& p : raw) {
            Phase phase{p.title, p.rationale, {}};
            for (const auto& itemId : p.itemIds) {
                auto it = byId.find(itemId);
                if (it == byId.end()) {
                    errors.push_back("phase '" + p.title + "' references unknown outline item_id: " + itemId);
                    continue;
                }
                for (const auto& hunkId : it->second->hunkIds) phase.hunkIds.push_back(hunkId);
            }
            plan.phases.push_back(std::move(phase));
        }
        if (!errors.empty()) {
            return json{{"ok", false}, {"errors", errors}}.dump();
        }

        std::vector<std::string> validationErrors = validatePlan(hunks_, plan);
        if (!validationErrors.empty()) {
            return json{{"ok", false}, {"errors", validationErrors}}.dump();
        }

        *outPlan_ = std::move(plan);
        audit_.writePlan(renderPlanMarkdown(*outPlan_));
        return json{{"ok", true}}.dump();
    }

private:
    const std::vector<Hunk>& hunks_;
    const Outline& outline_;
    std::shared_ptr<Plan> outPlan_;
    Audit& audit_;
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

std::vector<std::unique_ptr<Tool>> buildOutlineTools(Outline& outline, const Hunk& hunk, const Repository& repo,
                                                      const Repository::Range& range) {
    std::vector<std::unique_ptr<Tool>> tools;
    tools.push_back(std::make_unique<ClassifyHunkTool>(outline, hunk));
    tools.push_back(std::make_unique<ReadFileTool>(repo, range));
    tools.push_back(std::make_unique<ReadCommitMessagesTool>(repo, range));
    return tools;
}

std::vector<std::unique_ptr<Tool>> buildGroupingTools(const std::vector<Hunk>& hunks, const Outline& outline,
                                                       std::shared_ptr<Plan> outPlan, Audit& audit) {
    std::vector<std::unique_ptr<Tool>> tools;
    tools.push_back(std::make_unique<SubmitOutlinePlanTool>(hunks, outline, std::move(outPlan), audit));
    return tools;
}

} // namespace gitprogressive
