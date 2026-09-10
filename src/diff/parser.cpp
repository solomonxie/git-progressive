#include "diff/parser.hpp"

namespace gitprogressive {

// TODO(T3.1): parse unified diff headers/hunks into Hunk structs.
std::vector<Hunk> parseDiff(const std::string& unifiedDiff) {
    (void)unifiedDiff;
    return {};
}

} // namespace gitprogressive
