// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/Volume.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/StringUtil.h"

#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Enums.h"
#include "DiscIO/VolumeDisc.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWad.h"
#include "DiscIO/VolumeWii.h"

namespace DiscIO
{
const IOS::ES::TicketReader Volume::INVALID_TICKET{};
const IOS::ES::TMDReader Volume::INVALID_TMD{};
const std::vector<u8> Volume::INVALID_CERT_CHAIN{};

template <typename T>
static void AddToSyncHash(Common::SHA1::Context* context, const T& data)
{
  static_assert(std::is_trivially_copyable_v<T>);
  context->Update(reinterpret_cast<const u8*>(&data), sizeof(data));
}

void Volume::ReadAndAddToSyncHash(Common::SHA1::Context* context, u64 offset, u64 length,
                                  const Partition& partition) const
{
  std::vector<u8> buffer(length);
  if (Read(offset, length, buffer.data(), partition))
    context->Update(buffer);
}

void Volume::AddTMDToSyncHash(Common::SHA1::Context* context, const Partition& partition) const
{
  // We want to hash some important parts of the TMD, but nothing that changes when fakesigning.
  // (Fakesigned WADs are very popular, and we don't want people with properly signed WADs to
  // unnecessarily be at a disadvantage due to most netplay partners having fakesigned WADs.)

  const IOS::ES::TMDReader& tmd = GetTMD(partition);
  if (!tmd.IsValid())
    return;

  AddToSyncHash(context, tmd.GetIOSId());
  AddToSyncHash(context, tmd.GetTitleId());
  AddToSyncHash(context, tmd.GetTitleFlags());
  AddToSyncHash(context, tmd.GetGroupId());
  AddToSyncHash(context, tmd.GetRegion());
  AddToSyncHash(context, tmd.GetTitleVersion());
  AddToSyncHash(context, tmd.GetBootIndex());

  for (const IOS::ES::Content& content : tmd.GetContents())
    AddToSyncHash(context, content);
}

std::map<Language, std::string> Volume::ReadWiiNames(const std::vector<char16_t>& data)
{
  std::map<Language, std::string> names;
  for (size_t i = 0; i < NUMBER_OF_LANGUAGES; ++i)
  {
    const size_t name_start = NAME_CHARS_LENGTH * i;
    if (name_start + NAME_CHARS_LENGTH <= data.size())
    {
      const std::string name = UTF16BEToUTF8(data.data() + name_start, NAME_CHARS_LENGTH);
      if (!name.empty())
        names[static_cast<Language>(i)] = name;
    }
  }
  return names;
}

static std::unique_ptr<VolumeDisc> TryCreateDisc(std::unique_ptr<BlobReader>& reader)
{
  if (!reader)
    return nullptr;

  if (reader->ReadSwapped<u32>(0x18) == WII_DISC_MAGIC)
    return std::make_unique<VolumeWii>(std::move(reader));

  if (reader->ReadSwapped<u32>(0x1C) == GAMECUBE_DISC_MAGIC)
    return std::make_unique<VolumeGC>(std::move(reader));

  // No known magic words found
  return nullptr;
}

std::unique_ptr<VolumeDisc> CreateDisc(std::unique_ptr<BlobReader> reader)
{
  return TryCreateDisc(reader);
}

std::unique_ptr<VolumeDisc> CreateDisc(const std::string& path)
{
  return CreateDisc(CreateBlobReader(path));
}

static std::unique_ptr<VolumeWAD> TryCreateWAD(std::unique_ptr<BlobReader>& reader)
{
  if (!reader)
    return nullptr;

  // Check for WAD
  // 0x206962 for boot2 wads
  const std::optional<u32> wad_magic = reader->ReadSwapped<u32>(0x02);
  if (wad_magic == u32(0x00204973) || wad_magic == u32(0x00206962))
    return std::make_unique<VolumeWAD>(std::move(reader));

  // No known magic words found
  return nullptr;
}

std::unique_ptr<VolumeWAD> CreateWAD(std::unique_ptr<BlobReader> reader)
{
  return TryCreateWAD(reader);
}

std::unique_ptr<VolumeWAD> CreateWAD(const std::string& path)
{
  return CreateWAD(CreateBlobReader(path));
}

std::unique_ptr<Volume> CreateVolume(std::unique_ptr<BlobReader> reader)
{
  if (std::unique_ptr<VolumeDisc> disc = TryCreateDisc(reader))
    return disc;

  if (std::unique_ptr<VolumeWAD> wad = TryCreateWAD(reader))
    return wad;

  return nullptr;
}

std::unique_ptr<Volume> CreateVolume(const std::string& path)
{
  return CreateVolume(CreateBlobReader(path));
}
}  // namespace DiscIO
