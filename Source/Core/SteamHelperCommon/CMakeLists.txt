add_library(SteamHelperCommon
  IpcConnection.h
  IpcConnection.cpp
  PipeEnd.h
  PipeEnd.cpp
)

if(WIN32)
  target_sources(SteamHelperCommon
  PRIVATE
    PipeEnd_Windows.cpp
  )
else()
  target_sources(SteamHelperCommon
  PRIVATE
    PipeEnd_Unix.cpp
  )
endif()

target_link_libraries(SteamHelperCommon
PUBLIC
  sfml-network
  sfml-system
)
