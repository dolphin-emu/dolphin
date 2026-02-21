// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"

namespace Common
{
class StbImage final
{
public:
  StbImage();
  StbImage(u8* pixel_data, int width, int height, int channels);
  StbImage(std::vector<u8>&& pixels, int width, int height, int channels);

  static std::optional<StbImage> LoadFromMemory(std::vector<u8> image_data, int desired_channels);
  static std::optional<StbImage> LoadFromFile(File::IOFile file, int desired_channels);

  const std::vector<u8>& GetData() const { return m_pixel_data; }
  int GetWidth() const { return m_width; }
  int GetHeight() const { return m_height; }
  int GetChannels() const { return m_channels; }

  void Resize(int target_width, int target_height);
  std::optional<std::vector<u8>> EncodeToBaselineJPEG(int quality) const;

private:
  std::vector<u8> m_pixel_data;
  int m_width;
  int m_height;
  int m_channels;
};
}  // namespace Common
