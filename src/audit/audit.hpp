#pragma once

#include <fstream>
#include <string>

namespace gitprogressive {

struct AuditPaths {
    std::string dir;
    std::string outlinePath;
    std::string planPath;
    std::string logPath;
};

// Resolves the audit directory (default `/tmp/git-progressive`, or
// `overrideDir` if given) and creates it. The same directory is reused
// across runs — outline.md/plan.md/agent.log are truncated at the start
// of each run, so it always reflects the latest run only.
AuditPaths resolveAuditPaths(const std::string& overrideDir);

// Keeps a run auditable while it's in progress: outline.md and plan.md
// are overwritten with the latest snapshot on every update (not just at
// the end), and every agent decision is appended to agent.log and
// echoed to stdout live.
class Audit {
public:
    explicit Audit(AuditPaths paths);

    const AuditPaths& paths() const { return paths_; }

    // Prints the file locations to stdout — call once at startup, before
    // anything else runs, so the user knows what to watch.
    void printBanner() const;

    void log(const std::string& line);
    void writeOutline(const std::string& markdown);
    void writePlan(const std::string& markdown);

private:
    AuditPaths paths_;
    std::ofstream logFile_;
};

} // namespace gitprogressive
