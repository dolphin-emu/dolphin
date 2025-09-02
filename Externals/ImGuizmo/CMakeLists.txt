if(NOT MSVC)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(SRCS
  ImGuizmo/GraphEditor.cpp
  ImGuizmo/ImCurveEdit.cpp
  ImGuizmo/ImGradient.cpp
  ImGuizmo/ImGuizmo.cpp
  ImGuizmo/ImSequencer.cpp
)

add_library(ImGuizmo STATIC ${SRCS})
dolphin_disable_warnings(ImGuizmo)
target_include_directories(ImGuizmo
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/ImGuizmo"
  PRIVATE imgui
)

target_link_libraries(ImGuizmo
  PRIVATE imgui
  PRIVATE fmt::fmt
)
