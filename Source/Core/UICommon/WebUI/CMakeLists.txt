add_library(webinterface
  AVEncoder.cpp
  AVEncoder.h
  AVStream.cpp
  AVStream.h
  AVStreamRTCPeer.cpp
  AVStreamRTCPeer.h
  WebServer.cpp
  WebServer.h
)

# The httplib and LibDataChannel APIs require some use of exceptions.
if(NOT MSVC)
  set_source_files_properties(
    AVStreamRTCPeer.cpp
    WebServer.cpp
    PROPERTIES COMPILE_OPTIONS "-fexceptions"
  )
endif()

target_link_libraries(webinterface PRIVATE
  common
  core
  FFmpeg::avcodec
  FFmpeg::avutil
  FFmpeg::swscale
  LibDataChannel::LibDataChannel
  httplib::httplib
  fmt::fmt
)
