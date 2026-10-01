add_library(webinterface
  AVStream.cpp
  AVStream.h
  AVEncoder.cpp
  AVEncoder.h
  WebServer.cpp
  WebServer.h
)

# The httplib and LibDataChannel APIs require some use of exceptions.
if(NOT MSVC)
  set_source_files_properties(
    AVStream.cpp
    WebServer.cpp
    PROPERTIES COMPILE_OPTIONS "-fexceptions"
  )
endif()

target_link_libraries(webinterface PRIVATE LibDataChannel::LibDataChannel)
target_link_libraries(webinterface PRIVATE httplib::httplib)
