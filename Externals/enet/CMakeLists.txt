add_subdirectory(enet)

dolphin_disable_warnings(enet)
add_library(enet::enet ALIAS enet)
target_include_directories(enet PUBLIC enet/include)
