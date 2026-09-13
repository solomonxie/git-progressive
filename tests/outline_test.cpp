#include <doctest/doctest.h>

#include "planner/outline.hpp"

using namespace gitprogressive;

namespace {

std::vector<Hunk> twoHunks() {
    Hunk a; a.id = "h1"; a.file = "src/foo.cpp"; a.newStart = 10; a.newLines = 3;
    Hunk b; b.id = "h2"; b.file = "src/bar.cpp"; b.newStart = 40; b.newLines = 1;
    return {a, b};
}

} // namespace

TEST_CASE("renderOutline/parseOutline round-trips item/hunk membership") {
    auto hunks = twoHunks();
    Outline outline;
    outline.items.push_back({"item-1", "skeleton building", {"h1"}});
    outline.items.push_back({"item-2", "feature: add search endpoint", {"h2"}});

    std::string markdown = renderOutline(outline, hunks);
    Outline parsed = parseOutline(markdown);

    REQUIRE(parsed.items.size() == 2);
    CHECK(parsed.items[0].title == "skeleton building");
    CHECK(parsed.items[0].hunkIds == std::vector<std::string>{"h1"});
    CHECK(parsed.items[1].title == "feature: add search endpoint");
    CHECK(parsed.items[1].hunkIds == std::vector<std::string>{"h2"});
}

TEST_CASE("renderOutline self-describes file:line-range without the hunk table") {
    auto hunks = twoHunks();
    Outline outline;
    outline.items.push_back({"item-1", "skeleton building", {"h1"}});
    std::string markdown = renderOutline(outline, hunks);
    CHECK(markdown.find("src/foo.cpp:10-12") != std::string::npos);
}

TEST_CASE("parseOutline on an empty document yields no items") {
    CHECK(parseOutline("").items.empty());
}

TEST_CASE("an item with multiple hunk pointers round-trips all of them") {
    auto hunks = twoHunks();
    Outline outline;
    outline.items.push_back({"item-1", "clean: remove dead code", {"h1", "h2"}});
    Outline parsed = parseOutline(renderOutline(outline, hunks));
    REQUIRE(parsed.items.size() == 1);
    CHECK(parsed.items[0].hunkIds == std::vector<std::string>{"h1", "h2"});
}
