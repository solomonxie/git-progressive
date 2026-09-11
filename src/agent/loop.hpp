#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "agent/tool.hpp"
#include "llm/provider.hpp"

namespace gitprogressive {

// Called with one human-readable line per model response / tool call /
// tool result, for auditing how the agent reasoned to its answer (see
// audit/audit.hpp). Optional — a default-constructed AgentLogFn logs
// nothing.
using AgentLogFn = std::function<void(const std::string&)>;

// Generic tool-calling loop (T5.2): send request, dispatch any returned
// tool_calls to registered tools, append results, repeat until a
// terminal tool call (agent-specific, e.g. planner's submit_plan) or
// maxIterations is hit. Not planner-specific — any future agent reuses
// this.
class AgentLoop {
public:
    AgentLoop(Provider& provider, std::string systemPrompt, std::vector<std::unique_ptr<Tool>> tools,
              AgentLogFn logger = nullptr);

    // Returns the name of the terminal tool call that ended the loop,
    // or empty if maxIterations was reached without one.
    std::string run(const std::string& userPrompt, const std::string& terminalTool, int maxIterations = 20);

private:
    void log(const std::string& line) const;

    Provider& provider_;
    std::string systemPrompt_;
    std::vector<std::unique_ptr<Tool>> tools_;
    AgentLogFn logger_;
};

} // namespace gitprogressive
