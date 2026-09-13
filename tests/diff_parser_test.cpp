#include <doctest/doctest.h>

#include "diff/parser.hpp"

using namespace gitprogressive;

namespace {

const char* kTwoFileDiff =
    "diff --git a/foo.txt b/foo.txt\n"
    "index 1234567..89abcde 100644\n"
    "--- a/foo.txt\n"
    "+++ b/foo.txt\n"
    "@@ -1,3 +1,4 @@\n"
    " one\n"
    "+one point five\n"
    " two\n"
    " three\n"
    "@@ -10,2 +11,2 @@\n"
    "-ten\n"
    "+ten (edited)\n"
    " eleven\n"
    "diff --git a/bar.txt b/bar.txt\n"
    "new file mode 100644\n"
    "index 0000000..1111111\n"
    "--- /dev/null\n"
    "+++ b/bar.txt\n"
    "@@ -0,0 +1,2 @@\n"
    "+hello\n"
    "+world\n";

} // namespace

TEST_CASE("parseDiff splits hunks across files") {
    std::vector<Hunk> hunks = parseDiff(kTwoFileDiff);
    REQUIRE(hunks.size() == 3);

    CHECK(hunks[0].file == "foo.txt");
    CHECK(hunks[0].oldPath == "foo.txt");
    CHECK(hunks[0].newPath == "foo.txt");
    CHECK(hunks[0].oldStart == 1);
    CHECK(hunks[0].oldLines == 3);
    CHECK(hunks[0].newStart == 1);
    CHECK(hunks[0].newLines == 4);
    CHECK(hunks[0].text.rfind("@@ -1,3 +1,4 @@", 0) == 0);

    CHECK(hunks[1].file == "foo.txt");
    CHECK(hunks[1].oldStart == 10);
    CHECK(hunks[1].newStart == 11);

    CHECK(hunks[2].file == "bar.txt");
    CHECK(hunks[2].oldPath == "/dev/null");
    CHECK(hunks[2].newPath == "bar.txt");
}

TEST_CASE("parseDiff assigns stable, unique, sequential ids") {
    std::vector<Hunk> hunks = parseDiff(kTwoFileDiff);
    REQUIRE(hunks.size() == 3);
    CHECK(hunks[0].id == "h1");
    CHECK(hunks[1].id == "h2");
    CHECK(hunks[2].id == "h3");
}

TEST_CASE("parseDiff on empty input yields no hunks") {
    CHECK(parseDiff("").empty());
}

TEST_CASE("parseDiff defaults omitted hunk-header counts to 1") {
    const char* diff =
        "diff --git a/f.txt b/f.txt\n"
        "--- a/f.txt\n"
        "+++ b/f.txt\n"
        "@@ -5 +5,2 @@\n"
        " context\n"
        "+added\n";
    std::vector<Hunk> hunks = parseDiff(diff);
    REQUIRE(hunks.size() == 1);
    CHECK(hunks[0].oldStart == 5);
    CHECK(hunks[0].oldLines == 1);
    CHECK(hunks[0].newStart == 5);
    CHECK(hunks[0].newLines == 2);
}

TEST_CASE("detectDependencies chains same-file hunks in order") {
    std::vector<Hunk> hunks = parseDiff(kTwoFileDiff);
    detectDependencies(hunks);
    REQUIRE(hunks.size() == 3);

    CHECK(hunks[0].dependsOn.empty()); // first foo.txt hunk: no earlier hunk in that file
    REQUIRE(hunks[1].dependsOn.size() == 1);
    CHECK(hunks[1].dependsOn[0] == hunks[0].id); // second foo.txt hunk depends on the first
    CHECK(hunks[2].dependsOn.empty()); // bar.txt hunk: independent file, no dependency
}
