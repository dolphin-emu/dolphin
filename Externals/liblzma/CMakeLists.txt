dolphin_fetchcontent_declare(
  xz
  GIT_REPOSITORY https://github.com/tukaani-project/xz.git
  GIT_TAG d3e650e63c110e830fd5391e7f8b45df0b91d3da # v5.8.4
  GIT_SHALLOW TRUE
)

# Only build liblzma, with the LZMA1/LZMA2 filters used by DiscIO and minizip-ng.
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set(XZ_NLS OFF)
set(XZ_DOC OFF)
set(XZ_DOXYGEN OFF)
set(XZ_TOOL_XZ OFF)
set(XZ_TOOL_XZDEC OFF)
set(XZ_TOOL_LZMADEC OFF)
set(XZ_TOOL_LZMAINFO OFF)
set(XZ_TOOL_SCRIPTS OFF)
set(XZ_TOOL_SYMLINKS OFF)
set(XZ_TOOL_SYMLINKS_LZMA OFF)
set(XZ_MICROLZMA_ENCODER OFF)
set(XZ_MICROLZMA_DECODER OFF)
set(XZ_LZIP_DECODER OFF)
set(XZ_ENCODERS "lzma1;lzma2")
set(XZ_DECODERS "lzma1;lzma2")

FetchContent_MakeAvailable(xz)

# xz only adds the API headers as a PRIVATE include directory and relies on the install step
# to export them, so expose them to users of the in-tree target.
target_include_directories(liblzma INTERFACE
  "$<BUILD_INTERFACE:${xz_SOURCE_DIR}/src/liblzma/api>"
)

add_library(LibLZMA::LibLZMA ALIAS liblzma)
