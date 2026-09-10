#pragma once

#include <string>
#include <vector>

namespace gitprogressive {

enum class Role { System, User, Assistant, Tool };

struct ToolCall {
    std::string id;
    std::string name;
    std::string argumentsJson;
};

struct ToolResult {
    std::string toolCallId;
    std::string contentJson;
};

struct Message {
    Role role = Role::User;
    std::string text;
    std::vector<ToolCall> toolCalls;
    std::vector<ToolResult> toolResults;
};

struct ToolSpec {
    std::string name;
    std::string description;
    std::string jsonSchema;
};

struct CompletionRequest {
    std::vector<Message> messages;
    std::vector<ToolSpec> tools;
};

struct CompletionResponse {
    std::string text;
    std::vector<ToolCall> toolCalls;
};

// Common interface for LLM backends (T4.1), speaking each API's
// tool-calling protocol. Implementations: openai.cpp, claude.cpp
// (T4.3, T4.4) — real sibling backends, hence per-provider files.
class Provider {
public:
    virtual ~Provider() = default;
    virtual CompletionResponse complete(const CompletionRequest& request) = 0;
};

} // namespace gitprogressive
