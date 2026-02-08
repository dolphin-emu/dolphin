
add_executable(witest.app)

target_compile_definitions(witest.app PRIVATE
    -DWINAPI_FAMILY=WINAPI_FAMILY_PC_APP
    )

target_sources(witest.app PRIVATE
    ${COMMON_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../StlTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinRTTests.cpp
    )
