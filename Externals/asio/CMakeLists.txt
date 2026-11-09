dolphin_fetchcontent_declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG 8806a6803cde7054c3049d3666d3ec36786568c5 # asio-1-38-2
)
FetchContent_MakeAvailable(asio)

add_library(asio INTERFACE)
target_include_directories(asio SYSTEM INTERFACE "${asio_SOURCE_DIR}/include")
target_compile_definitions(asio INTERFACE ASIO_NO_EXCEPTIONS)
