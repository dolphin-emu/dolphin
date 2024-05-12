option(DOLPHIN_CXX_FLAGS "Flags used to compile Dolphin-only sources" "")
if(DOLPHIN_CXX_FLAGS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DOLPHIN_CXX_FLAGS}")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DNOMINMAX)
  add_definitions(-DUNICODE)
  add_definitions(-D_UNICODE)
  add_definitions(-DWIN32_LEAN_AND_MEAN)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
  add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
  add_definitions(-D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

if (NOT MSVC)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
  # Compile PCH
  add_subdirectory(PCH)
else()
  check_and_add_flag(HAVE_WALL -Wall)
  # TODO: would like these but they produce overwhelming amounts of warnings
  #check_and_add_flag(EXTRA -Wextra)
  #check_and_add_flag(MISSING_FIELD_INITIALIZERS -Wmissing-field-initializers)
  #check_and_add_flag(SWITCH_DEFAULT -Wswitch-default)
  #check_and_add_flag(FLOAT_EQUAL -Wfloat-equal)
  #check_and_add_flag(CONVERSION -Wconversion)
  #check_and_add_flag(ZERO_AS_NULL_POINTER_CONSTANT -Wzero-as-null-pointer-constant)
  check_and_add_flag(TYPE_LIMITS -Wtype-limits)
  check_and_add_flag(SIGN_COMPARE -Wsign-compare)
  check_and_add_flag(IGNORED_QUALIFIERS -Wignored-qualifiers)
  check_and_add_flag(UNINITIALIZED -Wuninitialized)
  check_and_add_flag(LOGICAL_OP -Wlogical-op)
  check_and_add_flag(SHADOW -Wshadow)
  check_and_add_flag(SHADOW_FIELD_IN_CONSTRUCTOR -Wshadow-field-in-constructor)
  check_and_add_flag(SHADOW_UNCAPTURED_LOCAL -Wshadow-uncaptured-local)
  check_and_add_flag(INIT_SELF -Winit-self)
  check_and_add_flag(MISSING_DECLARATIONS -Wmissing-declarations)
  check_and_add_flag(MISSING_VARIABLE_DECLARATIONS -Wmissing-variable-declarations)

  # Disable -Wstringop-truncation warnings as they result in many false positives.
  # In most (all?) cases where std::strncpy is used, we want to fill the entire buffer
  # or match emulated code that also ignores the null terminator, so the warnings are not useful.
  # Given that Dolphin itself mostly uses std::string, they do not really help catch any bugs.
  check_cxx_compiler_flag(-Wstringop-truncation HAS_STRINGOP_TRUNCATION_WARNING)
  if (HAS_STRINGOP_TRUNCATION_WARNING)
    check_and_add_flag(NO_STRINGOP_TRUNCATION -Wno-stringop-truncation)
  endif()

  # Format string issues that the compiler can detect should be compile time errors.
  check_cxx_compiler_flag(-Wformat HAS_FORMAT_WARNING)
  if (HAS_FORMAT_WARNING)
    check_and_add_flag(FORMAT_WARNING_TO_ERROR -Werror=format)
  endif()
endif()

# These aren't actually needed for C11/C++11
# but some dependencies require them (LLVM, libav).
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)

add_subdirectory(Core)
if (ANDROID)
  add_subdirectory(Android/jni)
endif()

if (ENABLE_TESTS)
  add_subdirectory(UnitTests)
endif()

if (DSPTOOL)
  add_subdirectory(DSPTool)
endif()

# TODO: Add DSPSpy. Preferably make it option() and cpack component
