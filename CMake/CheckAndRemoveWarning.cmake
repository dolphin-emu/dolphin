include(CheckAndAddFlag)

# check_and_remove_warning(<variable> <warning_name> [DEBUG_ONLY | RELEASE_ONLY])
#
# Remove a compiler warning flag from the current scope.
#
# Can optionally add the flag to Debug or Release configurations only.
# Release configurations means NOT Debug, so it will work for RelWithDebInfo or MinSizeRel too.
#
# If the flag is added successfully, the variables FLAG_C_<variable>/FLAG_CXX_<variable>
# may be set to ON.
#
# Examples:
#   check_and_remove_warning(NO_TRIGRAPHS trigraphs)
#   check_and_remove_warning(NO_UNUSED unused DEBUG_ONLY)

function(check_and_remove_warning variable warning_name)
  set(compilation_profile ${ARGV2})
  check_and_add_flag(${variable} "-Wno-${warning_name}" ${compilation_profile})
endfunction()
