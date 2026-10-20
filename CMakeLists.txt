cmake_minimum_required(VERSION 3.20)
project(zx-reduce VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler flags & Warnings
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

# Core static library
add_library(zx_reduce_core STATIC
    src/core/phase.cpp
    src/core/scalar.cpp
    src/core/bitmatrix.cpp
    src/core/graph.cpp
    src/core/tensor_oracle.cpp
    src/io/qasm_lexer.cpp
    src/io/qasm_parser.cpp
    src/io/qasm_writer.cpp
    src/zx/from_circuit.cpp
    src/zx/to_graph_like.cpp
    src/zx/simplify.cpp
    src/zx/extract.cpp
    src/zx/postopt.cpp
)

target_include_directories(zx_reduce_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# CLI Executable
add_executable(zx-reduce
    src/cli/main.cpp
)

target_link_libraries(zx-reduce PRIVATE zx_reduce_core)

# Testing
include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    macro(add_zx_test name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE zx_reduce_core Catch2::Catch2WithMain)
        add_test(NAME ${name} COMMAND ${name})
    endmacro()

    add_zx_test(test_phase_scalar tests/test_phase_scalar.cpp)
    add_zx_test(test_bitmatrix tests/test_bitmatrix.cpp)
    add_zx_test(test_graph tests/test_graph.cpp)
    add_zx_test(test_tensor_oracle tests/test_tensor_oracle.cpp)
    add_zx_test(test_parser_writer tests/test_parser_writer.cpp)
    add_zx_test(test_rules tests/test_rules.cpp)
    add_zx_test(test_extract tests/test_extract.cpp)
    add_zx_test(test_end_to_end tests/test_end_to_end.cpp)
endif()
