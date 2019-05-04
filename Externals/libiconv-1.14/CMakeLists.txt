include_directories(include)
include_directories(libcharset/include)

set(SRCS
  lib/iconv.c
  lib/relocatable.c
  libcharset/lib/localcharset.c
)

add_library(iconv STATIC ${SRCS})

set_target_properties(iconv PROPERTIES FOLDER Externals)
disable_all_warnings(iconv)
