add_library(MultipartParser STATIC
        MultipartParser.h
        MultipartReader.h
        MultipartParser.cpp)

target_include_directories(MultipartParser
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)