cmake_minimum_required(VERSION 3.10)

project(math-solver
    VERSION 1.0.0
    LANGUAGES CXX
)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source build is not allowed. Use a build directory.")
endif()

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Suppress warnings from third-party dependencies
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE INTERNAL "")

# ===== Dependencies =====
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# JSON
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

# Replxx
FetchContent_Declare(
    replxx
    GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
    GIT_TAG release-0.0.4
    GIT_SHALLOW TRUE
)

set(REPLXX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(REPLXX_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)

# Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)
set(BUILD_GMOCK   OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(json replxx googletest)

# ===== Core Library =====
add_library(math_core
    src/algebra/linear/linear_collector.cpp
    src/algebra/linear/simplify.cpp
    src/algebra/polynomial/ast_to_poly.cpp
    src/algebra/polynomial/factor.cpp
    src/algebra/solver/solver.cpp
    src/commands/registry.cpp
    src/config/config.cpp
    src/eval/evaluator.cpp
    src/lexer/command/command_token_stream.cpp
    src/parser/command/command_parser.cpp
    src/parser/command/command_parser_registry.cpp
    src/parser/command/subparsers/config_command_parser.cpp
    src/parser/command/subparsers/env_command_parser.cpp
    src/parser/command/subparsers/history_command_parser.cpp
    src/parser/command/subparsers/load_command_parser.cpp
    src/parser/command/subparsers/math_command_parser.cpp
    src/parser/command/subparsers/redo_command_parser.cpp
    src/parser/command/subparsers/system_command_parser.cpp
    src/parser/command/subparsers/var_command_parser.cpp
    src/parser/math/math_parser.cpp
    src/runtime/context/context.cpp
    src/runtime/context/resolver.cpp
    src/runtime/context/validator.cpp
    src/runtime/runtime.cpp
    src/ui/repl/repl.cpp
    src/ui/repl/runner.cpp
    src/diagnostics/diagnostic.cpp
)

target_include_directories(math_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(math_core PUBLIC cxx_std_17)

target_link_libraries(math_core
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC replxx::replxx
)

# Apply warnings ONLY to project targets
target_compile_options(math_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

if(MSVC)
    target_compile_definitions(math_core PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ===== Executable =====
add_executable(math-solver
    src/main.cpp
)

target_compile_features(math-solver PRIVATE cxx_std_17)

target_link_libraries(math-solver
    PRIVATE math_core
    PRIVATE replxx::replxx
)

target_compile_options(math-solver PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

if(MSVC)
    target_compile_definitions(math-solver PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ===== AST Unit Tests =====
enable_testing()
include(GoogleTest)

add_executable(ast_tests
    tests/ast/math/test_span.cpp
    tests/ast/math/test_number_expr.cpp
    tests/ast/math/test_variable_expr.cpp
    tests/ast/math/test_binary_expr.cpp
    tests/ast/math/test_equation_expr.cpp
    tests/ast/math/test_expr_visitor.cpp
    tests/ast/command/test_history_entry.cpp
    tests/ast/command/test_command_base.cpp
    tests/ast/command/test_system_command.cpp
    tests/ast/command/test_math_command.cpp
    tests/ast/command/test_var_command.cpp
    tests/ast/command/test_config_command.cpp
    tests/ast/command/test_env_command.cpp
    tests/ast/command/test_load_command.cpp
    tests/ast/command/test_history_command.cpp
    tests/ast/command/test_redo_command.cpp
    src/diagnostics/diagnostic.cpp
)

target_include_directories(ast_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_compile_features(ast_tests PRIVATE cxx_std_17)

target_link_libraries(ast_tests PRIVATE
    GTest::gtest_main
    nlohmann_json::nlohmann_json
    replxx::replxx
)

target_compile_options(ast_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

gtest_discover_tests(ast_tests)

# ===== UI Integration Tests =====
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(
    NAME ui_tests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_ui_tests.py
        --binary $<TARGET_FILE:math-solver>
        --tests-dir ${CMAKE_CURRENT_SOURCE_DIR}/tests/ui
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(ui_tests PROPERTIES
    DEPENDS math-solver
)
