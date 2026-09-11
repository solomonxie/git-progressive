#include "planner/outline.hpp"

#include <sstream>

namespace gitprogressive {

std::string Outline::toMarkdown() const {
    std::ostringstream os;
    os << "# Outline\n\n";
    if (items.empty()) {
        os << "(empty)\n";
        return os.str();
    }
    for (const auto& item : items) {
        os << "- **" << item.id << "** " << item.title << " (";
        for (size_t i = 0; i < item.hunkIds.size(); ++i) {
            if (i) os << ", ";
            os << item.hunkIds[i];
        }
        os << ")\n";
    }
    return os.str();
}

} // namespace gitprogressive
