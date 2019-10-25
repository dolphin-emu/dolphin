option(DOLPHIN_CXX_FLAGS "Flags used to compile Dolphin-only sources" "")
if(DOLPHIN_CXX_FLAGS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DOLPHIN_CXX_FLAGS}")
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
  add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
  add_definitions(-D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
	# TODO: Use https://cmake.org/cmake/help/latest/policy/CMP0092.html instead (once we can require CMake >= 3.15)
  # Taken from http://www.cmake.org/Wiki/CMake_FAQ#Dynamic_Replace.
  foreach(flag_var
	        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
					MAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
		# Replaces /W3 with /W4 in defaults (add_compile_options would cause very annoying warnings here)
    string(REPLACE "/W3" "/W4" ${flag_var} "${${flag_var}}")
  endforeach()

  # Disable some warnings
  add_compile_options(
    /wd4201 # nonstandard extension used : nameless struct/union
    /wd4127 # conditional expression is constant
    /wd4100 # 'identifier' : unreferenced formal parameter
    /wd4200 # InputCommon fix temp.
    /wd4244 # 'conversion' conversion from 'type1' to 'type2', possible loss of data
    /wd4121 # 'symbol' : alignment of a member was sensitive to packing
    /wd4324 # Padding was added at the end of a structure because you specified a __declspec(align) value.
    /wd4714 # function 'function' marked as __forceinline not inlined
    /wd4351 # new behavior: elements of array 'array' will be default initialized
    # TODO: Enable this warning once possible
    /wd4245 # conversion from 'type1' to 'type2', signed/unsigned mismatch
    # Currently jits use some annoying code patterns which makes this common
  )

  # Additional warnings
  add_compile_options(
    /w44263 # Non-virtual member function hides base class virtual function
    /w44265 # Class has virtual functions, but destructor is not virtual
  )

  # Treat all warnings as errors
  add_compile_options(/WX)

  # All files are encoded as UTF-8
  add_compile_options(/utf-8)

  # Use PCH
  add_subdirectory(PCH)
  add_definitions(/I${PCH_DIRECTORY})
  add_definitions(/Yu${PCH_PATH}) 

	# Don't include timestamps in binaries
	add_link_options(/Brepro)
endif()

# These aren't actually needed for C11/C++11
# but some dependencies require them (LLVM, libav).
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)

add_subdirectory(Core)
if (ANDROID)
  add_subdirectory(Android/jni)
endif()

if (IOS)
  add_subdirectory(iOS/Interface)
endif()

if (ENABLE_TESTS)
  add_subdirectory(UnitTests)
endif()

if (DSPTOOL)
  add_subdirectory(DSPTool)
endif()

# TODO: Add DSPSpy. Preferably make it option() and cpack component
