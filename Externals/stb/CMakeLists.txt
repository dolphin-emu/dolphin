add_library(stb STATIC stb_impl.cpp)

target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb)
target_compile_definitions(stb PUBLIC STBI_ONLY_JPEG)
