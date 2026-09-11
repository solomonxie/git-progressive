#pragma once

#include <string>
#include <vector>

#include "diff/hunk.hpp"

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
};

// Table-of-contents-style rendering: one heading per item, one bullet
// per hunk with its file:line-range looked up from `hunks` — the file
// is self-describing on its own, no in-memory hunk table needed to
// understand what a pointer refers to.
std::string renderOutline(const Outline& outline, const std::vector<Hunk>& hunks);

// Reconstructs item id/title/hunk-id membership from renderOutline's
// output. This is the real hand-off between the outline pass and the
// grouping pass (T6b.2/T6b.3 read outline.md back off disk, not the
// in-memory Outline) — location details in the text aren't needed back,
// they're re-derived from `hunks` wherever rendered again.
Outline parseOutline(const std::string& markdown);

} // namespace gitprogressive
