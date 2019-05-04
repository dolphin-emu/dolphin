cmake_minimum_required (VERSION 3.2.0)
project (DiscordRPC)

include(GNUInstallDirs)

# format
file(GLOB_RECURSE ALL_SOURCE_FILES
    include/*.h
    src/*.cpp src/*.h src/*.c
)

# add subdirs

add_subdirectory(src)
set_target_properties(discord-rpc PROPERTIES FOLDER Externals)
disable_all_warnings(discord-rpc)
