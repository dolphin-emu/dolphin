// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/Volume.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWad.h"
#include "DiscIO/VolumeWii.h"

namespace DiscIO
{
static const unsigned int WII_BANNER_WIDTH = 192;
static const unsigned int WII_BANNER_HEIGHT = 64;
static const unsigned int WII_BANNER_SIZE = WII_BANNER_WIDTH * WII_BANNER_HEIGHT * 2;
static const unsigned int WII_BANNER_OFFSET = 0xA0;

const IOS::ES::TicketReader Volume::INVALID_TICKET{};
const IOS::ES::TMDReader Volume::INVALID_TMD{};

std::vector<u32> Volume::GetWiiBanner(int* width, int* height, u64 title_id)
{
  *width = 0;
  *height = 0;

  const std::string file_name =
      Common::GetTitleDataPath(title_id, Common::FROM_CONFIGURED_ROOT) + "banner.bin";

  File::IOFile file(file_name, "rb");
  if (file.GetSize() < WII_BANNER_OFFSET + WII_BANNER_SIZE)
    return std::vector<u32>();

  if (!file.Seek(WII_BANNER_OFFSET, SEEK_SET))
    return std::vector<u32>();

  std::vector<u8> banner_file(WII_BANNER_SIZE);
  if (!file.ReadBytes(banner_file.data(), banner_file.size()))
    return std::vector<u32>();

  std::vector<u32> image_buffer(WII_BANNER_WIDTH * WII_BANNER_HEIGHT);
  ColorUtil::decode5A3image(image_buffer.data(), (u16*)banner_file.data(), WII_BANNER_WIDTH,
                            WII_BANNER_HEIGHT);

  *width = WII_BANNER_WIDTH;
  *height = WII_BANNER_HEIGHT;
  return image_buffer;
}

std::map<Language, std::string> Volume::ReadWiiNames(const std::vector<u8>& data)
{
  std::map<Language, std::string> names;
  for (size_t i = 0; i < NUMBER_OF_LANGUAGES; ++i)
  {
    size_t name_start = NAME_BYTES_LENGTH * i;
    size_t name_end = name_start + NAME_BYTES_LENGTH;
    if (data.size() >= name_end)
    {
      u16* temp = (u16*)(data.data() + name_start);
      std::wstring out_temp(NAME_STRING_LENGTH, '\0');
      std::transform(temp, temp + out_temp.size(), out_temp.begin(), (u16(&)(u16))Common::swap16);
      out_temp.erase(std::find(out_temp.begin(), out_temp.end(), 0x00), out_temp.end());
      std::string name = UTF16ToUTF8(out_temp);
      if (!name.empty())
        names[static_cast<Language>(i)] = name;
    }
  }
  return names;
}

std::unique_ptr<Volume> CreateVolumeFromFilename(const std::string& filename)
{
  std::unique_ptr<BlobReader> reader(CreateBlobReader(filename));
  if (reader == nullptr)
    return nullptr;

  // Check for Wii
  const std::optional<u32> wii_magic = reader->ReadSwapped<u32>(0x18);
  if (wii_magic == u32(0x5D1C9EA3))
    return std::make_unique<VolumeWii>(std::move(reader));

  // Check for WAD
  // 0x206962 for boot2 wads
  const std::optional<u32> wad_magic = reader->ReadSwapped<u32>(0x02);
  if (wad_magic == u32(0x00204973) || wad_magic == u32(0x00206962))
    return std::make_unique<VolumeWAD>(std::move(reader));

  // Check for GC
  const std::optional<u32> gc_magic = reader->ReadSwapped<u32>(0x1C);
  if (gc_magic == u32(0xC2339F3D))
    return std::make_unique<VolumeGC>(std::move(reader));

  // No known magic words found
  return nullptr;
}

}  // namespace
