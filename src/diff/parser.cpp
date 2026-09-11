#include "diff/parser.hpp"

#include <cstdio>
#include <sstream>
#include <unordered_map>

namespace gitprogressive {

namespace {

std::string stripAbPrefix(const std::string& path) {
    if (path == "/dev/null") return path;
    if (path.size() > 2 && (path[0] == 'a' || path[0] == 'b') && path[1] == '/') return path.substr(2);
    return path;
}

// Parses "@@ -old[,oldLines] +new[,newLines] @@ ...". Omitted counts
// default to 1, per the unified diff format.
void parseHunkHeader(const std::string& line, Hunk& hunk) {
    int oldStart = 0, oldLines = 1, newStart = 0, newLines = 1;
    if (std::sscanf(line.c_str(), "@@ -%d,%d +%d,%d @@", &oldStart, &oldLines, &newStart, &newLines) == 4 ||
        std::sscanf(line.c_str(), "@@ -%d +%d,%d @@", &oldStart, &newStart, &newLines) == 3 ||
        std::sscanf(line.c_str(), "@@ -%d,%d +%d @@", &oldStart, &oldLines, &newStart) == 3 ||
        std::sscanf(line.c_str(), "@@ -%d +%d @@", &oldStart, &newStart) == 2) {
        // one of the sscanf calls above filled in what it matched.
    }
    hunk.oldStart = oldStart;
    hunk.oldLines = oldLines;
    hunk.newStart = newStart;
    hunk.newLines = newLines;
}

} // namespace

std::vector<Hunk> parseDiff(const std::string& unifiedDiff) {
    std::vector<Hunk> hunks;
    std::istringstream stream(unifiedDiff);
    std::string line;
    std::string oldPath, newPath;
    Hunk* current = nullptr;
    int counter = 0;

    while (std::getline(stream, line)) {
        if (line.rfind("diff --git", 0) == 0) {
            current = nullptr;
            oldPath.clear();
            newPath.clear();
            continue;
        }
        if (line.rfind("--- ", 0) == 0) {
            current = nullptr;
            oldPath = stripAbPrefix(line.substr(4));
            continue;
        }
        if (line.rfind("+++ ", 0) == 0) {
            current = nullptr;
            newPath = stripAbPrefix(line.substr(4));
            continue;
        }
        if (line.rfind("@@ ", 0) == 0) {
            Hunk hunk;
            hunk.id = "h" + std::to_string(++counter);
            hunk.oldPath = oldPath;
            hunk.newPath = newPath;
            hunk.file = (newPath != "/dev/null") ? newPath : oldPath;
            parseHunkHeader(line, hunk);
            hunk.text = line + "\n";
            hunks.push_back(std::move(hunk));
            current = &hunks.back();
            continue;
        }
        if (current && (line.empty() || line[0] == ' ' || line[0] == '+' || line[0] == '-' || line[0] == '\\')) {
            current->text += line + "\n";
            continue;
        }
        // Other lines (index, mode changes, "Binary files ... differ", ...)
        // carry no hunk body of their own; not supported in v1.
        current = nullptr;
    }
    return hunks;
}

void detectDependencies(std::vector<Hunk>& hunks) {
    std::unordered_map<std::string, std::string> lastHunkIdForFile;
    for (auto& hunk : hunks) {
        auto it = lastHunkIdForFile.find(hunk.file);
        if (it != lastHunkIdForFile.end()) {
            hunk.dependsOn.push_back(it->second);
        }
        lastHunkIdForFile[hunk.file] = hunk.id;
    }
}

} // namespace gitprogressive
