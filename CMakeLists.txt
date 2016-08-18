########################################
# General setup
#
cmake_minimum_required(VERSION 2.8.8)
project(dolphin-emu)

execute_process (
    COMMAND bash -c "find . -iwholename '*' -not -name CMakeLists.txt -delete"
    OUTPUT_VARIABLE outVar
)

enable_testing()		
add_custom_target(unittests)		
add_custom_command(TARGET unittests POST_BUILD COMMAND echo)

add_custom_target(install)		
add_custom_command(TARGET install POST_BUILD COMMAND echo)
