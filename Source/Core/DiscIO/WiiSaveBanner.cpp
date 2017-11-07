// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/WiiSaveBanner.h"

#include <string>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"

namespace DiscIO
{
constexpr unsigned int BANNER_WIDTH = 192;
constexpr unsigned int BANNER_HEIGHT = 64;
constexpr unsigned int BANNER_SIZE = BANNER_WIDTH * BANNER_HEIGHT * 2;

constexpr unsigned int ICON_WIDTH = 48;
constexpr unsigned int ICON_HEIGHT = 48;
constexpr unsigned int ICON_SIZE = ICON_WIDTH * ICON_HEIGHT * 2;

WiiSaveBanner::WiiSaveBanner(u64 title_id)
    : WiiSaveBanner(Common::GetTitleDataPath(title_id, Common::FROM_CONFIGURED_ROOT) + "banner.bin")
{
}

WiiSaveBanner::WiiSaveBanner(const std::string& path) : m_path(path), m_valid(true)
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
  return UTF16BEToUTF8(m_header.name, ArraySize(m_header.name));
}

std::string WiiSaveBanner::GetDescription() const
{
  return UTF16BEToUTF8(m_header.description, ArraySize(m_header.description));
}

std::vector<u32> WiiSaveBanner::GetBanner(int* width, int* height) const
{
  *width = 0;
  *height = 0;

  File::IOFile file(m_path, "rb");
  if (!file.Seek(sizeof(Header), SEEK_SET))
    return std::vector<u32>();

  std::vector<u16> banner_data(BANNER_WIDTH * BANNER_HEIGHT);
  if (!file.ReadArray(banner_data.data(), banner_data.size()))
    return std::vector<u32>();

  std::vector<u32> image_buffer(BANNER_WIDTH * BANNER_HEIGHT);
  ColorUtil::decode5A3image(image_buffer.data(), banner_data.data(), BANNER_WIDTH, BANNER_HEIGHT);

  *width = BANNER_WIDTH;
  *height = BANNER_HEIGHT;
  return image_buffer;
}

}  // namespace DiscIO
