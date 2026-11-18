# wolfSSL's own CMake build is used with WOLFSSL_USER_SETTINGS: the whole configuration comes
# from user_settings.h. Only add the AES-NI assembly, which wolfSSL's CMake (5.9.4) never builds.

# Source archive of the release commit rather than a git clone: the repository is over 1 GiB.
dolphin_fetchcontent_declare(
  wolfssl
  # v5.9.4-stable
  URL https://github.com/wolfSSL/wolfssl/archive/3c5eead44904df64e6a5a1f4ebdce377d35a849a.tar.gz
  URL_HASH SHA256=20fceb75ecac0fdb241f6d683e8cd1b579db0297f6d506d13eca9656262d72a5
)

# wolfSSL's CMakeLists requires CMake 3.16, so its options would drop variables of the same name:
# set them as cache entries.
set(WOLFSSL_USER_SETTINGS yes CACHE INTERNAL "" FORCE)
set(WOLFSSL_EXAMPLES no CACHE INTERNAL "" FORCE)
set(WOLFSSL_CRYPT_TESTS no CACHE INTERNAL "" FORCE)
set(WOLFSSL_INSTALL no CACHE INTERNAL "" FORCE)
set(WOLFSSL_SYS_CA_CERTS no CACHE INTERNAL "" FORCE)
set(WOLFSSL_X86_64_BUILD_ASM no CACHE INTERNAL "" FORCE)
set(BUILD_SHARED_LIBS OFF)

set(WOLFSSL_AESNI_ASM OFF)
if(_M_X86_64)
  set(WOLFSSL_AESNI_ASM ON)
  if(MSVC)
    enable_language(ASM_MASM)
    # Restrict global MSVC options to C and C++ for wolfSSL (unknown for ml64 compiler).
    get_directory_property(dir_options COMPILE_OPTIONS)
    set(c_options)
    foreach(option IN LISTS dir_options)
      if(NOT option MATCHES "^[-/][Ww]")
        list(APPEND c_options "$<$<COMPILE_LANGUAGE:C,CXX>:${option}>")
      endif()
    endforeach()
    set_directory_properties(PROPERTIES COMPILE_OPTIONS "${c_options}")
  endif()
endif()

FetchContent_MakeAvailable(wolfssl)

target_compile_definitions(wolfssl INTERFACE WOLFSSL_USER_SETTINGS)
target_include_directories(wolfssl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

if(WOLFSSL_AESNI_ASM)
  if(MSVC)
    set(masm_sources
      "${wolfssl_SOURCE_DIR}/wolfcrypt/src/aes_x86_64_asm.asm"
      "${wolfssl_SOURCE_DIR}/wolfcrypt/src/aes_gcm_asm.asm"
    )
    target_sources(wolfssl PRIVATE ${masm_sources})
    set_source_files_properties(${masm_sources} TARGET_DIRECTORY wolfssl
      PROPERTIES LANGUAGE ASM_MASM)
  else()
    target_sources(wolfssl PRIVATE
      "${wolfssl_SOURCE_DIR}/wolfcrypt/src/aes_x86_64_asm.S"
      "${wolfssl_SOURCE_DIR}/wolfcrypt/src/aes_gcm_asm.S"
    )
    target_compile_options(wolfssl PRIVATE $<$<COMPILE_LANGUAGE:C>:-maes>)
  endif()
  set(aesni_value 1)
else()
  set(aesni_value 0)
endif()
# PUBLIC: changes sizeof(Aes), users of the headers must see the same value.
target_compile_definitions(wolfssl PUBLIC DOLPHIN_WOLFSSL_AESNI=${aesni_value})
