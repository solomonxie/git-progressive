cmake_minimum_required(VERSION 3.16)
project(git-progressive CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(json httplib)

# Everything except main.cpp, so tests/ can link the same code without a
# second definition of main().
add_library(git-progressive-core
    src/cli/args.cpp
    src/git/repo.cpp
    src/diff/parser.cpp
    src/llm/ollama.cpp
    src/llm/openai.cpp
    src/llm/claude.cpp
    src/agent/loop.cpp
    src/audit/audit.cpp
    src/planner/outline.cpp
    src/planner/validate.cpp
    src/planner/tools.cpp
    src/planner/planner.cpp
    src/commit/builder.cpp
)
target_include_directories(git-progressive-core PUBLIC src)
target_link_libraries(git-progressive-core PUBLIC nlohmann_json::nlohmann_json httplib::httplib)

add_executable(git-progressive src/main.cpp)
target_link_libraries(git-progressive PRIVATE git-progressive-core)

enable_testing()
add_subdirectory(tests)
