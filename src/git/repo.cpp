#include "git/repo.hpp"

namespace gitprogressive {

Repository::Range Repository::resolveRange(const std::string& input) const {
    (void)input;
    return {};
}

std::string Repository::diff(const Range& range) const {
    (void)range;
    return "";
}

void Repository::createBranch(const std::string& name, const std::string& base) const {
    (void)name;
    (void)base;
}

} // namespace gitprogressive
