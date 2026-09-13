#include "visualize/visualize.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "commit/patch.hpp"

namespace gitprogressive {

using json = nlohmann::json;

namespace {

std::string escapeHtml(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string diffLineClass(const std::string& line) {
    if (line.rfind("diff --git", 0) == 0 || line.rfind("---", 0) == 0 || line.rfind("+++", 0) == 0 ||
        line.rfind("new file mode", 0) == 0 || line.rfind("deleted file mode", 0) == 0) {
        return "hdr";
    }
    if (line.rfind("@@", 0) == 0) return "hunk";
    if (!line.empty() && line[0] == '+') return "add";
    if (!line.empty() && line[0] == '-') return "del";
    return "ctx";
}

// One <span> per line so each can be colorized by its diff prefix; the
// span text itself is HTML-escaped, the markup around it is not.
std::string renderDiffHtml(const std::string& diff) {
    std::ostringstream html;
    std::istringstream stream(diff);
    std::string line;
    while (std::getline(stream, line)) {
        html << "<span class=\"" << diffLineClass(line) << "\">" << escapeHtml(line) << "</span>\n";
    }
    return html.str();
}

const char* kPageTemplate = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>git-progressive: phases</title>
<style>
body { display: flex; height: 100vh; margin: 0; font-family: -apple-system, BlinkMacSystemFont, sans-serif; }
nav { width: 280px; overflow-y: auto; border-right: 1px solid #ddd; padding: 8px; box-sizing: border-box; flex-shrink: 0; }
nav button { display: block; width: 100%; text-align: left; padding: 8px; margin-bottom: 4px; border: none;
             background: none; cursor: pointer; border-radius: 4px; font-size: 13px; }
nav button:hover { background: #eef1f8; }
nav button.active { background: #dbe4fb; font-weight: 600; }
main { flex: 1; overflow-y: auto; padding: 16px 24px; box-sizing: border-box; }
h2 { margin-top: 0; }
.rationale { color: #555; margin-bottom: 16px; white-space: pre-wrap; }
pre { background: #f6f8fa; padding: 12px; overflow-x: auto; font-size: 13px; line-height: 1.5; border-radius: 6px; }
pre span { display: block; white-space: pre; }
.add { color: #22863a; background: #e6ffed; }
.del { color: #b31d28; background: #ffeef0; }
.hunk { color: #6f42c1; }
.hdr { color: #999; }
</style>
</head>
<body>
<nav id="nav"></nav>
<main id="main"></main>
<script>
const phases = __PHASES_JSON__;
const nav = document.getElementById('nav');
const main = document.getElementById('main');
phases.forEach((p, i) => {
  const b = document.createElement('button');
  b.textContent = (i + 1) + '. ' + p.title;
  b.onclick = () => show(i);
  nav.appendChild(b);
});
function show(i) {
  [...nav.children].forEach((b, j) => b.classList.toggle('active', j === i));
  main.innerHTML = '<h2>' + (i + 1) + '. ' + phases[i].title + '</h2>'
    + '<div class="rationale">' + phases[i].rationale + '</div>'
    + '<pre>' + phases[i].diffHtml + '</pre>';
}
if (phases.length) show(0);
</script>
</body>
</html>
)HTML";

} // namespace

std::string renderVisualizationHtml(const Plan& plan, const std::vector<Hunk>& hunks) {
    json phases = json::array();
    for (const auto& phase : plan.phases) {
        std::string diff = synthesizePatch(hunks, phase.hunkIds);
        phases.push_back({
            {"title", escapeHtml(phase.title)},
            {"rationale", escapeHtml(phase.rationale)},
            {"diffHtml", renderDiffHtml(diff)},
        });
    }

    std::string page = kPageTemplate;
    std::string placeholder = "__PHASES_JSON__";
    page.replace(page.find(placeholder), placeholder.size(), phases.dump());
    return page;
}

void writeVisualization(const Plan& plan, const std::vector<Hunk>& hunks, const std::string& outPath) {
    std::ofstream out(outPath);
    if (!out) throw std::runtime_error("failed to write visualization to " + outPath);
    out << renderVisualizationHtml(plan, hunks);
}

} // namespace gitprogressive
