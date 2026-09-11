#pragma once

#include <string>

namespace gitprogressive {

// Ollama (local, no API key) is the only backend implemented so far —
// see docs/design/git-progressive.md. OpenAI/Claude remain the eventual
// goal but aren't wired up yet. Named LlmBackend, not Provider, to avoid
// colliding with llm::Provider (the client interface).
enum class LlmBackend { Ollama, OpenAI, Claude };

struct Args {
    std::string range;
    std::string branchName = "progressive";
    LlmBackend provider = LlmBackend::Ollama;
    std::string model = "qwen3:8b";
    std::string ollamaHost = "http://localhost:11434";
    bool dryRun = false;
    bool valid = false;
};

Args parseArgs(int argc, char** argv);

} // namespace gitprogressive
