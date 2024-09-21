cmake_minimum_required(VERSION 3.25)
project(owl-cpu VERSION 0.1.0 LANGUAGES C CXX)

# Part 2 Implementing the main loop and an assembler
# https://badlydrawnrod.github.io/posts/2024/04/07/lbavm-002/
add_executable(lbavm-002 source/lbavm-002.cpp)
target_compile_features(lbavm-002 PUBLIC cxx_std_20)
set_target_properties(lbavm-002 PROPERTIES CXX_EXTENSIONS OFF)

# Part 3 Adding labels to the assembler
# https://badlydrawnrod.github.io/posts/2024/05/05/lbavm-003/
add_executable(lbavm-003 source/lbavm-003.cpp)
target_compile_features(lbavm-003 PUBLIC cxx_std_20)
set_target_properties(lbavm-003 PROPERTIES CXX_EXTENSIONS OFF)

# Part 4 Call, return, and calling conventions
# https://badlydrawnrod.github.io/posts/2024/06/11/lbavm-004/
add_executable(lbavm-004 source/lbavm-004.cpp)
target_compile_features(lbavm-004 PUBLIC cxx_std_20)
set_target_properties(lbavm-004 PROPERTIES CXX_EXTENSIONS OFF)

# Part 5 Implementing memory
# https://badlydrawnrod.github.io/posts/2024/07/05/lbavm-005/
add_executable(lbavm-005 source/lbavm-005.cpp)
target_compile_features(lbavm-005 PUBLIC cxx_std_20)
set_target_properties(lbavm-005 PROPERTIES CXX_EXTENSIONS OFF)

# Part 6 Adding system calls
# https://badlydrawnrod.github.io/posts/2024/07/21/lbavm-006/
add_executable(lbavm-006 source/lbavm-006.cpp)
target_compile_features(lbavm-006 PUBLIC cxx_std_20)
set_target_properties(lbavm-006 PROPERTIES CXX_EXTENSIONS OFF)

# Part 7 Implementing the Owl-2820 instruction set
# https://badlydrawnrod.github.io/posts/2024/07/30/lbavm-007/
add_executable(lbavm-007 source/lbavm-007.cpp)
target_compile_features(lbavm-007 PUBLIC cxx_std_20)
set_target_properties(lbavm-007 PROPERTIES CXX_EXTENSIONS OFF)

# Part 9 Running a compiled image on an Owl-2820 VM
# https://badlydrawnrod.github.io/posts/2024/09/01/lbavm-009/
add_executable(lbavm-009 source/lbavm-009.cpp)
target_compile_features(lbavm-009 PUBLIC cxx_std_20)
set_target_properties(lbavm-009 PROPERTIES CXX_EXTENSIONS OFF)

# Part 10 Running RISC-V instructions directly
# https://badlydrawnrod.github.io/posts/2024/09/08/lbavm-010/
add_executable(lbavm-010 source/lbavm-010.cpp)
target_compile_features(lbavm-010 PUBLIC cxx_std_20)
set_target_properties(lbavm-010 PROPERTIES CXX_EXTENSIONS OFF)
# Microbenchmark from Part 10.
add_executable(lbavm-010-benchmark source/lbavm-010-benchmark.cpp)
target_compile_features(lbavm-010-benchmark PUBLIC cxx_std_20)
set_target_properties(lbavm-010-benchmark PROPERTIES CXX_EXTENSIONS OFF)

# Part 11 WIP
add_executable(lbavm-011 source/lbavm-011.cpp)
target_compile_features(lbavm-011 PUBLIC cxx_std_20)
set_target_properties(lbavm-011 PROPERTIES CXX_EXTENSIONS OFF)

add_executable(lbavm-011-benchmark source/lbavm-011-benchmark.cpp)
target_compile_features(lbavm-011-benchmark PUBLIC cxx_std_20)
set_target_properties(lbavm-011-benchmark PROPERTIES CXX_EXTENSIONS OFF)
