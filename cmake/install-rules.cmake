if(PROJECT_IS_TOP_LEVEL)
  set(
      CMAKE_INSTALL_INCLUDEDIR "include/owl-cpu-${PROJECT_VERSION}"
      CACHE STRING ""
  )
  set_property(CACHE CMAKE_INSTALL_INCLUDEDIR PROPERTY TYPE PATH)
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
set(package owl-cpu)

install(
    DIRECTORY
    include/
    "${PROJECT_BINARY_DIR}/export/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT owl-cpu_Development
)

install(
    TARGETS owl-cpu_owl-cpu
    EXPORT owl-cpuTargets
    RUNTIME #
    COMPONENT owl-cpu_Runtime
    LIBRARY #
    COMPONENT owl-cpu_Runtime
    NAMELINK_COMPONENT owl-cpu_Development
    ARCHIVE #
    COMPONENT owl-cpu_Development
    INCLUDES #
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

# Allow package maintainers to freely override the path for the configs
set(
    owl-cpu_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${package}"
    CACHE STRING "CMake package config location relative to the install prefix"
)
set_property(CACHE owl-cpu_INSTALL_CMAKEDIR PROPERTY TYPE PATH)
mark_as_advanced(owl-cpu_INSTALL_CMAKEDIR)

install(
    FILES cmake/install-config.cmake
    DESTINATION "${owl-cpu_INSTALL_CMAKEDIR}"
    RENAME "${package}Config.cmake"
    COMPONENT owl-cpu_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${owl-cpu_INSTALL_CMAKEDIR}"
    COMPONENT owl-cpu_Development
)

install(
    EXPORT owl-cpuTargets
    NAMESPACE owl-cpu::
    DESTINATION "${owl-cpu_INSTALL_CMAKEDIR}"
    COMPONENT owl-cpu_Development
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
