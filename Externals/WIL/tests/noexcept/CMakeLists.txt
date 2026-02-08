
add_executable(witest.noexcept)

# Turn off exceptions for this test
replace_cxx_flag("/EHsc" "/EHs-c-")
target_compile_definitions(witest.noexcept PRIVATE
    -DCATCH_CONFIG_DISABLE_EXCEPTIONS
    )

# Catch2 has a no exceptions mode (configured above), however still includes STL headers which contain try...catch
# statements... Thankfully MSVC just gives us a warning that we can disable
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(witest.noexcept PRIVATE /wd4530)
endif()

target_sources(witest.noexcept PRIVATE
    ${COMMON_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../TokenHelpersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WatcherTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinRTTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinVerifyTrustTest.cpp
    )
