#pragma once

#include <string>

namespace gitprogressive {

// Named LlmBackend, not Provider, to avoid colliding with llm::Provider
// (the client interface). OpenAI/Claude read their API key from
// OPENAI_API_KEY/ANTHROPIC_API_KEY; Ollama needs none (local server).
enum class LlmBackend { Ollama, OpenAI, Claude };

struct Args {
    std::string range;
    std::string branchName = "progressive";
    LlmBackend provider = LlmBackend::Ollama;
    // Empty means "use the provider's default" — resolved in main() once
    // the provider is known, since each backend's model names differ.
    std::string model;
    std::string ollamaHost = "http://localhost:11434";
    bool dryRun = false;
    // Directory for the outline/plan/log audit files, reused (and
    // truncated) across runs. Empty means the default (/tmp/git-progressive).
    std::string auditDir;
    bool valid = false;
};

Args parseArgs(int argc, char** argv);

} // namespace gitprogressive
