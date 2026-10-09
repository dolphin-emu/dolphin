dolphin_fetchcontent_declare(
  pugixml
  GIT_REPOSITORY https://github.com/zeux/pugixml.git
  GIT_TAG ee86beb30e4973f5feffe3ce63bfa4fbadf72f38 # v1.15
  GIT_SHALLOW TRUE
)

set(PUGIXML_NO_EXCEPTIONS ON)
set(PUGIXML_STATIC_CRT OFF)
set(PUGIXML_BUILD_DEFINES OFF)
set(PUGIXML_INSTALL OFF)

FetchContent_MakeAvailable(pugixml)
