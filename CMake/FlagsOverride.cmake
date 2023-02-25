if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # The default MSVC flags for Release and RelWithDebInfo are poorly chosen
  # (see issue https://gitlab.kitware.com/cmake/cmake/-/issues/20812)
  # By default, inlining is disabled for RelWithDebInfo.
  # Manually redefine MSVC flags to match Visual Studio defaults
  # and ensure that Release builds generate debug info.
  foreach(f CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELEASE)
    # optimize, define NDEBUG, generate debug info
    set(${f} "/O2 /DNDEBUG /Z7")
  endforeach()
endif()
