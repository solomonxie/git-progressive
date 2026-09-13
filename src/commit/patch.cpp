#include "commit/patch.hpp"

#include <unordered_set>

namespace gitprogressive {

std::string synthesizePatch(const std::vector<Hunk>& hunks, const std::vector<std::string>& hunkIds) {
    std::unordered_set<std::string> wanted(hunkIds.begin(), hunkIds.end());
    std::string patch;
    std::string currentFileKey;
    bool haveCurrent = false;

    for (const auto& hunk : hunks) {
        if (!wanted.count(hunk.id)) continue;
        std::string fileKey = hunk.oldPath + "\x1f" + hunk.newPath;
        if (!haveCurrent || fileKey != currentFileKey) {
            std::string aPath = hunk.oldPath == "/dev/null" ? hunk.newPath : hunk.oldPath;
            std::string bPath = hunk.newPath == "/dev/null" ? hunk.oldPath : hunk.newPath;
            patch += "diff --git a/" + aPath + " b/" + bPath + "\n";
            // Without an explicit new/deleted file mode line, git apply
            // path-strips "/dev/null" too (-p1 strips its leading empty
            // component, yielding the bogus path "dev/null").
            if (hunk.oldPath == "/dev/null") {
                patch += "new file mode 100644\n";
            } else if (hunk.newPath == "/dev/null") {
                patch += "deleted file mode 100644\n";
            }
            patch += "--- " + (hunk.oldPath == "/dev/null" ? std::string("/dev/null") : "a/" + hunk.oldPath) + "\n";
            patch += "+++ " + (hunk.newPath == "/dev/null" ? std::string("/dev/null") : "b/" + hunk.newPath) + "\n";
            currentFileKey = fileKey;
            haveCurrent = true;
        }
        patch += hunk.text;
    }
    return patch;
}

} // namespace gitprogressive
