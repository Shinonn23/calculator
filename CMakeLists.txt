cmake_minimum_required(VERSION 3.10)
project("math-solver" VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ===== JSON Library =====
include(FetchContent)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

# ===== Replxx Library (interactive line editing) =====
FetchContent_Declare(
    replxx
    GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
    GIT_TAG release-0.0.4
)
set(REPLXX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(REPLXX_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(json replxx)

# ===== Core library =====
add_library(math_core
    src/core/eval/evaluator.cpp
    src/core/parser/parser.cpp
)

target_include_directories(math_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(math_core
    PUBLIC nlohmann_json::nlohmann_json
)

# ===== Main executable =====
add_executable("math-solver"
    src/main.cpp
)

target_link_libraries("math-solver"
    PRIVATE math_core
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE replxx::replxx
)
