#pragma once

#include <string>

#include "llm/provider.hpp"

namespace gitprogressive {

// Provider backed by Anthropic's /v1/messages tool-use API.
// API key read from the ANTHROPIC_API_KEY env var.
class ClaudeProvider : public Provider {
public:
    explicit ClaudeProvider(std::string model, std::string host = "https://api.anthropic.com");

    CompletionResponse complete(const CompletionRequest& request) override;

private:
    std::string model_;
    std::string host_;
    std::string apiKey_;
};

} // namespace gitprogressive
