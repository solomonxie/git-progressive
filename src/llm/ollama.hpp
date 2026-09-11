#pragma once

#include <string>

#include "llm/provider.hpp"

namespace gitprogressive {

// Provider backed by a local Ollama server's /api/chat endpoint (tool-
// calling models, e.g. qwen3). No API key: it's a local model.
class OllamaProvider : public Provider {
public:
    explicit OllamaProvider(std::string model, std::string host = "http://localhost:11434");

    CompletionResponse complete(const CompletionRequest& request) override;

private:
    std::string model_;
    std::string host_;
};

} // namespace gitprogressive
