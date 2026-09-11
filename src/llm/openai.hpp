#pragma once

#include <string>

#include "llm/provider.hpp"

namespace gitprogressive {

// Provider backed by OpenAI's /v1/chat/completions tool-calling API.
// API key read from the OPENAI_API_KEY env var.
class OpenAiProvider : public Provider {
public:
    explicit OpenAiProvider(std::string model, std::string host = "https://api.openai.com");

    CompletionResponse complete(const CompletionRequest& request) override;

private:
    std::string model_;
    std::string host_;
    std::string apiKey_;
};

} // namespace gitprogressive
