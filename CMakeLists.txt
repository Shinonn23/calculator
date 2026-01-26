cmake_minimum_required(VERSION 3.10)
project("math-solver" VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Core library
add_library(math_core
    src/core/eval/evaluator.cpp
    src/core/parser/parser.cpp
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
