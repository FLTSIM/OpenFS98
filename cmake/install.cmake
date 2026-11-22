# ofs_install / ofs_revert: the retired Makefile's `install` and `revert`. (`install` is a reserved CMake target.)
# Each file is copied to <name>.openfs98 and then renamed over the target.
# Each list is <name>;<installed path under FS98_DIR>;<retail file under orig/>, from tools/modules.txt.
set(OFS_MODULES "")
set(OFS_SHARED_AND_ENGLISH "")
file(STRINGS "${CMAKE_SOURCE_DIR}/tools/modules.txt" ofs_module_lines REGEX "^[^#]")
foreach(line IN LISTS ofs_module_lines)
  string(REGEX REPLACE "[ \t]+" ";" f "${line}")
  list(GET f 0 name)
  list(GET f 1 kind)
  list(GET f 2 path)
  list(GET f 3 retail)
  if(kind STREQUAL "module")
    list(APPEND OFS_MODULES ${name} ${path} ${retail})
  elseif(kind STREQUAL "english" OR kind STREQUAL "shared")
    list(APPEND OFS_SHARED_AND_ENGLISH ${name} ${path} ${retail})
  endif()
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tools/modules.txt")

# FS98_DIR in the environment, when set, overrides the configured one (as `make install FS98_DIR=...` did).
add_custom_target(ofs_install
  COMMAND ${CMAKE_COMMAND} -DMODE=install -DFS98_DIR=${FS98_DIR} -DSRC=${CMAKE_SOURCE_DIR}
          "-DOFS_MODULES=${OFS_MODULES}" "-DOFS_ENGLISH=${OFS_SHARED_AND_ENGLISH}" -P ${CMAKE_SOURCE_DIR}/cmake/install_run.cmake
  COMMAND sh -c "python3 -B tools/stage_gauges.py \"\${FS98_DIR:-${FS98_DIR}}\""
  COMMAND sh -c "python3 -B tools/stage_mdls.py \"\${FS98_DIR:-${FS98_DIR}}\" --all"
  COMMAND sh -c "python3 -B tools/stage_sim1.py \"\${FS98_DIR:-${FS98_DIR}}\""
  DEPENDS openfs98 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
add_custom_target(ofs_revert
  COMMAND ${CMAKE_COMMAND} -DMODE=revert -DFS98_DIR=${FS98_DIR} -DSRC=${CMAKE_SOURCE_DIR}
          "-DOFS_MODULES=${OFS_MODULES}" "-DOFS_ENGLISH=${OFS_SHARED_AND_ENGLISH}" -P ${CMAKE_SOURCE_DIR}/cmake/install_run.cmake
  COMMAND sh -c "python3 -B tools/stage_gauges.py \"\${FS98_DIR:-${FS98_DIR}}\" --retail"
  COMMAND sh -c "python3 -B tools/stage_mdls.py \"\${FS98_DIR:-${FS98_DIR}}\" --all --retail"
  COMMAND sh -c "python3 -B tools/stage_sim1.py \"\${FS98_DIR:-${FS98_DIR}}\" --retail"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
