
add_library(dolphin_libretro SHARED
  Boot.cpp
  Host.cpp
  Input.cpp
  Log.cpp
  Main.cpp
  Options.cpp
  Video.cpp
)

if (NOT APPLE)
   target_sources(dolphin_libretro PRIVATE
      Vulkan.cpp
   )
endif()

include_directories(${CMAKE_SOURCE_DIR}/Externals/Libretro/Include)
include_directories(${CMAKE_SOURCE_DIR}/Externals/Vulkan/Include)

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

set_target_properties(dolphin_libretro PROPERTIES PREFIX "")
if(ANDROID)
   set_target_properties(dolphin_libretro PROPERTIES SUFFIX "_android.so")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
   set(CLANG 1)
endif()

if(NOT MSVC AND NOT CLANG)
   set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
endif()

target_link_libraries(dolphin_libretro PRIVATE
  core
  uicommon
  ${LIBS}
)
