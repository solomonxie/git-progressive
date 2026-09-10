#include "agent/loop.hpp"

namespace gitprogressive {

AgentLoop::AgentLoop(Provider& provider, std::string systemPrompt, std::vector<std::unique_ptr<Tool>> tools)
    : provider_(provider), systemPrompt_(std::move(systemPrompt)), tools_(std::move(tools)) {}

// TODO(T5.2): build the message list, call provider_.complete(), dispatch
// tool_calls to matching tools_ entries by name, append ToolResults,
// repeat until terminalTool is called or maxIterations is hit.
std::string AgentLoop::run(const std::string& userPrompt, const std::string& terminalTool, int maxIterations) {
    (void)userPrompt;
    (void)terminalTool;
    (void)maxIterations;
    return "";
}

} // namespace gitprogressive
