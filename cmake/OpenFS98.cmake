# ofs_step(<group> OUTPUTS <files...> COMMANDS <shell lines...>)
# One build step: its commands run in order, from the source root, under sh. Steps run in the Makefile's order. Re-run when a named input or a header in a named source directory
# is newer, or a .inc/.h beside a named tools/ source. Since Story 1.4 every module's .inc files are committed C beside its
# sources, so they are inputs too; the generators left read them or orig/ and write only into bin/. A .c that a named
# source #includes is not globbed: name it with DEPENDS_FILES (src/fe/fe_library.c, src/advdrv/advdrv_relocs.inc), which
# keeps the dependency if the file is removed (the build then fails instead of going stale). DEPENDS_FILES also names a
# generated file under bin/ (the token scan ignores it), which must be another step's output.
# Every step that runs tools/check_data_image.py depends on tools/data_images.txt.
string(REPLACE ";" " " OFS_MODULE_FLAGS_TEXT "${OFS_MODULE_FLAGS}")

# The World and SIMSUI unit lists live once, in src/<module>/sources.txt, which the break runners read too
# (split on whitespace, as Python's split() does): @WORLD_SOURCES@, @SIMSUI_SOURCES@ and @SIMSUI_OBJECTS@.
foreach(m IN ITEMS world simsui)
  string(TOUPPER "${m}" M)
  file(READ "${CMAKE_SOURCE_DIR}/src/${m}/sources.txt" t)
  string(REGEX MATCHALL "[^ \t\r\n]+" OFS_${M}_SOURCES "${t}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/${m}/sources.txt")
endforeach()
list(TRANSFORM OFS_SIMSUI_SOURCES REPLACE "^src/simsui/(.*)\\.c$" "bin/simsui-native/\\1.o" OUTPUT_VARIABLE OFS_SIMSUI_OBJECTS)
foreach(p IN ITEMS WORLD_SOURCES SIMSUI_SOURCES SIMSUI_OBJECTS)
  string(JOIN " " OFS_${p}_TEXT ${OFS_${p}})
endforeach()

# The AD-15 lint (Story 1.8): a target with no output, so it runs on every build, even an up-to-date tree. Every
# output-producing custom command below names it in DEPENDS, so no module recipe starts before it passes.
add_custom_target(ofs_arena_lint COMMAND python3 -B tools/check_arena.py
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" USES_TERMINAL VERBATIM)

function(ofs_step group)
  cmake_parse_arguments(S "" "" "OUTPUTS;COMMANDS;DEPENDS_GLOB;DEPENDS_FILES" ${ARGN})
  set(deps "${CMAKE_SOURCE_DIR}/cmake/toolchains/win32-retail.cmake")  # a flag change rebuilds every step
  set(dirs include/fs98)
  set(cmds "")
  foreach(c IN LISTS S_COMMANDS)
    string(REPLACE "@SEMI@" "$<SEMICOLON>" c "${c}")  # as in ofs_target: a bare ";" would split the list
    string(REPLACE "@MODULE_FLAGS@" "${OFS_MODULE_FLAGS_TEXT}" c "${c}")
    foreach(p IN ITEMS WORLD_SOURCES SIMSUI_SOURCES SIMSUI_OBJECTS)
      string(REPLACE "@${p}@" "${OFS_${p}_TEXT}" c "${c}")  # before the token scan, so each unit is an input
    endforeach()
    list(APPEND cmds COMMAND sh -c "${c}")
    if(c MATCHES "tools/check_data_image\\.py")
      list(APPEND deps "${CMAKE_SOURCE_DIR}/tools/data_images.txt")
    endif()
    string(REGEX MATCHALL "[A-Za-z0-9_./+-]+" toks "${c}")
    if(c MATCHES "^(PYTHONDONTWRITEBYTECODE=1 )?python3 ")
      # a generator names its outputs too: depend on its script and the orig/ file only
      list(FILTER toks INCLUDE REGEX "^(tools/[a-z0-9_]+\\.py|orig/.*)$")
    endif()
    foreach(t IN LISTS toks)
      if(t MATCHES "^(src|tools|orig|include)/" AND EXISTS "${CMAKE_SOURCE_DIR}/${t}" AND NOT IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${t}")
        list(APPEND deps "${CMAKE_SOURCE_DIR}/${t}")
        get_filename_component(d "${t}" DIRECTORY)
        if(d MATCHES "^src/")
          list(APPEND dirs "${d}")
        elseif(d MATCHES "^tools/" AND t MATCHES "\\.c$")
          # a harness's own headers and .inc files (hand-written, all in git), as the Makefile had them
          file(GLOB his CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/${d}/*.inc" "${CMAKE_SOURCE_DIR}/${d}/*.h")
          list(APPEND deps ${his})
        endif()
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES dirs)
  foreach(d IN LISTS dirs)
    file(GLOB hs CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/${d}/*.h" "${CMAKE_SOURCE_DIR}/${d}/*.inc")
    list(APPEND deps ${hs})
  endforeach()
  set(abs "")
  foreach(o IN LISTS S_OUTPUTS)
    list(APPEND abs "${CMAKE_SOURCE_DIR}/${o}")
  endforeach()
  # DEPENDS_GLOB: source-root-relative patterns, searched recursively (committed files a command reads that it does not name)
  foreach(g IN LISTS S_DEPENDS_GLOB)
    file(GLOB_RECURSE gs CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/${g}")
    list(APPEND deps ${gs})
  endforeach()
  # DEPENDS_FILES: source-root-relative files, unconditionally (a generated file may not exist yet: another step writes it)
  foreach(f IN LISTS S_DEPENDS_FILES)
    list(APPEND deps "${CMAKE_SOURCE_DIR}/${f}")
  endforeach()
  list(REMOVE_DUPLICATES deps)
  add_custom_command(OUTPUT ${abs} ${cmds} WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" DEPENDS ${deps} ofs_arena_lint VERBATIM)
  # Order by target, not by file: a step runs after the one before it (they share generated files),
  # but an edit to one module does not make every later step look out of date.
  list(LENGTH OFS_${group}_OUTPUTS n)
  add_custom_target(ofs_${group}_step_${n} DEPENDS ${abs})
  if(OFS_${group}_PREV)
    add_dependencies(ofs_${group}_step_${n} ${OFS_${group}_PREV})
  endif()
  set(OFS_${group}_PREV ofs_${group}_step_${n} PARENT_SCOPE)
  # a multi-output step is chained through its first output; the group target needs all of them
  set(OFS_${group}_OUTPUTS ${OFS_${group}_OUTPUTS} ${abs} PARENT_SCOPE)
endfunction()

# ofs_target(<name> [DEPENDS <targets>] COMMANDS <shell lines>)
# A make target ported by name: not part of `all`, runs its lines in order from the source root under sh.
function(ofs_target name)
  cmake_parse_arguments(T "" "" "DEPENDS;COMMANDS" ${ARGN})
  set(cmds "")
  foreach(c IN LISTS T_COMMANDS)
    string(REPLACE "@SEMI@" "$<SEMICOLON>" c "${c}")  # a bare ";" would split the list (here and in COMMAND)
    string(REPLACE "@MODULE_FLAGS@" "${OFS_MODULE_FLAGS_TEXT}" c "${c}")
    list(APPEND cmds COMMAND sh -c "${c}")
  endforeach()
  if(cmds)
    add_custom_target(${name} ${cmds} WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" USES_TERMINAL VERBATIM)
  else()
    add_custom_target(${name})  # a make target that only built things: the dependencies are the work
  endif()
  add_dependencies(${name} ofs_arena_lint)  # command targets too: a lint failure stops them before any compile
  if(T_DEPENDS)
    add_dependencies(${name} ${T_DEPENDS})
  endif()
endfunction()
