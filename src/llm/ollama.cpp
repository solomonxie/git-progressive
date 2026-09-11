#include "llm/ollama.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <stdexcept>

namespace gitprogressive {

using json = nlohmann::json;

namespace {

const char* roleName(Role role) {
    switch (role) {
        case Role::System: return "system";
        case Role::User: return "user";
        case Role::Assistant: return "assistant";
        case Role::Tool: return "tool";
    }
    return "user";
}

json argumentsToJson(const std::string& argumentsJson) {
    if (argumentsJson.empty()) return json::object();
    try {
        return json::parse(argumentsJson);
    } catch (const json::parse_error&) {
        return json::object();
    }
}

std::string argumentsFromJson(const json& arguments) {
    return arguments.is_string() ? arguments.get<std::string>() : arguments.dump();
}

json buildMessages(const std::vector<Message>& messages) {
    json out = json::array();
    for (const auto& msg : messages) {
        if (!msg.toolResults.empty()) {
            // Ollama expects one "tool" message per result, not a batch.
            for (const auto& result : msg.toolResults) {
                out.push_back({
                    {"role", "tool"},
                    {"content", result.contentJson},
                    {"tool_call_id", result.toolCallId},
                });
            }
            continue;
        }
        json m;
        m["role"] = roleName(msg.role);
        m["content"] = msg.text;
        if (!msg.toolCalls.empty()) {
            json calls = json::array();
            for (const auto& call : msg.toolCalls) {
                calls.push_back({
                    {"id", call.id},
                    {"type", "function"},
                    {"function", {{"name", call.name}, {"arguments", argumentsToJson(call.argumentsJson)}}},
                });
            }
            m["tool_calls"] = calls;
        }
        out.push_back(std::move(m));
    }
    return out;
}

json buildTools(const std::vector<ToolSpec>& tools) {
    json out = json::array();
    for (const auto& tool : tools) {
        json parameters;
        try {
            parameters = json::parse(tool.jsonSchema);
        } catch (const json::parse_error&) {
            parameters = json::object();
        }
        out.push_back({
            {"type", "function"},
            {"function", {{"name", tool.name}, {"description", tool.description}, {"parameters", parameters}}},
        });
    }
    return out;
}

} // namespace

OllamaProvider::OllamaProvider(std::string model, std::string host)
    : model_(std::move(model)), host_(std::move(host)) {}

CompletionResponse OllamaProvider::complete(const CompletionRequest& request) {
    json body;
    body["model"] = model_;
    body["stream"] = false;
    body["messages"] = buildMessages(request.messages);
    if (!request.tools.empty()) {
        body["tools"] = buildTools(request.tools);
    }

    httplib::Client client(host_);
    client.set_read_timeout(300, 0);
    auto res = client.Post("/api/chat", body.dump(), "application/json");
    if (!res) {
        throw std::runtime_error("ollama: request to " + host_ + " failed: " +
                                  httplib::to_string(res.error()));
    }
    if (res->status != 200) {
        throw std::runtime_error("ollama: HTTP " + std::to_string(res->status) + ": " + res->body);
    }

    json parsed = json::parse(res->body);
    const json& message = parsed.at("message");

    CompletionResponse response;
    response.text = message.value("content", "");
    if (message.contains("tool_calls")) {
        int index = 0;
        for (const auto& call : message.at("tool_calls")) {
            ToolCall toolCall;
            toolCall.id = call.value("id", "call_" + std::to_string(index++));
            toolCall.name = call.at("function").value("name", "");
            toolCall.argumentsJson = argumentsFromJson(call.at("function").value("arguments", json::object()));
            response.toolCalls.push_back(std::move(toolCall));
        }
    }
    return response;
}

} // namespace gitprogressive
