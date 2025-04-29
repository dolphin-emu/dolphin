# TODO: Replace DiscordRPC which has been deprecated with
#       the Discord Social SDK https://discord.com/developers/docs/rich-presence/overview
cmake_minimum_required (VERSION 3.6)
project (DiscordRPC)

include(GNUInstallDirs)

# format
file(GLOB_RECURSE ALL_SOURCE_FILES
    include/*.h
    src/*.cpp src/*.h src/*.c
)

# add subdirs

add_subdirectory(src)
