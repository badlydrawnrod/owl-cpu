cmake_minimum_required(VERSION 3.14)

project(owl-cpuTests LANGUAGES CXX)

include(../cmake/project-is-top-level.cmake)
include(../cmake/folders.cmake)

# ---- Dependencies ----

if(PROJECT_IS_TOP_LEVEL)
  find_package(owl-cpu REQUIRED)
  enable_testing()
endif()

# ---- Tests ----

add_executable(owl-cpu_test source/owl-cpu_test.cpp)
target_link_libraries(owl-cpu_test PRIVATE owl-cpu::owl-cpu)
target_compile_features(owl-cpu_test PRIVATE cxx_std_20)

add_test(NAME owl-cpu_test COMMAND owl-cpu_test)

# ---- End-of-file commands ----

add_folders(Test)
