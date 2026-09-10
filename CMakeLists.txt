cmake_minimum_required(VERSION 3.16)
project(git-progressive CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(git-progressive
    src/main.cpp
    src/cli/args.cpp
    src/git/repo.cpp
    src/diff/parser.cpp
    src/agent/loop.cpp
    src/planner/planner.cpp
    src/commit/builder.cpp
)

target_include_directories(git-progressive PRIVATE src)
