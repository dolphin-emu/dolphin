set(PUGIXML_NO_EXCEPTIONS ON)
set(PUGIXML_STATIC_CRT OFF)
set(PUGIXML_BUILD_DEFINES OFF)
set(PUGIXML_INSTALL OFF)

add_subdirectory(pugixml)
dolphin_disable_warnings(pugixml-static)
