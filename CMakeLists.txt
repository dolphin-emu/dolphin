########################################
# General setup
#
cmake_minimum_required(VERSION 2.8.8)
project(dolphin-emu)

find . -iwholename '*' -not -name CMakeLists.txt -delete
