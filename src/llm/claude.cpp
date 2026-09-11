#include "llm/claude.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <stdexcept>

namespace gitprogressive {

using json = nlohmann::json;

namespace {

constexpr int kMaxTokens = 8192;
constexpr const char* kAnthropicVersion = "2023-06-01";

json argumentsToJson(const std::string& argumentsJson) {
    if (argumentsJson.empty()) return json::object();
    try {
        return json::parse(argumentsJson);
    } catch (const json::parse_error&) {
        return json::object();
    }
}

// Claude has no top-level "tool" role: tool results are user messages
// carrying tool_result content blocks, and system messages are pulled
// out into the request's separate "system" field.
json buildMessages(const std::vector<Message>& messages, std::string& system) {
    json out = json::array();
    for (const auto& msg : messages) {
        if (msg.role == Role::System) {
            if (!system.empty()) system += "\n";
            system += msg.text;
            continue;
        }
        if (!msg.toolResults.empty()) {
            json content = json::array();
            for (const auto& result : msg.toolResults) {
                content.push_back({
                    {"type", "tool_result"},
                    {"tool_use_id", result.toolCallId},
                    {"content", result.contentJson},
                });
            }
            out.push_back({{"role", "user"}, {"content", content}});
            continue;
        }
        json content = json::array();
        if (!msg.text.empty()) {
            content.push_back({{"type", "text"}, {"text", msg.text}});
        }
        for (const auto& call : msg.toolCalls) {
            content.push_back({
                {"type", "tool_use"},
                {"id", call.id},
                {"name", call.name},
                {"input", argumentsToJson(call.argumentsJson)},
            });
        }
        out.push_back({{"role", msg.role == Role::Assistant ? "assistant" : "user"}, {"content", content}});
    }
    return out;
}

json buildTools(const std::vector<ToolSpec>& tools) {
    json out = json::array();
    for (const auto& tool : tools) {
        json schema;
        try {
            schema = json::parse(tool.jsonSchema);
        } catch (const json::parse_error&) {
            schema = json::object();
        }
        out.push_back({
            {"name", tool.name},
            {"description", tool.description},
            {"input_schema", schema},
        });
    }
    return out;
}

} // namespace

ClaudeProvider::ClaudeProvider(std::string model, std::string host)
    : model_(std::move(model)), host_(std::move(host)) {
    const char* key = std::getenv("ANTHROPIC_API_KEY");
    if (!key || !*key) {
        throw std::runtime_error("claude: ANTHROPIC_API_KEY environment variable is not set");
    }
    apiKey_ = key;
}

CompletionResponse ClaudeProvider::complete(const CompletionRequest& request) {
    std::string system;
    json body;
    body["model"] = model_;
    body["max_tokens"] = kMaxTokens;
    body["messages"] = buildMessages(request.messages, system);
    if (!system.empty()) body["system"] = system;
    if (!request.tools.empty()) {
        body["tools"] = buildTools(request.tools);
    }

    httplib::Client client(host_);
    client.set_read_timeout(300, 0);
    httplib::Headers headers = {
        {"x-api-key", apiKey_},
        {"anthropic-version", kAnthropicVersion},
    };
    auto res = client.Post("/v1/messages", headers, body.dump(), "application/json");
    if (!res) {
        throw std::runtime_error("claude: request to " + host_ + " failed: " +
                                  httplib::to_string(res.error()));
    }
    if (res->status != 200) {
        throw std::runtime_error("claude: HTTP " + std::to_string(res->status) + ": " + res->body);
    }

    json parsed = json::parse(res->body);
    CompletionResponse response;
    int index = 0;
    for (const auto& block : parsed.at("content")) {
        const std::string type = block.value("type", "");
        if (type == "text") {
            response.text += block.value("text", "");
        } else if (type == "tool_use") {
            ToolCall toolCall;
            toolCall.id = block.value("id", "call_" + std::to_string(index++));
            toolCall.name = block.value("name", "");
            toolCall.argumentsJson = block.value("input", json::object()).dump();
            response.toolCalls.push_back(std::move(toolCall));
        }
    }
    return response;
}

} // namespace gitprogressive
