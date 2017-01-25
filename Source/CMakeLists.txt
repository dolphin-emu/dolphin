check_and_add_flag(CXX14 -std=c++14)
if(NOT FLAG_CXX_CXX14)
  check_and_add_flag(HAS_CXX1Y -std=c++1y)
endif()

# These aren't actually needed for C11/C++11
# but some dependencies require them (LLVM, libav).
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)

macro(add_dolphin_library lib srcs libs)
	add_library(${lib} STATIC ${srcs})
	target_link_libraries(${lib} ${libs})
endmacro()

add_subdirectory(Core)
if (ANDROID)
	add_subdirectory(Android/jni)
endif()
add_subdirectory(UnitTests)

if (DSPTOOL)
	add_subdirectory(DSPTool)
endif()

# TODO: Add DSPSpy. Preferably make it option() and cpack component
