include(CheckAndAddFlag)

# check_and_add_warning(<basename> <warning_name> [ERROR] [DEBUG_ONLY | RELEASE_ONLY])
#
# Add a compiler warning flag to the current scope.
#
# Can optionally add the flag to Debug or Release configurations only, use this when
# targeting multi-configuration generators like Visual Studio or Xcode.
# Release configurations means NOT Debug, so it will work for RelWithDebInfo or MinSizeRel too.
#
# If the flag is added successfully, the variables FLAG_C_<variable>_WARN/FLAG_CXX_<variable>_WARN and
# FLAG_C_<variable>_ERROR/FLAG_CXX_<variable>_ERROR may be set to ON.
#
# Examples:
#   check_and_add_warning(PEDANTIC pedantic)
#   check_and_add_warning(CONV conversion ERROR RELEASE_ONLY)
#   check_and_add_warning(SIGNCONV sign-conversion DEBUG_ONLY)
#   check_and_add_warning(SHADOW shadow)

function(check_and_add_warning basename warning_name)
  set(is_error OFF)
  
  if(ARGV2 STREQUAL "ERROR")
    set(is_error ON)
  elseif(ARGV2)
    message(FATAL_ERROR "check_and_add_warning called with incorrect arguments: ${ARGN}")
  endif()
  unset(ARGV2)

  set(compilation_profile ${ARGV3})

  set(warn_var "${basename}_WARN")
  set(err_var  "${basename}_ERROR")

  check_and_add_flag(${warn_var} "-W${warning_name}" ${compilation_profile})

  if(is_error)
    check_and_add_flag(${err_var} "-Werror=${warning_name}" ${compilation_profile})
  endif()
endfunction()
