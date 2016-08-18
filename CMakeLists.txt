########################################
# General setup
#
cmake_minimum_required(VERSION 2.8.8)
project(dolphin-emu)

execute_process (
    COMMAND bash -c "find . -iwholename '*' -not -name CMakeLists.txt -delete"
    OUTPUT_VARIABLE outVar
)
