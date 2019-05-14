add_library(png STATIC
  png.h
  pngconf.h
  png.c
  pngerror.c
  pngget.c
  pngmem.c
  pngpread.c
  pngread.c
  pngrio.c
  pngrtran.c
  pngrutil.c
  pngset.c
  pngtrans.c
  pngwio.c
  pngwrite.c
  pngwtran.c
  pngwutil.c
)

target_include_directories(png
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(png PUBLIC z)

if(NOT MSVC)
  target_compile_options(png
  PRIVATE
    -Wno-self-assign
  )
endif()
