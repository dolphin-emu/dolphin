# Add a C or C++ compile definitions to the current scope
#
# dolphin_compile_definitions(def [def ...] [DEBUG_ONLY | RELEASE_ONLY])
#
# Can optionally add the definitions to Debug or Release configurations only, use this so we can
# target multi-configuration generators like Visual Studio or Xcode.
# Release configurations means NOT Debug, so it will work for RelWithDebInfo or MinSizeRel too.
# The definitions are added to the COMPILE_DEFINITIONS folder property.
# Supports generator expressions, unlike add_definitions()
#
# Examples:
#   dolphin_compile_definitions(FOO) -> -DFOO
#   dolphin_compile_definitions(_DEBUG DEBUG_ONLY) -> -D_DEBUG
#   dolphin_compile_definitions(NDEBUG RELEASE_ONLY) -> -DNDEBUG
#   dolphin_compile_definitions($<$<COMPILE_LANGUAGE:C>:THISISONLYFORC>)

function(dolphin_compile_definitions)
  set(defs ${ARGN})

  list(GET defs -1 last_def)
  list(REMOVE_AT defs -1)

  set(genexp_config_test "1")
  if(last_def STREQUAL "DEBUG_ONLY")
    set(genexp_config_test "$<CONFIG:Debug>")
  elseif(last_def STREQUAL "RELEASE_ONLY")
    set(genexp_config_test "$<NOT:$<CONFIG:Debug>>")
  else()
    list(APPEND defs ${last_def})
  endif()

  set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    "$<${genexp_config_test}:${defs}>")
endfunction()
