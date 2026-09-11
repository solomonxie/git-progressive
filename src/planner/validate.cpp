#include "planner/validate.hpp"

#include <unordered_map>

namespace gitprogressive {

std::vector<std::string> validatePlan(const std::vector<Hunk>& hunks, const Plan& plan) {
    std::vector<std::string> errors;
    std::unordered_map<std::string, const Hunk*> byId;
    for (const auto& hunk : hunks) byId[hunk.id] = &hunk;

    std::unordered_map<std::string, int> phaseOf;
    for (size_t p = 0; p < plan.phases.size(); ++p) {
        for (const auto& id : plan.phases[p].hunkIds) {
            if (!byId.count(id)) {
                errors.push_back("phase '" + plan.phases[p].title + "' references unknown hunk id: " + id);
                continue;
            }
            if (phaseOf.count(id)) {
                errors.push_back("hunk " + id + " assigned to more than one phase");
                continue;
            }
            phaseOf[id] = static_cast<int>(p);
        }
    }
    for (const auto& hunk : hunks) {
        if (!phaseOf.count(hunk.id)) {
            errors.push_back("hunk " + hunk.id + " not assigned to any phase");
        }
    }
    for (const auto& hunk : hunks) {
        auto it = phaseOf.find(hunk.id);
        if (it == phaseOf.end()) continue;
        for (const auto& depId : hunk.dependsOn) {
            auto depIt = phaseOf.find(depId);
            if (depIt == phaseOf.end()) continue; // already reported as missing above
            if (depIt->second > it->second) {
                errors.push_back("hunk " + hunk.id + " depends on " + depId +
                                  " but is scheduled in an earlier phase (" + std::to_string(it->second) +
                                  " < " + std::to_string(depIt->second) + ")");
            }
        }
    }
    return errors;
}

} // namespace gitprogressive
