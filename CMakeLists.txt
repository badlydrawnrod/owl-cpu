cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    owl-cpu
    VERSION 0.1.0
    DESCRIPTION "Short description"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    owl-cpu_owl-cpu
    source/owl-cpu.cpp
)
add_library(owl-cpu::owl-cpu ALIAS owl-cpu_owl-cpu)

include(GenerateExportHeader)
generate_export_header(
    owl-cpu_owl-cpu
    BASE_NAME owl-cpu
    EXPORT_FILE_NAME export/owl-cpu/owl-cpu_export.hpp
    CUSTOM_CONTENT_FROM_VARIABLE pragma_suppress_c4251
)

if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(owl-cpu_owl-cpu PUBLIC OWL_CPU_STATIC_DEFINE)
endif()

set_target_properties(
    owl-cpu_owl-cpu PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
    EXPORT_NAME owl-cpu
    OUTPUT_NAME owl-cpu
)

target_include_directories(
    owl-cpu_owl-cpu ${warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

target_include_directories(
    owl-cpu_owl-cpu SYSTEM
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/export>"
)

target_compile_features(owl-cpu_owl-cpu PUBLIC cxx_std_20)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Examples ----

if(PROJECT_IS_TOP_LEVEL)
  option(BUILD_EXAMPLES "Build examples tree." "${owl-cpu_DEVELOPER_MODE}")
  if(BUILD_EXAMPLES)
    add_subdirectory(example)
  endif()
endif()

# ---- Developer mode ----

if(NOT owl-cpu_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of owl-cpu"
  )
endif()

include(cmake/dev-mode.cmake)
