#include "llm/openai.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
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

json buildMessages(const std::vector<Message>& messages) {
    json out = json::array();
    for (const auto& msg : messages) {
        if (!msg.toolResults.empty()) {
            // OpenAI expects one "tool" message per result, not a batch.
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
                    {"function", {{"name", call.name}, {"arguments", call.argumentsJson}}},
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

OpenAiProvider::OpenAiProvider(std::string model, std::string host)
    : model_(std::move(model)), host_(std::move(host)) {
    const char* key = std::getenv("OPENAI_API_KEY");
    if (!key || !*key) {
        throw std::runtime_error("openai: OPENAI_API_KEY environment variable is not set");
    }
    apiKey_ = key;
}

CompletionResponse OpenAiProvider::complete(const CompletionRequest& request) {
    json body;
    body["model"] = model_;
    body["messages"] = buildMessages(request.messages);
    if (!request.tools.empty()) {
        body["tools"] = buildTools(request.tools);
    }

    httplib::Client client(host_);
    client.set_read_timeout(300, 0);
    httplib::Headers headers = {{"Authorization", "Bearer " + apiKey_}};
    auto res = client.Post("/v1/chat/completions", headers, body.dump(), "application/json");
    if (!res) {
        throw std::runtime_error("openai: request to " + host_ + " failed: " +
                                  httplib::to_string(res.error()));
    }
    if (res->status != 200) {
        throw std::runtime_error("openai: HTTP " + std::to_string(res->status) + ": " + res->body);
    }

    json parsed = json::parse(res->body);
    const json& message = parsed.at("choices").at(0).at("message");

    CompletionResponse response;
    response.text = message.value("content", "");
    if (message.contains("tool_calls") && !message.at("tool_calls").is_null()) {
        for (const auto& call : message.at("tool_calls")) {
            ToolCall toolCall;
            toolCall.id = call.value("id", "");
            toolCall.name = call.at("function").value("name", "");
            toolCall.argumentsJson = call.at("function").value("arguments", "{}");
            response.toolCalls.push_back(std::move(toolCall));
        }
    }
    return response;
}

} // namespace gitprogressive
