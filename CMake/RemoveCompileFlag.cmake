# from https://stackoverflow.com/a/49216539
# The linked answer does some weird preconfiguring by manually splitting the CMAKE_CXX_FLAGS variable and applying it to a target,
# but as far as I can tell none of that is necessary, this works just fine as-is.

#
# Removes the specified compile flag from the specified target.
#   _target     - The target to remove the compile flag from
#   _flag       - The compile flag to remove
#
macro(remove_cxx_flag_from_target _target _flag)
    get_target_property(_target_cxx_flags ${_target} COMPILE_OPTIONS)
    if(_target_cxx_flags)
        list(REMOVE_ITEM _target_cxx_flags ${_flag})
        set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "${_target_cxx_flags}")
    endif()
endmacro()
