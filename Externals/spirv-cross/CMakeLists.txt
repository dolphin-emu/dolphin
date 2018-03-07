set(SRCS
	spirv_cfg.cpp
	spirv_cpp.cpp
	spirv_cross.cpp
	spirv_cross_util.cpp
	spirv_glsl.cpp
	spirv_hlsl.cpp
	spirv_msl.cpp
)

if(NOT MSVC)
# glslang requires C++11 at a minimum to compile.
add_compile_options(-std=c++11)

# Silence some warnings that occur frequently to reduce noise in build logs.
add_compile_options(-Wno-shadow)
add_compile_options(-Wno-reorder)
add_compile_options(-Wno-sign-compare)
add_compile_options(-Wno-parentheses)
add_compile_options(-Wno-unused-variable)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	add_compile_options(-Wno-unused-but-set-variable)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	add_compile_options(-Wno-missing-variable-declarations)
endif()
endif()

add_library(spirv-cross STATIC ${SRCS})
target_compile_definitions(spirv-cross PRIVATE SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
target_include_directories(spirv-cross PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(spirv-cross PUBLIC SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
target_include_directories(spirv-cross PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})

