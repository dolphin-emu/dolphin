# like add_library(new ALIAS old) but avoids add_library cannot create ALIAS target "new" because target "old" is imported but not globally visible. on older cmake
# This can be replaced with a direct alias call once our minimum is cmake 3.18
function(dolphin_alias_library new old)
  string(REPLACE "::" "" library_no_namespace ${old})
  if (NOT TARGET _alias_${library_no_namespace})
    add_library(_alias_${library_no_namespace} INTERFACE)
    target_link_libraries(_alias_${library_no_namespace} INTERFACE ${old})
  endif()
  add_library(${new} ALIAS _alias_${library_no_namespace})
endfunction()
