include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

# check_add_add_flag(<variable> <flag> [DEBUG_ONLY | RELEASE_ONLY])
#
# Add a C or C++ compilation flag to the current scope
#
# Can optionally add the flag to Debug or Release configurations only, use this when
# targeting multi-configuration generators like Visual Studio or Xcode.
# Release configurations means NOT Debug, so it will work for RelWithDebInfo or MinSizeRel too.
#
# If the flag is added successfully, the variables FLAG_C_<variable> and FLAG_CXX_<variable>
# may be set to ON.
#
# Examples:
#   check_and_add_flag(FOO -foo)
#   check_and_add_flag(ONLYDEBUG -onlydebug DEBUG_ONLY)
#   check_and_add_flag(OPTMAX -O9001 RELEASE_ONLY)

function(check_and_add_flag var flag)
  set(genexp_config_test "1")
  if(ARGV2 STREQUAL "DEBUG_ONLY")
    set(genexp_config_test "$<CONFIG:Debug>")
  elseif(ARGV2 STREQUAL "RELEASE_ONLY")
    set(genexp_config_test "$<NOT:$<CONFIG:Debug>>")
  elseif(ARGV2)
    message(FATAL_ERROR "check_and_add_flag called with incorrect arguments: ${ARGN}")
  endif()

  set(is_c "$<COMPILE_LANGUAGE:C>")
  set(is_cxx "$<COMPILE_LANGUAGE:CXX>")

  # The Visual Studio generators don't support COMPILE_LANGUAGE
  # So we fail all the C flags and only actually test CXX ones
  if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(is_c "0")
    set(is_cxx "1")
  endif()

  check_c_compiler_flag(${flag} FLAG_C_${var})
  if(FLAG_C_${var})
    add_compile_options("$<$<AND:${is_c},${genexp_config_test}>:${flag}>")
  endif()

  check_cxx_compiler_flag(${flag} FLAG_CXX_${var})
  if(FLAG_CXX_${var})
    add_compile_options("$<$<AND:${is_cxx},${genexp_config_test}>:${flag}>")
  endif()
endfunction()
