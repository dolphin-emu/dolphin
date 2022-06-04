if (NOT MSVC)
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(SRCS
  source/ChannelMaps.cpp
  source/KissFFT.cpp
  source/KissFFTR.cpp
  source/FreeSurroundDecoder.cpp
)

add_library(FreeSurround STATIC ${SRCS})
dolphin_disable_warnings_msvc(FreeSurround)
target_include_directories(FreeSurround PUBLIC include)
target_compile_options(FreeSurround PRIVATE -w)
