set(UPNPC_BUILD_STATIC ON)
set(UPNPC_BUILD_SHARED OFF)
set(UPNPC_BUILD_TESTS OFF)
set(UPNPC_BUILD_SAMPLE OFF)
set(UPNPC_NO_INSTALL ON)

add_subdirectory(miniupnp/miniupnpc)

add_library(Miniupnpc::miniupnpc ALIAS libminiupnpc-static)
