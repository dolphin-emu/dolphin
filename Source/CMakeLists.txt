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
  check_and_add_warning(HAVE_WALL all)
  # TODO: would like these but they produce overwhelming amounts of warnings
  #check_and_add_warning(EXTRA extra)
  #check_and_add_warning(MISSING_FIELD_INITIALIZERS missing-field-initializers)
  #check_and_add_warning(SWITCH_DEFAULT switch-default)
  #check_and_add_warning(FLOAT_EQUAL float-equal)
  #check_and_add_warning(CONVERSION conversion)
  #check_and_add_warning(ZERO_AS_NULL_POINTER_CONSTANT zero-as-null-pointer-constant)
  check_and_add_warning(TYPE_LIMITS type-limits ERROR)
  check_and_add_warning(SIGN_COMPARE sign-compare ERROR)
  check_and_add_warning(IGNORED_QUALIFIERS ignored-qualifiers ERROR)
  check_and_add_warning(UNINITIALIZED uninitialized ERROR)
  check_and_add_warning(LOGICAL_OP logical-op ERROR)
  check_and_add_warning(SHADOW shadow ERROR)
  check_and_add_warning(SHADOW_FIELD_IN_CONSTRUCTOR shadow-field-in-constructor ERROR)
  check_and_add_warning(INIT_SELF init-self ERROR)
  check_and_add_warning(MISSING_DECLARATIONS missing-declarations ERROR)
  check_and_add_warning(MISSING_VARIABLE_DECLARATIONS missing-variable-declarations ERROR)
  check_and_add_warning(FORMAT format ERROR)

  # Disable -Wstringop-truncation warnings as they result in many false positives.
  # In most (all?) cases where std::strncpy is used, we want to fill the entire buffer
  # or match emulated code that also ignores the null terminator, so the warnings are not useful.
  # Given that Dolphin itself mostly uses std::string, they do not really help catch any bugs.
  check_cxx_compiler_flag(-Wstringop-truncation HAS_STRINGOP_TRUNCATION_WARNING)
  if (HAS_STRINGOP_TRUNCATION_WARNING)
    check_and_add_flag(NO_STRINGOP_TRUNCATION -Wno-stringop-truncation)
  endif()
endif()

# These aren't actually needed for C11/C++11
# but some dependencies require them (LLVM, libav).
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)

check_symbol_exists(elf_aux_info sys/auxv.h HAVE_ELF_AUX_INFO)
if(HAVE_ELF_AUX_INFO)
    add_definitions(-DHAVE_ELF_AUX_INFO)
endif()

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
