include(ExternalProject)

set(MOLTENVK_VERSION "v1.4.1")
set(MOLTENVK_ARCHIVE_HASH "5ea0c259df7ded9a275444820f09cced54d6e5a7c7a31d262de62a5cdb7e15cf")
set(MOLTENVK_ARCHIVE_URL "https://github.com/KhronosGroup/MoltenVK/releases/download/${MOLTENVK_VERSION}/MoltenVK-macos.tar")

ExternalProject_Add(MoltenVK
    URL "${MOLTENVK_ARCHIVE_URL}"
    URL_HASH "SHA256=${MOLTENVK_ARCHIVE_HASH}"

    PATCH_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""

    BUILD_BYPRODUCTS <SOURCE_DIR>/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib

    LOG_DOWNLOAD ON
)