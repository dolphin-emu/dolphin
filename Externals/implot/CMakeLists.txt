if(NOT MSVC)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(SRCS
  implot/implot.cpp
  implot/implot_items.cpp
)

add_library(implot STATIC ${SRCS})
dolphin_disable_warnings(implot)
target_include_directories(implot
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/implot"
  PRIVATE imgui
)

target_link_libraries(implot
  PRIVATE imgui
  PRIVATE fmt::fmt
)
