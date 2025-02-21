set(SRCS
	SPIRV-Cross/GLSL.std.450.h
	SPIRV-Cross/spirv.h
	SPIRV-Cross/spirv.hpp
	SPIRV-Cross/spirv_cfg.cpp
	SPIRV-Cross/spirv_cfg.hpp
	SPIRV-Cross/spirv_common.hpp
	SPIRV-Cross/spirv_cpp.cpp
	SPIRV-Cross/spirv_cpp.hpp
	SPIRV-Cross/spirv_cross.cpp
	SPIRV-Cross/spirv_cross.hpp
	SPIRV-Cross/spirv_cross_c.cpp
	SPIRV-Cross/spirv_cross_c.h
	SPIRV-Cross/spirv_cross_containers.hpp
	SPIRV-Cross/spirv_cross_error_handling.hpp
	SPIRV-Cross/spirv_cross_parsed_ir.cpp
	SPIRV-Cross/spirv_cross_parsed_ir.hpp
	SPIRV-Cross/spirv_cross_util.cpp
	SPIRV-Cross/spirv_cross_util.hpp
	SPIRV-Cross/spirv_glsl.cpp
	SPIRV-Cross/spirv_glsl.hpp
	SPIRV-Cross/spirv_hlsl.cpp
	SPIRV-Cross/spirv_hlsl.hpp
	SPIRV-Cross/spirv_msl.cpp
	SPIRV-Cross/spirv_msl.hpp
	SPIRV-Cross/spirv_parser.cpp
	SPIRV-Cross/spirv_parser.hpp
	SPIRV-Cross/spirv_reflect.cpp
	SPIRV-Cross/spirv_reflect.hpp
)

if(NOT MSVC)
# spirv_cross requires C++11 at a minimum to compile.
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

add_library(spirv_cross STATIC ${SRCS})
dolphin_disable_warnings(spirv_cross)

target_compile_definitions(spirv_cross PUBLIC SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
target_include_directories(spirv_cross PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Cross/include ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Cross)
