if (NOT MSVC)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(SRCS
  imgui/imgui.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
)

add_library(imgui STATIC ${SRCS})
dolphin_disable_warnings(imgui)

target_include_directories(imgui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
)

target_compile_definitions(imgui
  PUBLIC
    IMGUI_USER_CONFIG="imgui_user_config.h"
)

target_link_libraries(imgui
PRIVATE
  fmt::fmt
)
