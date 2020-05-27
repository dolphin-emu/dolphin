
project(witest)
add_executable(witest)

target_sources(witest PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ComTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../FileSystemTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ResourceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ResultTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../Rpc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../SafeCastTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../StlTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TokenHelpersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WatcherTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinRTTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WistdTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../wiTest.cpp
    )
