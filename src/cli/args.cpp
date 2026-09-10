#include "cli/args.hpp"

namespace gitprogressive {

// TODO(T7.1): real flag parsing (--branch-name, --provider, --dry-run).
Args parseArgs(int argc, char** argv) {
    Args args;
    if (argc < 2) {
        return args;
    }
    args.range = argv[1];
    args.valid = true;
    return args;
}

} // namespace gitprogressive
