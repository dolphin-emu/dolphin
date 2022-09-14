if (NOT MSVC)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(SRCS
  imgui.cpp
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
)

add_library(imgui STATIC ${SRCS})
dolphin_disable_warnings_msvc(imgui)
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(imgui
PRIVATE
  fmt::fmt
)
