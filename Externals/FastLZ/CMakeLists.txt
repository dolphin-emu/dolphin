add_library(FastLZ STATIC fastlz.c fastlz.h)

target_include_directories(FastLZ PUBLIC .)
