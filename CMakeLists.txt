cmake_minimum_required(VERSION 3.25)
project(owl-cpu VERSION 0.1.0 LANGUAGES C CXX)

add_executable(lbavm-012 source/lbavm-012.cpp)
target_compile_features(lbavm-012 PUBLIC cxx_std_20)
set_target_properties(lbavm-012 PROPERTIES CXX_EXTENSIONS OFF)
