########################################
# General setup
#
cmake_minimum_required(VERSION 2.8.8)
project(dolphin-emu)

execute_process (
    COMMAND bash -c "find . -iwholename '*' -not -name CMakeLists.txt -delete; mkdir -p Binaries; mkdir -p build/Binaries; mkdir -p build/build/Binaries; touch Binaries/Dolphin.app; touch build/Binaries/Dolphin.app; touch build/build/Binaries/Dolphin.app"
    OUTPUT_VARIABLE outVar
)

enable_testing()		
add_custom_target(unittests)		
add_custom_command(TARGET unittests POST_BUILD COMMAND echo)

add_custom_target(install)		
add_custom_command(TARGET install POST_BUILD COMMAND echo)
