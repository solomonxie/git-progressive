#include "cli/args.hpp"

namespace gitprogressive {

namespace {

bool parseProvider(const std::string& value, LlmBackend& out) {
    if (value == "ollama") {
        out = LlmBackend::Ollama;
    } else if (value == "openai") {
        out = LlmBackend::OpenAI;
    } else if (value == "claude") {
        out = LlmBackend::Claude;
    } else {
        return false;
    }
    return true;
}

} // namespace

Args parseArgs(int argc, char** argv) {
    Args invalid; // range left empty => valid stays false
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        bool needsValue = arg == "--branch-name" || arg == "--provider" || arg == "--model" || arg == "--host";
        if (needsValue && i + 1 >= argc) return invalid;

        if (arg == "--branch-name") {
            args.branchName = argv[++i];
        } else if (arg == "--provider") {
            if (!parseProvider(argv[++i], args.provider)) return invalid;
        } else if (arg == "--model") {
            args.model = argv[++i];
        } else if (arg == "--host") {
            args.ollamaHost = argv[++i];
        } else if (arg == "--dry-run") {
            args.dryRun = true;
        } else if (!arg.empty() && arg[0] == '-') {
            return invalid; // unknown flag
        } else if (args.range.empty()) {
            args.range = arg;
        } else {
            return invalid; // unexpected extra positional arg
        }
    }
    args.valid = !args.range.empty();
    return args;
}

} // namespace gitprogressive
