#include <array>
#include <string_view>

// This contains the texture names for each culprit causing the infamous pixel glitch.
// These textures will be made transparent when detected in the TextureCacheBase.

const std::array<std::string_view, 3> prime_pixel_textures = {
  "tex1_32x32_m_e0267b6c99e16dd4_14",
  "tex1_32x32_m_e0267b6c99e16dd4_1",
  "tex1_128x32_b2f411682a7de4a5_14"
};
