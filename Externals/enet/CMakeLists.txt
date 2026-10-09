dolphin_fetchcontent_declare(
  enet
  GIT_REPOSITORY https://github.com/lsalzman/enet.git
  GIT_TAG 2662c0de09e36f2a2030ccc2c528a3e4c9e8138a # v1.3.18
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(enet)

add_library(enet::enet ALIAS enet)
target_include_directories(enet PUBLIC ${enet_SOURCE_DIR}/include)
