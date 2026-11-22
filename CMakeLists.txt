cmake_minimum_required(VERSION 3.28)
project(OpenFS98 C)

# The generators, windres and the verification scripts all write to bin/ under the source root.
# CMake therefore builds into bin/ too, exactly as the retired Makefile did.
if(NOT OFS_TOOLCHAIN STREQUAL "win32-retail")
  message(FATAL_ERROR "Only win32-retail exists so far: configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/win32-retail.cmake")
endif()
if(NOT IS_DIRECTORY "${CMAKE_SOURCE_DIR}/orig")
  message(FATAL_ERROR "orig/ is missing: it holds the retail CD files the build reads. See docs/building.md ('Which CD').")
endif()

# The semantic flag set (OFS_MODULE_FLAGS, from the toolchain file) for anything that links OpenFS98::module.
add_library(openfs98_module INTERFACE)
add_library(OpenFS98::module ALIAS openfs98_module)
target_compile_options(openfs98_module INTERFACE ${OFS_MODULE_FLAGS})

set(FS98_DIR "$ENV{HOME}/.wine/drive_c/FS98" CACHE PATH "Game tree for ofs_install / ofs_revert")
include(cmake/OpenFS98.cmake)
include(cmake/steps/all.cmake)
add_custom_target(openfs98 ALL DEPENDS ${OFS_all_OUTPUTS})
add_dependencies(openfs98 ${OFS_all_PREV})

# Non-default: cmake --build <dir> --target difftests (runs after the `all` steps, as they share generated files)
set(OFS_difftests_PREV "${OFS_all_PREV}")
include(cmake/steps/difftests.cmake)
add_custom_target(difftests DEPENDS ${OFS_difftests_OUTPUTS})
add_dependencies(difftests ${OFS_difftests_PREV})

include(cmake/install.cmake)
include(cmake/targets.cmake)
