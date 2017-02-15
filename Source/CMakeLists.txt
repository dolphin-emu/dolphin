check_and_add_flag(CXX14 -std=c++14)
if(NOT FLAG_CXX_CXX14)
  check_and_add_flag(HAS_CXX1Y -std=c++1y)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DNOMINMAX)
  add_definitions(-DUNICODE)
  add_definitions(-D_UNICODE)
  add_definitions(-DWIN32_LEAN_AND_MEAN)
  add_definitions(-D_WIN32_WINNT=0x0602)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
endif()

# These aren't actually needed for C11/C++11
# but some dependencies require them (LLVM, libav).
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)

macro(add_dolphin_library lib srcs libs)
	add_library(${lib} STATIC ${srcs})
	target_link_libraries(${lib} PUBLIC ${libs})
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
