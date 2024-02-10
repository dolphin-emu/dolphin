// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/WiiSaveBanner.h"

#include <iterator>
#include <string>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"

namespace DiscIO
{
constexpr u32 BANNER_WIDTH = 192;
constexpr u32 BANNER_HEIGHT = 64;
constexpr u32 BANNER_SIZE = BANNER_WIDTH * BANNER_HEIGHT * 2;

constexpr u32 ICON_WIDTH = 48;
constexpr u32 ICON_HEIGHT = 48;
constexpr u32 ICON_SIZE = ICON_WIDTH * ICON_HEIGHT * 2;

WiiSaveBanner::WiiSaveBanner(u64 title_id)
    : WiiSaveBanner(Common::GetTitleDataPath(title_id, Common::FromWhichRoot::Configured) +
                    "/banner.bin")
{
}

WiiSaveBanner::WiiSaveBanner(const std::string& path) : m_path(path)
{
  constexpr size_t MINIMUM_SIZE = sizeof(Header) + BANNER_SIZE + ICON_SIZE;
  File::IOFile file(path, "rb");
  if (!file.ReadArray(&m_header, 1))
  {
    m_header = {};
    m_valid = false;
  }
  else if (file.GetSize() < MINIMUM_SIZE)
  {
    m_valid = false;
  }
}

std::string WiiSaveBanner::GetName() const
{
  return UTF16BEToUTF8(m_header.name, std::size(m_header.name));
}

std::string WiiSaveBanner::GetDescription() const
{
  return UTF16BEToUTF8(m_header.description, std::size(m_header.description));
}

std::vector<u32> WiiSaveBanner::GetBanner(u32* width, u32* height) const
{
  *width = 0;
  *height = 0;

  File::IOFile file(m_path, "rb");
  if (!file.Seek(sizeof(Header), File::SeekOrigin::Begin))
    return std::vector<u32>();

  std::vector<u16> banner_data(BANNER_WIDTH * BANNER_HEIGHT);
  if (!file.ReadArray(banner_data.data(), banner_data.size()))
    return std::vector<u32>();

  std::vector<u32> image_buffer(BANNER_WIDTH * BANNER_HEIGHT);
  Common::Decode5A3Image(image_buffer.data(), banner_data.data(), BANNER_WIDTH, BANNER_HEIGHT);

  *width = BANNER_WIDTH;
  *height = BANNER_HEIGHT;
  return image_buffer;
}

}  // namespace DiscIO
