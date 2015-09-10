set(SRCS dis_decode.cc
         dis_groups.cc
         resolve.cc
         syntax.cc)

if(WIN32)
    set(SRCS ${SRCS} stdafx.cc)
endif(WIN32)

add_library(bdisasm STATIC ${SRCS})
