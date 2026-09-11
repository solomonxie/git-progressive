#include "planner/planner.hpp"

#include "agent/loop.hpp"
#include "planner/tools.hpp"

namespace gitprogressive {

namespace {

const char* kSystemPrompt =
    "You are the planning stage of git-progressive, a tool that turns a flat diff into a "
    "progressive, onion-layer sequence of commits for review.\n\n"
    "You will group and order the diff's hunks into phases: core change first, then side "
    "effects, then additional features — each phase should read like one step of a lesson, "
    "reviewable on its own and building on the phases before it.\n\n"
    "Rules:\n"
    "- Every hunk must end up in exactly one phase.\n"
    "- A hunk that depends on another hunk (same file, earlier in the original diff) must be "
    "in the same phase or a later one, never earlier.\n"
    "- A phase does not need to cover a whole file — a file's hunks can be split across "
    "phases if that tells a clearer story.\n"
    "- You never write or rewrite code. You only decide grouping and order.\n\n"
    "Use list_hunks to see everything, read_hunk/read_file/read_commit_messages to get "
    "context before you decide, then call submit_plan. If submit_plan returns errors, fix "
    "the plan and call it again.";

} // namespace

Plan planPhases(const std::vector<Hunk>& hunks, const Repository& repo, const Repository::Range& range,
                 Provider& provider) {
    auto plan = std::make_shared<Plan>();
    AgentLoop loop(provider, kSystemPrompt, buildPlannerTools(hunks, repo, range, plan));
    loop.run("Plan the progressive commit phases for this diff.", "submit_plan");
    return *plan;
}

} // namespace gitprogressive
