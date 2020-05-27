# When packaging Dolphin for an OS distribution, distro vendors usually prefer
# to limit vendored ("Externals") dependencies as much as possible, in favor of
# using system provided libraries. This modules provides an option to allow
# only specific vendored dependencies and error-out at configuration time for
# non-approved ones.
#
# Usage:
#  $ cmake -D APPROVED_VENDORED_DEPENDENCIES="a;b;c;..."
#
# Unless the option is explicitly used, vendored dependencies control is
# disabled.
#
# If you want to disallow all vendored dependencies, put "none" in the approved
# dependencies list.

set(APPROVED_VENDORED_DEPENDENCIES "" CACHE STRING "\
Semicolon separated list of approved vendored dependencies. See docstring in \
CMake/CheckVendoringApproved.cmake.")

function(check_vendoring_approved dep)
  if(APPROVED_VENDORED_DEPENDENCIES)
    if(NOT dep IN_LIST APPROVED_VENDORED_DEPENDENCIES)
      message(SEND_ERROR "\
Library ${dep} was not found systemwide and was not approved for vendoring. \
Vendored dependencies control is enabled. Add \"${dep}\" to the \
APPROVED_VENDORED_DEPENDENCIES list to bypass this error.")
    endif()
  endif()
endfunction()
