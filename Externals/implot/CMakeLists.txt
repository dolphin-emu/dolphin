if(NOT MSVC)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

dolphin_fetchcontent_declare(
  implot
  GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG 4707b245fbcd69075b1a8a74fa8d2435561b3134 # v0.17
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(implot)

set(SRCS
  ${implot_SOURCE_DIR}/implot.cpp
  ${implot_SOURCE_DIR}/implot_items.cpp
)

add_library(implot STATIC ${SRCS})
target_include_directories(implot
  PUBLIC "${implot_SOURCE_DIR}"
  PRIVATE imgui
)

target_link_libraries(implot
  PRIVATE imgui
  PRIVATE fmt::fmt
)

# https://github.com/epezent/implot/pull/565
target_compile_options(implot PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/FI${CMAKE_CURRENT_SOURCE_DIR}/implot_isnan_fix.h>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_CURRENT_SOURCE_DIR}/implot_isnan_fix.h>
)
