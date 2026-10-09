dolphin_fetchcontent_declare(
  miniupnpc
  GIT_REPOSITORY https://github.com/miniupnp/miniupnp.git
  GIT_TAG bf4215a7574f88aa55859db9db00e3ae58cf42d6 # miniupnpc_2_3_3
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR miniupnpc
)

set(UPNPC_BUILD_STATIC ON)
set(UPNPC_BUILD_SHARED OFF)
set(UPNPC_BUILD_TESTS OFF)
set(UPNPC_BUILD_SAMPLE OFF)
set(UPNPC_NO_INSTALL ON)

FetchContent_MakeAvailable(miniupnpc)

add_library(Miniupnpc::miniupnpc ALIAS libminiupnpc-static)
