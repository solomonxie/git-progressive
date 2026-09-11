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

add_executable(git-progressive
    src/main.cpp
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

target_include_directories(git-progressive PRIVATE src)
target_link_libraries(git-progressive PRIVATE nlohmann_json::nlohmann_json httplib::httplib)
