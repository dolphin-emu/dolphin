set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
set(NO_MEDIA OFF CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(PREFER_SYSTEM_LIB ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  libdatachannel
  GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
  # v0.24.6
  GIT_TAG 6b1e2e620f1e37f0eafeee702eaea0043cb305fd
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(libdatachannel)

if(NOT MSVC)
  target_compile_options(datachannel PRIVATE -fexceptions)
endif()
