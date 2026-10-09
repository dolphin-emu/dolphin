if (NOT MSVC)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

dolphin_fetchcontent_declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG 45acd5e0e82f4c954432533ae9985ff0e1aad6d5 # v1.92.2b
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(SRCS
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

add_library(imgui STATIC ${SRCS})

target_include_directories(imgui
  PUBLIC
    "${imgui_SOURCE_DIR}"
)

target_compile_definitions(imgui
  PUBLIC
    IMGUI_USER_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/imgui_user_config.h"
)

target_link_libraries(imgui
  PRIVATE
    fmt::fmt
)
