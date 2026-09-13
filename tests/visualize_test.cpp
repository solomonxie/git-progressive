#include <doctest/doctest.h>

#include "visualize/visualize.hpp"

using namespace gitprogressive;

namespace {

std::vector<Hunk> twoHunks() {
    Hunk a;
    a.id = "h1";
    a.file = "foo.txt";
    a.oldPath = "foo.txt";
    a.newPath = "foo.txt";
    a.text = "@@ -1,1 +1,2 @@\n one\n+two\n";
    Hunk b;
    b.id = "h2";
    b.file = "bar.txt";
    b.oldPath = "/dev/null";
    b.newPath = "bar.txt";
    b.text = "@@ -0,0 +1,1 @@\n+new file line\n";
    return {a, b};
}

} // namespace

TEST_CASE("renderVisualizationHtml embeds one phase per plan phase with its diff") {
    auto hunks = twoHunks();
    Plan plan;
    plan.phases.push_back({"skeleton building", "sets up the base file", {"h1"}});
    plan.phases.push_back({"feature: add bar.txt", "layers in the new file", {"h2"}});

    std::string html = renderVisualizationHtml(plan, hunks);

    CHECK(html.find("skeleton building") != std::string::npos);
    CHECK(html.find("feature: add bar.txt") != std::string::npos);
    CHECK(html.find("sets up the base file") != std::string::npos);
    CHECK(html.find("+two") != std::string::npos);
    CHECK(html.find("class=\\\"add\\\"") != std::string::npos); // embedded in the JSON-escaped diffHtml
}

TEST_CASE("renderVisualizationHtml HTML-escapes phase titles and rationale") {
    auto hunks = twoHunks();
    Plan plan;
    plan.phases.push_back({"<script>alert(1)</script>", "a & b < c", {"h1"}});
    std::string html = renderVisualizationHtml(plan, hunks);
    CHECK(html.find("<script>alert") == std::string::npos);
    CHECK(html.find("&lt;script&gt;") != std::string::npos);
    CHECK(html.find("a &amp; b &lt; c") != std::string::npos);
}

TEST_CASE("renderVisualizationHtml on an empty plan produces a valid empty page") {
    Plan plan;
    std::string html = renderVisualizationHtml(plan, {});
    CHECK(html.find("const phases = []") != std::string::npos);
}
