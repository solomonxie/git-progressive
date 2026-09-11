#include "planner/outline.hpp"

#include <sstream>
#include <unordered_map>

namespace gitprogressive {

namespace {

std::string hunkLocation(const Hunk& hunk) {
    std::ostringstream os;
    os << hunk.file << ":";
    if (hunk.newLines > 0) {
        os << hunk.newStart << "-" << (hunk.newStart + hunk.newLines - 1);
    } else {
        os << hunk.oldStart << "-" << (hunk.oldStart + hunk.oldLines - 1) << " (deleted)";
    }
    if (hunk.oldPath == "/dev/null") os << " (new file)";
    return os.str();
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // namespace

std::string renderOutline(const Outline& outline, const std::vector<Hunk>& hunks) {
    std::unordered_map<std::string, const Hunk*> byId;
    for (const auto& hunk : hunks) byId[hunk.id] = &hunk;

    std::ostringstream os;
    os << "# Outline\n\n";
    if (outline.items.empty()) {
        os << "(empty)\n";
        return os.str();
    }
    for (const auto& item : outline.items) {
        os << "## " << item.id << ": " << item.title << "\n\n";
        for (const auto& hunkId : item.hunkIds) {
            auto it = byId.find(hunkId);
            os << "- " << hunkId << " — " << (it != byId.end() ? hunkLocation(*it->second) : "(unknown hunk)")
               << "\n";
        }
        os << "\n";
    }
    return os.str();
}

Outline parseOutline(const std::string& markdown) {
    Outline outline;
    std::istringstream stream(markdown);
    std::string line;
    OutlineItem* current = nullptr;

    while (std::getline(stream, line)) {
        if (line.rfind("## ", 0) == 0) {
            std::string rest = line.substr(3);
            size_t colon = rest.find(':');
            OutlineItem item;
            item.id = trim(colon == std::string::npos ? rest : rest.substr(0, colon));
            item.title = colon == std::string::npos ? "" : trim(rest.substr(colon + 1));
            outline.items.push_back(std::move(item));
            current = &outline.items.back();
            continue;
        }
        if (current && line.rfind("- ", 0) == 0) {
            std::string rest = line.substr(2);
            size_t space = rest.find(' ');
            std::string hunkId = trim(space == std::string::npos ? rest : rest.substr(0, space));
            if (!hunkId.empty()) current->hunkIds.push_back(hunkId);
        }
    }
    return outline;
}

} // namespace gitprogressive
