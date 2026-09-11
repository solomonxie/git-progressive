#include "audit/audit.hpp"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace gitprogressive {

namespace fs = std::filesystem;

namespace {

std::string timestamp(const char* fmt) {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::ostringstream os;
    os << std::put_time(&tm, fmt);
    return os.str();
}

} // namespace

AuditPaths resolveAuditPaths(const std::string& overrideDir) {
    AuditPaths paths;
    // Always /tmp by default — not $TMPDIR (macOS's per-user, per-boot
    // /var/folders/... path), so the location is predictable and the
    // same across runs/machines. --audit-dir overrides.
    std::string base = overrideDir.empty() ? "/tmp" : overrideDir;
    paths.dir = base + "/git-progressive-" + timestamp("%Y%m%d-%H%M%S");
    fs::create_directories(paths.dir);
    paths.outlinePath = paths.dir + "/outline.md";
    paths.planPath = paths.dir + "/plan.md";
    paths.logPath = paths.dir + "/agent.log";
    return paths;
}

Audit::Audit(AuditPaths paths) : paths_(std::move(paths)) {
    logFile_.open(paths_.logPath, std::ios::out | std::ios::trunc);
}

void Audit::printBanner() const {
    std::cout << "git-progressive: auditable run files (watch them while it runs):\n"
              << "  outline: " << paths_.outlinePath << "\n"
              << "  plan:    " << paths_.planPath << "\n"
              << "  log:     " << paths_.logPath << "  (tail -f " << paths_.logPath << ")\n";
}

void Audit::log(const std::string& line) {
    std::string stamped = "[" + timestamp("%H:%M:%S") + "] " + line;
    std::cout << stamped << "\n";
    if (logFile_) logFile_ << stamped << std::endl;
}

void Audit::writeOutline(const std::string& markdown) {
    std::ofstream(paths_.outlinePath, std::ios::out | std::ios::trunc) << markdown;
}

void Audit::writePlan(const std::string& markdown) {
    std::ofstream(paths_.planPath, std::ios::out | std::ios::trunc) << markdown;
}

} // namespace gitprogressive
