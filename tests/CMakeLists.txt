include(FetchContent)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # doctest's own CMakeLists predates CMake 4's minimum
FetchContent_Declare(doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
)
FetchContent_MakeAvailable(doctest)

add_executable(git-progressive-tests
    main_test.cpp
    diff_parser_test.cpp
    validate_test.cpp
    outline_test.cpp
    agent_loop_test.cpp
    visualize_test.cpp
)
target_link_libraries(git-progressive-tests PRIVATE git-progressive-core doctest::doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(git-progressive-tests)
