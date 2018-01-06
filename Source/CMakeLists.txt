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

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  # enable the latest C++ standard feature set,
  # and also disable MSVC specific extensions
  # to be even more standards compliant.
  check_and_add_flag(CPPLATEST /std:c++latest)
  check_and_add_flag(STANDARD_COMPLIANCE /permissive-)
else()
  # Enable C++17, but fall back to C++14 if it isn't available.
  # CMAKE_CXX_STANDARD cannot be used here because we require C++14 or newer, not any standard.
  check_and_add_flag(CXX17 -std=c++17)
  if(NOT FLAG_CXX_CXX17)
    check_and_add_flag(CXX1Z -std=c++1z)
    if (NOT FLAG_CXX_CXX1Z)
      set(CMAKE_CXX_STANDARD 14)
      set(CMAKE_CXX_STANDARD_REQUIRED ON)
      set(CMAKE_CXX_EXTENSIONS OFF)
    endif()
  endif()

  # These compat headers must not be in the include path when building with MSVC,
  # because it currently does not support __has_include_next / #include_next.
  include_directories(SYSTEM Core/Common/Compat)
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
