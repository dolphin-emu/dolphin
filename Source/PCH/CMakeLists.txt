if(ENABLE_PCH)
  add_library(build_pch STATIC pch.cpp pch.h)
  target_include_directories(build_pch PRIVATE ${PROJECT_SOURCE_DIR}/Source/Core)

  target_link_libraries(build_pch PRIVATE fmt::fmt)
  target_precompile_headers(build_pch PRIVATE pch.h)
  
  add_library(use_pch INTERFACE)
  target_precompile_headers(use_pch INTERFACE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>")
endif()

function(dolphin_shared_pch target)
  if(NOT ENABLE_PCH)
    return()
  endif()

  target_precompile_headers(${target} REUSE_FROM build_pch)

  if(CMAKE_OBJCXX_COMPILER_LOADED)
    get_target_property(_sources ${target} SOURCES)
    list(FILTER _sources INCLUDE REGEX "\\.mm$")
    if(_sources)
      set_source_files_properties(${_sources} PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)
    endif()
  endif()
endfunction()

# Use this for targets where compile flags differ from the shared ones
# (e.g. linking Qt adds -mno-direct-extern-access on GCC).
function(dolphin_pch target)
  if(ENABLE_PCH)
    target_link_libraries(${target} PRIVATE use_pch)
  endif()
endfunction()
