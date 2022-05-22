set(SRCS
	glslang/GenericCodeGen/CodeGen.cpp
	glslang/GenericCodeGen/Link.cpp
	#glslang/HLSL/hlslAttributes.cpp
	#glslang/HLSL/hlslGrammar.cpp
	#glslang/HLSL/hlslOpMap.cpp
	#glslang/HLSL/hlslParseables.cpp
	#glslang/HLSL/hlslParseHelper.cpp
	#glslang/HLSL/hlslScanContext.cpp
	#glslang/HLSL/hlslTokenStream.cpp
	glslang/MachineIndependent/attribute.cpp
	glslang/MachineIndependent/Constant.cpp
	glslang/MachineIndependent/glslang_tab.cpp
	glslang/MachineIndependent/InfoSink.cpp
	glslang/MachineIndependent/Initialize.cpp
	glslang/MachineIndependent/Intermediate.cpp
	glslang/MachineIndependent/intermOut.cpp
	glslang/MachineIndependent/IntermTraverse.cpp
	glslang/MachineIndependent/iomapper.cpp
	glslang/MachineIndependent/limits.cpp
	glslang/MachineIndependent/linkValidate.cpp
	glslang/MachineIndependent/parseConst.cpp
	glslang/MachineIndependent/ParseContextBase.cpp
	glslang/MachineIndependent/ParseHelper.cpp
	glslang/MachineIndependent/PoolAlloc.cpp
	glslang/MachineIndependent/preprocessor/Pp.cpp
	glslang/MachineIndependent/preprocessor/PpAtom.cpp
	glslang/MachineIndependent/preprocessor/PpContext.cpp
	glslang/MachineIndependent/preprocessor/PpScanner.cpp
	glslang/MachineIndependent/preprocessor/PpTokens.cpp
	glslang/MachineIndependent/propagateNoContraction.cpp
	glslang/MachineIndependent/reflection.cpp
	glslang/MachineIndependent/RemoveTree.cpp
	glslang/MachineIndependent/Scan.cpp
	glslang/MachineIndependent/ShaderLang.cpp
	glslang/MachineIndependent/SpirvIntrinsics.cpp
	glslang/MachineIndependent/SymbolTable.cpp
	glslang/MachineIndependent/Versions.cpp
	OGLCompilersDLL/InitializeDll.cpp
	SPIRV/disassemble.cpp
	SPIRV/doc.cpp
	SPIRV/GlslangToSpv.cpp
	SPIRV/InReadableOrder.cpp
	SPIRV/Logger.cpp
	SPIRV/SpvBuilder.cpp
	SPIRV/SpvPostProcess.cpp
	SPIRV/SPVRemapper.cpp
	SPIRV/SpvTools.cpp
	StandAlone/ResourceLimits.cpp
)

if(WIN32)
	list(APPEND SRCS 	glslang/OSDependent/Windows/ossource.cpp)
else()
	list(APPEND SRCS 	glslang/OSDependent/Unix/ossource.cpp)
endif()

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

add_library(glslang STATIC ${SRCS})
dolphin_disable_warnings_msvc(glslang)

target_include_directories(glslang
PRIVATE
	${CMAKE_SOURCE_DIR}/Externals/glslang
)
