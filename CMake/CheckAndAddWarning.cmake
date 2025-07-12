include(CheckAndAddFlag)

# check_and_add_warning(<variable> <warning_name> [ERROR | WARNING] [DEBUG_ONLY | RELEASE_ONLY])
#
# Add a compiler warning flag to the current scope.
#
# The second optional argument controls whether the warning is treated as an error.
# It can be either ERROR or WARNING (default if omitted).
#
# Can optionally add the flag to Debug or Release configurations only.
# Release configurations means NOT Debug, so it will work for RelWithDebInfo or MinSizeRel too.
#
# If the flag is added successfully, the variables FLAG_C_<variable>_WARN/FLAG_CXX_<variable>_WARN and
# FLAG_C_<variable>_ERROR/FLAG_CXX_<variable>_ERROR may be set to ON.
#
# Examples:
#   check_and_add_warning(PEDANTIC pedantic)
#   check_and_add_warning(CONV conversion ERROR RELEASE_ONLY)
#   check_and_add_warning(SIGNCONV sign-conversion WARNING DEBUG_ONLY)
#   check_and_add_warning(SHADOW shadow)

function(check_and_add_warning variable warning_name)
  set(is_error OFF)
  
  if(ARGV2 STREQUAL "ERROR")
    set(is_error ON)
  elseif(ARGV2)
    if(NOT ARGV2 STREQUAL "WARNING")
      message(FATAL_ERROR "check_and_add_warning called with unknown second argument: ${ARGV2}")
    endif()
  endif()
  unset(ARGV2)

  set(compilation_profile ${ARGV3})

  set(warn_var "${variable}_WARN")
  set(err_var  "${variable}_ERROR")

  check_and_add_flag(${warn_var} "-W${warning_name}" ${compilation_profile})

  if(is_error)
    check_and_add_flag(${err_var} "-Werror=${warning_name}" ${compilation_profile})
  endif()
endfunction()
