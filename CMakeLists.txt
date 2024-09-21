cmake_minimum_required(VERSION 3.25)
project(owl-cpu VERSION 0.1.0 LANGUAGES C CXX)

add_executable(lbavm-011 source/lbavm-011.cpp)
target_compile_features(lbavm-011 PUBLIC cxx_std_20)
set_target_properties(lbavm-011 PROPERTIES CXX_EXTENSIONS OFF)

add_executable(lbavm-011-benchmark source/lbavm-011-benchmark.cpp)
target_compile_features(lbavm-011-benchmark PUBLIC cxx_std_20)
set_target_properties(lbavm-011-benchmark PROPERTIES CXX_EXTENSIONS OFF)
