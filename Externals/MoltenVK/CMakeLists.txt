include(ExternalProject)

ExternalProject_Add(MoltenVK
  GIT_REPOSITORY https://github.com/KhronosGroup/MoltenVK.git
  GIT_TAG v1.1.4

  CONFIGURE_COMMAND <SOURCE_DIR>/fetchDependencies --macos

  PATCH_COMMAND git reset --hard v1.1.4 && git apply ${CMAKE_SOURCE_DIR}/Externals/MoltenVK/patches/0001-SPIRVToMSLConverter-Enable-use_framebuffer_fetch_sub.patch

  BUILD_COMMAND make -C <SOURCE_DIR> macos
  BUILD_IN_SOURCE ON
  BUILD_BYPRODUCTS <SOURCE_DIR>/Package/Release/MoltenVK/dylib/macOS/libMoltenVK.dylib

  INSTALL_COMMAND ""

  LOG_CONFIGURE ON
  LOG_BUILD ON
  LOG_OUTPUT_ON_FAILURE ON
)
