#include "git/repo.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace gitprogressive {

namespace {

std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += "'";
    return out;
}

struct CommandOutput {
    std::string text;
    int exitCode;
};

CommandOutput runCommand(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("failed to run: " + cmd);
    size_t n;
    while ((n = fread(buffer.data(), 1, buffer.size(), pipe)) > 0) {
        result.append(buffer.data(), n);
    }
    int status = pclose(pipe);
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return {result, exitCode};
}

// For read-only queries (diff, show, rev-parse, ...): stderr discarded,
// throws on non-zero exit.
std::string runGit(const std::string& args) {
    CommandOutput out = runCommand("git " + args + " 2>/dev/null");
    if (out.exitCode != 0) throw std::runtime_error("git " + args + " failed");
    return out.text;
}

// For mutating commands (checkout, apply, commit, ...): captures
// stderr into the exception message so failures are diagnosable.
std::string runGitChecked(const std::string& args) {
    CommandOutput out = runCommand("git " + args + " 2>&1");
    if (out.exitCode != 0) throw std::runtime_error("git " + args + " failed:\n" + out.text);
    return out.text;
}

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

std::string defaultBranch() {
    try {
        std::string ref = trim(runGit("symbolic-ref refs/remotes/origin/HEAD"));
        auto pos = ref.rfind('/');
        if (pos != std::string::npos) return ref.substr(pos + 1);
    } catch (const std::runtime_error&) {
    }
    for (const char* candidate : {"main", "master"}) {
        try {
            runGit(std::string("rev-parse --verify ") + candidate);
            return candidate;
        } catch (const std::runtime_error&) {
        }
    }
    throw std::runtime_error("could not determine the default branch (tried origin/HEAD, main, master)");
}

std::string tempDir() {
    const char* env = std::getenv("TMPDIR");
    std::string dir = env && *env ? env : "/tmp";
    if (dir.back() != '/') dir += '/';
    return dir;
}

std::string writeTempFile(const std::string& content, const std::string& suffix) {
    std::string path = tempDir() + "git-progressive-XXXXXX" + suffix;
    std::vector<char> buf(path.begin(), path.end());
    buf.push_back('\0');
    int fd = mkstemps(buf.data(), static_cast<int>(suffix.size()));
    if (fd == -1) throw std::runtime_error("failed to create temp file at " + path);
    std::string finalPath(buf.data());
    ssize_t written = write(fd, content.data(), static_cast<ssize_t>(content.size()));
    close(fd);
    if (written < 0 || static_cast<size_t>(written) != content.size()) {
        std::remove(finalPath.c_str());
        throw std::runtime_error("failed to write temp file " + finalPath);
    }
    return finalPath;
}

} // namespace

Repository::Range Repository::resolveRange(const std::string& input) const {
    auto sep = input.find("..");
    if (sep != std::string::npos) {
        std::string left = input.substr(0, sep);
        size_t rightStart = sep + 2;
        if (rightStart < input.size() && input[rightStart] == '.') ++rightStart; // "A...B"
        std::string right = input.substr(rightStart);
        Range range;
        range.base = trim(runGit("rev-parse " + shellQuote(left)));
        range.head = trim(runGit("rev-parse " + shellQuote(right)));
        return range;
    }

    std::string head = trim(runGit("rev-parse " + shellQuote(input)));
    std::string defBranch = defaultBranch();
    std::string defHead = trim(runGit("rev-parse " + shellQuote(defBranch)));

    std::string base;
    if (defHead == head) {
        // Whole-repo walkthrough (e.g. input == "master"): base is the
        // repo's root commit, so phase 1 is the skeleton.
        base = trim(runGit("rev-list --max-parents=0 " + shellQuote(input)));
        auto nl = base.find('\n');
        if (nl != std::string::npos) base = base.substr(0, nl); // multiple roots: first one
    } else {
        base = trim(runGit("merge-base " + shellQuote(defBranch) + " " + shellQuote(input)));
    }
    return {base, head};
}

std::string Repository::diff(const Range& range) const {
    return runGit("diff " + shellQuote(range.base) + " " + shellQuote(range.head));
}

std::vector<CommitInfo> Repository::commitMessages(const Range& range) const {
    // \x1f/\x1e (unit/record separators) split fields/commits safely,
    // since commit messages can contain almost anything else.
    std::string output =
        runGit("log --pretty=format:%H%x1f%s%x1f%b%x1e " + shellQuote(range.base + ".." + range.head));

    std::vector<CommitInfo> commits;
    std::istringstream stream(output);
    std::string record;
    while (std::getline(stream, record, '\x1e')) {
        if (!record.empty() && record.front() == '\n') record.erase(0, 1);
        if (record.empty()) continue;
        size_t f1 = record.find('\x1f');
        size_t f2 = record.find('\x1f', f1 == std::string::npos ? f1 : f1 + 1);
        if (f1 == std::string::npos || f2 == std::string::npos) continue;
        CommitInfo info;
        info.hash = record.substr(0, f1);
        info.subject = record.substr(f1 + 1, f2 - f1 - 1);
        info.body = trim(record.substr(f2 + 1));
        commits.push_back(std::move(info));
    }
    return commits;
}

void Repository::createBranch(const std::string& name, const std::string& base) const {
    runGitChecked("checkout -b " + shellQuote(name) + " " + shellQuote(base));
}

std::string Repository::showFile(const std::string& ref, const std::string& path) const {
    return runGit("show " + shellQuote(ref + ":" + path));
}

void Repository::applyAndCommit(const std::string& patchText, const std::string& message) const {
    std::string patchPath = writeTempFile(patchText, ".patch");
    try {
        runGitChecked("apply --cached " + shellQuote(patchPath));
    } catch (...) {
        std::remove(patchPath.c_str());
        throw;
    }
    std::remove(patchPath.c_str());

    std::string msgPath = writeTempFile(message, ".msg");
    try {
        runGitChecked("commit -F " + shellQuote(msgPath));
    } catch (...) {
        std::remove(msgPath.c_str());
        throw;
    }
    std::remove(msgPath.c_str());
}

} // namespace gitprogressive
