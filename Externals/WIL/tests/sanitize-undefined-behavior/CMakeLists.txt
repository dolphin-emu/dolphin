
add_executable(witest.ubsan)

target_compile_options(witest.ubsan PRIVATE
    -fsanitize=undefined
    -fno-sanitize-recover=undefined # So we get test failures
    )

target_compile_definitions(witest.ubsan PRIVATE
    -DWITEST_UB_SANITIZER # To conditionally enable/disable code
    )

# UBSan libraries were built assuming static linking
set_property(TARGET witest.ubsan
    PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")

target_sources(witest.ubsan PUBLIC
    ${COMMON_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../StlTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TokenHelpersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WatcherTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinRTTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinVerifyTrustTest.cpp
    )
