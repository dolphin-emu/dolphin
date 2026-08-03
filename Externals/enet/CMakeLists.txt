add_subdirectory(enet)

add_library(enet::enet ALIAS enet)
target_include_directories(enet PUBLIC enet/include)
