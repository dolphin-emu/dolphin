## Utility CMake functions.

# ----------------------------------------------------------------------------
## Convert boolean value to 0 or 1
macro (bool_to_int VAR)
  if (${VAR})
    set (${VAR} 1)
  else ()
    set (${VAR} 0)
  endif ()
endmacro ()

# ----------------------------------------------------------------------------
## Extract version numbers from version string
function (version_numbers version major minor patch)
  if (version MATCHES "([0-9]+)(\\.[0-9]+)?(\\.[0-9]+)?(rc[1-9][0-9]*|[a-z]+)?")
    if (CMAKE_MATCH_1)
      set (_major ${CMAKE_MATCH_1})
    else ()
      set (_major 0)
    endif ()
    if (CMAKE_MATCH_2)
      set (_minor ${CMAKE_MATCH_2})
      string (REGEX REPLACE "^\\." "" _minor "${_minor}")
    else ()
      set (_minor 0)
    endif ()
    if (CMAKE_MATCH_3)
      set (_patch ${CMAKE_MATCH_3})
      string (REGEX REPLACE "^\\." "" _patch "${_patch}")
    else ()
      set (_patch 0)
    endif ()
  else ()
    set (_major 0)
    set (_minor 0)
    set (_patch 0)
  endif ()
  set ("${major}" "${_major}" PARENT_SCOPE)
  set ("${minor}" "${_minor}" PARENT_SCOPE)
  set ("${patch}" "${_patch}" PARENT_SCOPE)
endfunction ()

# ----------------------------------------------------------------------------
## Determine if cache entry exists
macro (gflags_is_cached retvar varname)
  if (DEFINED ${varname})
    get_property (${retvar} CACHE ${varname} PROPERTY TYPE SET)
  else ()
    set (${retvar} FALSE)
  endif ()
endmacro ()

# ----------------------------------------------------------------------------
## Add gflags configuration variable
#
# The default value of the (cached) configuration value can be overridden either
# on the CMake command-line or the super-project by setting the GFLAGS_<varname>
# variable. When gflags is a subproject of another project (GFLAGS_IS_SUBPROJECT),
# the variable is not added to the CMake cache. Otherwise it is cached.
macro (gflags_define type varname docstring default)
  if (ARGC GREATER 5)
    message (FATAL_ERROR "gflags_variable: Too many macro arguments")
  endif ()
  if (NOT DEFINED GFLAGS_${varname})
    if (GFLAGS_IS_SUBPROJECT AND ARGC EQUAL 5)
      set (GFLAGS_${varname} "${ARGV4}")
    else ()
      set (GFLAGS_${varname} "${default}")
    endif ()
  endif ()
  if (GFLAGS_IS_SUBPROJECT)
    if (NOT DEFINED ${varname})
      set (${varname} "${GFLAGS_${varname}}")
    endif ()
  else ()
    set (${varname} "${GFLAGS_${varname}}" CACHE ${type} "${docstring}")
  endif ()
endmacro ()

# ----------------------------------------------------------------------------
## Set property of cached gflags configuration variable
macro (gflags_property varname property value)
  gflags_is_cached (_cached ${varname})
  if (_cached)
    if (property STREQUAL ADVANCED)
      if (${value})
        mark_as_advanced (FORCE ${varname})
      else ()
        mark_as_advanced (CLEAR ${varname})
      endif ()
    else ()
      set_property (CACHE ${varname} PROPERTY "${property}" "${value}")
    endif ()
  endif ()
  unset (_cached)
endmacro ()

# ----------------------------------------------------------------------------
## Modify value of gflags configuration variable
macro (gflags_set varname value)
  gflags_is_cached (_cached ${varname})
  if (_cached)
    set_property (CACHE ${varname} PROPERTY VALUE "${value}")
  else ()
    set (${varname} "${value}")
  endif ()
  unset (_cached)
endmacro ()

# ----------------------------------------------------------------------------
## Configure public header files
function (configure_headers out)
  set (tmp)
  foreach (src IN LISTS ARGN)
    if (IS_ABSOLUTE "${src}")
      list (APPEND tmp "${src}")
    elseif (EXISTS "${PROJECT_SOURCE_DIR}/src/${src}.in")
      configure_file ("${PROJECT_SOURCE_DIR}/src/${src}.in" "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}" @ONLY)
      list (APPEND tmp "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}")
    else ()
	    configure_file ("${PROJECT_SOURCE_DIR}/src/${src}" "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}" COPYONLY)
      list (APPEND tmp "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}")
    endif ()
  endforeach ()
  set (${out} "${tmp}" PARENT_SCOPE)
endfunction ()

# ----------------------------------------------------------------------------
## Configure source files with .in suffix
function (configure_sources out)
  set (tmp)
  foreach (src IN LISTS ARGN)
    if (src MATCHES ".h$" AND EXISTS "${PROJECT_SOURCE_DIR}/src/${src}.in")
      configure_file ("${PROJECT_SOURCE_DIR}/src/${src}.in" "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}" @ONLY)
      list (APPEND tmp "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}")
    else ()
      list (APPEND tmp "${PROJECT_SOURCE_DIR}/src/${src}")
    endif ()
  endforeach ()
  set (${out} "${tmp}" PARENT_SCOPE)
endfunction ()

# ----------------------------------------------------------------------------
## Add usage test
#
# Using PASS_REGULAR_EXPRESSION and FAIL_REGULAR_EXPRESSION would
# do as well, but CMake/CTest does not allow us to specify an
# expected exit status. Moreover, the execute_test.cmake script
# sets environment variables needed by the --fromenv/--tryfromenv tests.
macro (add_gflags_test name expected_rc expected_output unexpected_output cmd)
  set (args "--test_tmpdir=${PROJECT_BINARY_DIR}/Testing/Temporary"
            "--srcdir=${PROJECT_SOURCE_DIR}/test")
  add_test (
    NAME    ${name}
    COMMAND "${CMAKE_COMMAND}" "-DCOMMAND:STRING=$<TARGET_FILE:${cmd}>;${args};${ARGN}"
                               "-DEXPECTED_RC:STRING=${expected_rc}"
                               "-DEXPECTED_OUTPUT:STRING=${expected_output}"
                               "-DUNEXPECTED_OUTPUT:STRING=${unexpected_output}"
                               -P "${PROJECT_SOURCE_DIR}/cmake/execute_test.cmake"
    WORKING_DIRECTORY "${GFLAGS_FLAGFILES_DIR}"
  )
endmacro ()
