// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "StbImage.h"

#include <cstring>
#include <optional>
#include <vector>

#include <stb_image.h>
#include <stb_image_resize2.h>
#include <stb_image_write.h>

#include "Common/CommonTypes.h"

namespace Common
{
StbImage::StbImage() : m_pixel_data({0}), m_width(1), m_height(1), m_channels(1)
{
}

StbImage::StbImage(u8* pixel_data, int width, int height, int channels)
    : m_pixel_data(pixel_data, pixel_data + width * height * channels), m_width(width),
      m_height(height), m_channels(channels)
{
}

StbImage::StbImage(std::vector<u8>&& pixels, int width, int height, int channels)
    : m_pixel_data(std::move(pixels)), m_width(width), m_height(height), m_channels(channels)
{
}

std::optional<StbImage> StbImage::LoadFromFile(File::IOFile file, int desired_channels)
{
  std::vector<u8> image_data(static_cast<size_t>(file.GetSize()));
  if (file.ReadBytes(image_data.data(), image_data.size()))
    return LoadFromMemory(std::move(image_data), desired_channels);
  return std::nullopt;
}

std::optional<StbImage> StbImage::LoadFromMemory(std::vector<u8> image_data, int desired_channels)
{
  int width;
  int height;
  int channels_in_file;
  u8* pixels = stbi_load_from_memory(image_data.data(), static_cast<int>(image_data.size()), &width,
                                     &height, &channels_in_file, desired_channels);

  if (!pixels)
    return std::nullopt;

  const int actual_channels = desired_channels ? desired_channels : channels_in_file;

  std::vector<u8> vec(pixels, pixels + width * height * actual_channels);
  stbi_image_free(pixels);
  return StbImage(std::move(vec), width, height, actual_channels);
}

void StbImage::Resize(int target_width, int target_height)
{
  if ((m_width == target_width && m_height == target_height) || m_pixel_data.empty())
    return;

  std::vector<u8> image_resized;
  image_resized.resize(target_width * target_height * m_channels);
  stbir_resize_uint8_linear(m_pixel_data.data(), m_width, m_height, 0, image_resized.data(),
                            target_width, target_height, 0,
                            static_cast<stbir_pixel_layout>(m_channels));
  m_pixel_data.swap(image_resized);
  m_width = target_width;
  m_height = target_height;
}

std::optional<std::vector<u8>> StbImage::EncodeToBaselineJPEG(int quality) const
{
  std::vector<u8> output;

  stbi_write_func* write_func = [](void* ctx, void* data, int size) {
    auto& vec = *reinterpret_cast<std::vector<u8>*>(ctx);
    vec.insert(vec.end(), static_cast<u8*>(data), static_cast<u8*>(data) + size);
  };

  if (!stbi_write_jpg_to_func(write_func, &output, m_width, m_height, m_channels,
                              m_pixel_data.data(), quality))
  {
    return std::nullopt;
  }

  return output;
}
}  // namespace Common
