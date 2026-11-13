add_library(asio INTERFACE)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ASIO QUIET IMPORTED_TARGET asio>=1.32.0)
endif()
if(ASIO_FOUND)
    target_link_libraries(asio INTERFACE PkgConfig::ASIO)
else()
    find_package(Git REQUIRED)
    dolphin_fetchcontent_declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG 8806a6803cde7054c3049d3666d3ec36786568c5 # asio-1-38-2
    )
    FetchContent_MakeAvailable(asio)

    target_include_directories(asio SYSTEM INTERFACE "${asio_SOURCE_DIR}/include")
endif()

target_compile_definitions(asio INTERFACE ASIO_NO_EXCEPTIONS)
