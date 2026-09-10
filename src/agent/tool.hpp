#pragma once

#include <string>

#include "llm/provider.hpp"

namespace gitprogressive {

// A callable the agent loop can offer to the model (T5.1). Concrete
// tools live with the agent that uses them, e.g. src/planner/tools.cpp.
class Tool {
public:
    virtual ~Tool() = default;

    virtual ToolSpec spec() const = 0;
    virtual std::string invoke(const std::string& argumentsJson) = 0;
};

} // namespace gitprogressive
