project(minizip C)

add_library(minizip STATIC
  mz.h
  mz_compat.c
  mz_compat.h
  mz_crypt.c
  mz_crypt.h
  mz_os.c
  mz_os.h
  mz_strm.c
  mz_strm.h
  mz_strm_buf.c
  mz_strm_buf.h
  mz_strm_mem.c
  mz_strm_mem.h
  mz_strm_os.h
  mz_strm_split.c
  mz_strm_split.h
  mz_strm_zlib.c
  mz_strm_zlib.h
  mz_zip.c
  mz_zip.h
  mz_zip_rw.c
  mz_zip_rw.h
  unzip.h
  zip.h
)
dolphin_disable_warnings_msvc(minizip)

if (UNIX)
  target_sources(minizip PRIVATE
    mz_os_posix.c
    mz_strm_os_posix.c
  )
endif()

if (WIN32)
  target_sources(minizip PRIVATE
    mz_os_win32.c
    mz_strm_os_win32.c
  )
endif()

target_include_directories(minizip PUBLIC .)

target_compile_definitions(minizip PRIVATE HAVE_ZLIB ZLIB_COMPAT MZ_ZIP_NO_CRYPTO MZ_ZIP_NO_ENCRYPTION)
if (UNIX)
  target_compile_definitions(minizip PRIVATE _POSIX_C_SOURCE=200112L)
  target_compile_definitions(minizip PRIVATE __USE_LARGEFILE64 _LARGEFILE64_SOURCE)
endif()

check_include_file(stdint.h HAVE_STDINT_H)
if (HAVE_STDINT_H)
  target_compile_definitions(minizip PRIVATE HAVE_STDINT_H)
endif()

check_include_file(inttypes.h HAVE_INTTYPES_H)
if (HAVE_INTTYPES_H)
  target_compile_definitions(minizip PRIVATE HAVE_INTTYPES_H)
endif()

check_function_exists(fseeko HAVE_FSEEKO)
if (NOT HAVE_FSEEKO)
  target_compile_definitions(minizip PRIVATE NO_FSEEKO)
endif()

target_link_libraries(minizip PUBLIC ZLIB::ZLIB)

add_library(minizip-ng ALIAS minizip)
