#include "agent/loop.hpp"

#include <nlohmann/json.hpp>

namespace gitprogressive {

using json = nlohmann::json;

namespace {

Tool* findTool(const std::vector<std::unique_ptr<Tool>>& tools, const std::string& name) {
    for (const auto& tool : tools) {
        if (tool->spec().name == name) return tool.get();
    }
    return nullptr;
}

// Convention: a terminal tool's result may carry a top-level "ok" boolean
// (e.g. submit_plan's validation outcome) telling the loop whether to
// stop or let the model self-correct and try again. Absent/unparseable
// means "stop" — most tools aren't terminal and don't use this field.
bool resultIsOk(const std::string& contentJson) {
    try {
        json parsed = json::parse(contentJson);
        if (parsed.is_object() && parsed.contains("ok")) {
            return parsed.at("ok").get<bool>();
        }
    } catch (const json::parse_error&) {
    }
    return true;
}

std::vector<ToolSpec> collectSpecs(const std::vector<std::unique_ptr<Tool>>& tools) {
    std::vector<ToolSpec> specs;
    specs.reserve(tools.size());
    for (const auto& tool : tools) specs.push_back(tool->spec());
    return specs;
}

} // namespace

AgentLoop::AgentLoop(Provider& provider, std::string systemPrompt, std::vector<std::unique_ptr<Tool>> tools,
                     AgentLogFn logger)
    : provider_(provider), systemPrompt_(std::move(systemPrompt)), tools_(std::move(tools)), logger_(std::move(logger)) {}

void AgentLoop::log(const std::string& line) const {
    if (logger_) logger_(line);
}

std::string AgentLoop::run(const std::string& userPrompt, const std::string& terminalTool, int maxIterations) {
    std::vector<Message> messages;
    messages.push_back({Role::System, systemPrompt_, {}, {}});
    messages.push_back({Role::User, userPrompt, {}, {}});

    const std::vector<ToolSpec> tools = collectSpecs(tools_);

    for (int iteration = 0; iteration < maxIterations; ++iteration) {
        CompletionResponse response = provider_.complete({messages, tools});

        Message assistantMsg{Role::Assistant, response.text, response.toolCalls, {}};
        messages.push_back(assistantMsg);

        if (!response.text.empty()) log("model: " + response.text);

        if (response.toolCalls.empty()) {
            log("model called no tool; prompting it to continue");
            messages.push_back({Role::User,
                                 "Continue by calling one of the available tools, and finish by calling '" +
                                     terminalTool + "' once you have a complete plan.",
                                 {}, {}});
            continue;
        }

        Message resultsMsg{Role::Tool, "", {}, {}};
        bool terminalCalled = false;
        bool terminalOk = false;
        for (const auto& call : response.toolCalls) {
            log("tool_call: " + call.name + "(" + call.argumentsJson + ")");
            Tool* tool = findTool(tools_, call.name);
            std::string result = tool ? tool->invoke(call.argumentsJson)
                                       : R"({"ok":false,"error":"unknown tool"})";
            log("tool_result: " + call.name + " -> " + result);
            resultsMsg.toolResults.push_back({call.id, result});
            if (call.name == terminalTool) {
                terminalCalled = true;
                terminalOk = resultIsOk(result);
            }
        }
        messages.push_back(resultsMsg);

        if (terminalCalled && terminalOk) return terminalTool;
    }
    log("agent loop hit max iterations (" + std::to_string(maxIterations) + ") without a valid terminal call");
    return "";
}

} // namespace gitprogressive
