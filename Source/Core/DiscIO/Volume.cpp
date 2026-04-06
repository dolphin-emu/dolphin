// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/Volume.h"

#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/StringUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/IOS/ES/Formats.h"

#include "DiscIO/Blob.h"
#include "DiscIO/CachedBlob.h"
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

std::string Volume::DecodeString(std::span<const char> data) const
{
  // strnlen to trim null bytes
  std::string string(data.data(), strnlen(data.data(), data.size()));
  return GetRegion() == Region::NTSC_J ? SHIFTJISToUTF8(string) : CP1252ToUTF8(string);
}

std::string Volume::FilterGameID(std::span<const char> data)
{
  std::string string(data.data(), data.size());

  // We don't want game IDs to contain characters that are unprintable or might cause path
  // traversal. Game IDs normally only contain ASCII uppercase letters and numbers,
  // but GNHE5d contains a lowercase letter, so let's allow all ASCII letters and numbers.
  std::ranges::replace_if(string, std::not_fn(Common::IsAlnum), '-');

  return string;
}

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

template <typename T = std::identity>
static std::unique_ptr<VolumeDisc> TryCreateDisc(std::unique_ptr<BlobReader>& reader,
                                                 const T& reader_adapter_factory = {})
{
  if (!reader)
    return nullptr;

  // `reader_adapter_factory` is used *after* successful magic word read.
  // This prevents `CachedBlobReader` from showing warnings when failing to scrub a .dol file.

  if (reader->ReadSwapped<u32>(0x18) == WII_DISC_MAGIC)
    return std::make_unique<VolumeWii>(reader_adapter_factory(std::move(reader)));

  if (reader->ReadSwapped<u32>(0x1C) == GAMECUBE_DISC_MAGIC)
    return std::make_unique<VolumeGC>(reader_adapter_factory(std::move(reader)));

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

std::unique_ptr<VolumeDisc> CreateDiscForCore(const std::string& path)
{
  auto reader = CreateBlobReader(path);

  if (Config::Get(Config::MAIN_LOAD_GAME_INTO_MEMORY))
    return TryCreateDisc(reader, CreateScrubbingCachedBlobReader);

  return TryCreateDisc(reader);
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
  std::unique_ptr<VolumeDisc> disc = TryCreateDisc(reader);
  if (disc)
    return disc;

  std::unique_ptr<VolumeWAD> wad = TryCreateWAD(reader);
  if (wad)
    return wad;

  return nullptr;
}

std::unique_ptr<Volume> CreateVolume(const std::string& path)
{
  return CreateVolume(CreateBlobReader(path));
}
}  // namespace DiscIO
