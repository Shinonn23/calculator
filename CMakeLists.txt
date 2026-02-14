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
)

# Replxx
FetchContent_Declare(
    replxx
    GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
    GIT_TAG release-0.0.4
)

set(REPLXX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(REPLXX_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(json replxx)

# ===== Core Library =====
add_library(math_core
    src/core/eval/evaluator.cpp
    src/core/parser/parser.cpp
)

target_include_directories(math_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(math_core PUBLIC cxx_std_17)

target_link_libraries(math_core
    PUBLIC nlohmann_json::nlohmann_json
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
