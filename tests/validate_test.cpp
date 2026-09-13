#include <doctest/doctest.h>

#include "planner/validate.hpp"

using namespace gitprogressive;

namespace {

std::vector<Hunk> threeChainedHunks() {
    Hunk a; a.id = "h1"; a.file = "f.txt";
    Hunk b; b.id = "h2"; b.file = "f.txt"; b.dependsOn = {"h1"};
    Hunk c; c.id = "h3"; c.file = "f.txt"; c.dependsOn = {"h2"};
    return {a, b, c};
}

} // namespace

TEST_CASE("validatePlan accepts full coverage in dependency order") {
    auto hunks = threeChainedHunks();
    Plan plan;
    plan.phases.push_back({"phase 1", "", {"h1"}});
    plan.phases.push_back({"phase 2", "", {"h2", "h3"}});
    CHECK(validatePlan(hunks, plan).empty());
}

TEST_CASE("validatePlan rejects a hunk scheduled before its dependency") {
    auto hunks = threeChainedHunks();
    Plan plan;
    plan.phases.push_back({"phase 1", "", {"h2"}}); // h2 depends on h1, not yet scheduled
    plan.phases.push_back({"phase 2", "", {"h1", "h3"}});
    auto errors = validatePlan(hunks, plan);
    REQUIRE(!errors.empty());
    CHECK(errors[0].find("h2") != std::string::npos);
}

TEST_CASE("validatePlan allows a dependency and dependent in the same phase") {
    auto hunks = threeChainedHunks();
    Plan plan;
    plan.phases.push_back({"phase 1", "", {"h1", "h2", "h3"}});
    CHECK(validatePlan(hunks, plan).empty());
}

TEST_CASE("validatePlan flags an unassigned hunk") {
    auto hunks = threeChainedHunks();
    Plan plan;
    plan.phases.push_back({"phase 1", "", {"h1", "h2"}}); // h3 missing
    auto errors = validatePlan(hunks, plan);
    bool found = false;
    for (const auto& e : errors) found |= e.find("h3") != std::string::npos && e.find("not assigned") != std::string::npos;
    CHECK(found);
}

TEST_CASE("validatePlan flags a hunk assigned to more than one phase") {
    auto hunks = threeChainedHunks();
    Plan plan;
    plan.phases.push_back({"phase 1", "", {"h1"}});
    plan.phases.push_back({"phase 2", "", {"h1", "h2", "h3"}}); // h1 duplicated
    auto errors = validatePlan(hunks, plan);
    bool found = false;
    for (const auto& e : errors) found |= e.find("more than one phase") != std::string::npos;
    CHECK(found);
}

TEST_CASE("validatePlan flags an unknown hunk id") {
    auto hunks = threeChainedHunks();
    Plan plan;
    plan.phases.push_back({"phase 1", "", {"h1", "h2", "h3", "ghost"}});
    auto errors = validatePlan(hunks, plan);
    bool found = false;
    for (const auto& e : errors) found |= e.find("unknown hunk id") != std::string::npos;
    CHECK(found);
}
