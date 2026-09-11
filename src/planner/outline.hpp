#pragma once

#include <string>
#include <vector>

namespace gitprogressive {

// One category in the running outline (T6b.1) — e.g. "feature: add
// search endpoint" — with pointers to every hunk classified under it.
struct OutlineItem {
    std::string id;
    std::string title;
    std::vector<std::string> hunkIds;
};

struct Outline {
    std::vector<OutlineItem> items;

    // Table-of-contents-style rendering used both as the audit file
    // snapshot and as the prompt context fed back to the model.
    std::string toMarkdown() const;
};

} // namespace gitprogressive
