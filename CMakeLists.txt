cmake_minimum_required(VERSION 3.10)
project("math-solver" VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler warnings and settings
if(MSVC)
    add_compile_options(/W4 /EHsc)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Core library
add_library(math_core
    src/eval/evaluator.cpp
    src/parser/parser.cpp
)

target_include_directories(math_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Main executable
add_executable("math-solver"
    src/main.cpp
)

target_link_libraries("math-solver"
    PRIVATE math_core
)


# Testing Setup
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()

# Collect all test files
file(GLOB_RECURSE TEST_SOURCES
    tests/**/*.cpp
)

# Create test executable
add_executable(math_tests
    ${TEST_SOURCES}
)

# ให้ test เห็น include path ของ core
target_include_directories(math_tests
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# link library และ gtest
target_link_libraries(math_tests
    PRIVATE
        math_core
        GTest::gtest_main
)

# Auto register tests
include(GoogleTest)
gtest_discover_tests(math_tests
    DISCOVERY_MODE POST_BUILD
)