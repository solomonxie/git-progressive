#pragma once

#include <string>

namespace gitprogressive {

// Common interface for LLM backends (T4.1). Implementations: openai.cpp,
// claude.cpp (T4.3, T4.4) — real sibling backends, hence per-provider files.
class Provider {
public:
    virtual ~Provider() = default;

    // Sends a prompt, returns the raw text response (expected: JSON plan).
    virtual std::string complete(const std::string& prompt) = 0;
};

} // namespace gitprogressive
