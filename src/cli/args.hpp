#pragma once

#include <string>

namespace gitprogressive {

enum class Provider { OpenAI, Claude };

struct Args {
    std::string range;
    std::string branchName = "progressive";
    Provider provider = Provider::Claude;
    bool dryRun = false;
    bool valid = false;
};

Args parseArgs(int argc, char** argv);

} // namespace gitprogressive
