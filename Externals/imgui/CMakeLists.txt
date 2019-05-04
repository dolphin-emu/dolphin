set(SRCS
  imgui.cpp
  imgui_draw.cpp
  imgui_widgets.cpp
)

add_library(imgui STATIC ${SRCS})
target_include_directories(imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(imgui PROPERTIES FOLDER Externals)
disable_all_warnings(imgui)
