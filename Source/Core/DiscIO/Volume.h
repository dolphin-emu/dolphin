// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Enums.h"

namespace DiscIO
{
class BlobReader;
enum class BlobType;
enum class DataSizeType;
class FileSystem;
class VolumeDisc;
class VolumeWAD;

struct Partition final
{
  constexpr Partition() = default;
  constexpr explicit Partition(u64 offset_) : offset(offset_) {}
  constexpr bool operator==(const Partition& other) const { return offset == other.offset; }
  constexpr bool operator!=(const Partition& other) const { return !(*this == other); }
  constexpr bool operator<(const Partition& other) const { return offset < other.offset; }
  constexpr bool operator>(const Partition& other) const { return other < *this; }
  constexpr bool operator<=(const Partition& other) const { return !(*this < other); }
  constexpr bool operator>=(const Partition& other) const { return !(*this > other); }
  u64 offset{std::numeric_limits<u64>::max()};
};

constexpr Partition PARTITION_NONE(std::numeric_limits<u64>::max() - 1);

class Volume
{
public:
  Volume() {}
  virtual ~Volume() {}
  virtual bool Read(u64 offset, u64 length, u8* buffer, const Partition& partition) const = 0;
  template <typename T>
  std::optional<T> ReadSwapped(u64 offset, const Partition& partition) const
  {
    T temp;
    if (!Read(offset, sizeof(T), reinterpret_cast<u8*>(&temp), partition))
      return std::nullopt;
    return Common::FromBigEndian(temp);
  }
  std::optional<u64> ReadSwappedAndShifted(u64 offset, const Partition& partition) const
  {
    const std::optional<u32> temp = ReadSwapped<u32>(offset, partition);
    if (!temp)
      return std::nullopt;
    return static_cast<u64>(*temp) << GetOffsetShift();
  }

  virtual bool HasWiiHashes() const { return false; }
  virtual bool HasWiiEncryption() const { return false; }
  virtual std::vector<Partition> GetPartitions() const { return {}; }
  virtual Partition GetGamePartition() const { return PARTITION_NONE; }
  virtual std::optional<u32> GetPartitionType(const Partition& partition) const
  {
    return std::nullopt;
  }
  std::optional<u64> GetTitleID() const { return GetTitleID(GetGamePartition()); }
  virtual std::optional<u64> GetTitleID(const Partition& partition) const { return std::nullopt; }
  virtual const IOS::ES::TicketReader& GetTicket(const Partition& partition) const
  {
    return INVALID_TICKET;
  }
  virtual const IOS::ES::TMDReader& GetTMD(const Partition& partition) const { return INVALID_TMD; }
  virtual const std::vector<u8>& GetCertificateChain(const Partition& partition) const
  {
    return INVALID_CERT_CHAIN;
  }
  virtual std::vector<u8> GetContent(u16 index) const { return {}; }
  virtual std::vector<u64> GetContentOffsets() const { return {}; }
  virtual bool CheckContentIntegrity(const IOS::ES::Content& content,
                                     const std::vector<u8>& encrypted_data,
                                     const IOS::ES::TicketReader& ticket) const
  {
    return false;
  }
  virtual IOS::ES::TicketReader GetTicketWithFixedCommonKey() const { return {}; }
  // Returns a non-owning pointer. Returns nullptr if the file system couldn't be read.
  virtual const FileSystem* GetFileSystem(const Partition& partition) const = 0;
  virtual u64 PartitionOffsetToRawOffset(u64 offset, const Partition& partition) const
  {
    return offset;
  }
  virtual std::string GetGameID(const Partition& partition = PARTITION_NONE) const = 0;
  virtual std::string GetGameTDBID(const Partition& partition = PARTITION_NONE) const = 0;
  virtual std::string GetMakerID(const Partition& partition = PARTITION_NONE) const = 0;
  virtual std::optional<u16> GetRevision(const Partition& partition = PARTITION_NONE) const = 0;
  virtual std::string GetInternalName(const Partition& partition = PARTITION_NONE) const = 0;
  virtual std::map<Language, std::string> GetShortNames() const { return {}; }
  virtual std::map<Language, std::string> GetLongNames() const { return {}; }
  virtual std::map<Language, std::string> GetShortMakers() const { return {}; }
  virtual std::map<Language, std::string> GetLongMakers() const { return {}; }
  virtual std::map<Language, std::string> GetDescriptions() const { return {}; }
  virtual std::vector<u32> GetBanner(u32* width, u32* height) const = 0;
  std::string GetApploaderDate() const { return GetApploaderDate(GetGamePartition()); }
  virtual std::string GetApploaderDate(const Partition& partition) const = 0;
  // 0 is the first disc, 1 is the second disc
  virtual std::optional<u8> GetDiscNumber(const Partition& partition = PARTITION_NONE) const
  {
    return 0;
  }
  virtual Platform GetVolumeType() const = 0;
  virtual bool IsDatelDisc() const = 0;
  virtual bool IsNKit() const = 0;
  virtual bool CheckH3TableIntegrity(const Partition& partition) const { return false; }
  virtual bool CheckBlockIntegrity(u64 block_index, const u8* encrypted_data,
                                   const Partition& partition) const
  {
    return false;
  }
  virtual bool CheckBlockIntegrity(u64 block_index, const Partition& partition) const
  {
    return false;
  }
  virtual Region GetRegion() const = 0;
  virtual Country GetCountry(const Partition& partition = PARTITION_NONE) const = 0;
  virtual BlobType GetBlobType() const = 0;
  // Size of virtual disc (may be inaccurate depending on the blob type)
  virtual u64 GetDataSize() const = 0;
  virtual DataSizeType GetDataSizeType() const = 0;
  // Size on disc (compressed size)
  virtual u64 GetRawSize() const = 0;
  virtual const BlobReader& GetBlobReader() const = 0;

  // This hash is intended to be (but is not guaranteed to be):
  // 1. Identical for discs with no differences that affect netplay/TAS sync
  // 2. Different for discs with differences that affect netplay/TAS sync
  // 3. Much faster than hashing the entire disc
  // The way the hash is calculated may change with updates to Dolphin.
  virtual std::array<u8, 20> GetSyncHash() const = 0;

protected:
  template <u32 N>
  std::string DecodeString(const char (&data)[N]) const
  {
    // strnlen to trim NULLs
    std::string string(data, strnlen(data, sizeof(data)));

    if (GetRegion() == Region::NTSC_J)
      return SHIFTJISToUTF8(string);
    else
      return CP1252ToUTF8(string);
  }

  void ReadAndAddToSyncHash(Common::SHA1::Context* context, u64 offset, u64 length,
                            const Partition& partition) const;
  void AddTMDToSyncHash(Common::SHA1::Context* context, const Partition& partition) const;

  virtual u32 GetOffsetShift() const { return 0; }
  static std::map<Language, std::string> ReadWiiNames(const std::vector<char16_t>& data);

  static constexpr size_t NUMBER_OF_LANGUAGES = 10;
  static constexpr size_t NAME_CHARS_LENGTH = 42;
  static constexpr size_t NAME_BYTES_LENGTH = NAME_CHARS_LENGTH * sizeof(char16_t);
  static constexpr size_t NAMES_TOTAL_CHARS = NAME_CHARS_LENGTH * NUMBER_OF_LANGUAGES;
  static constexpr size_t NAMES_TOTAL_BYTES = NAME_BYTES_LENGTH * NUMBER_OF_LANGUAGES;

  static const IOS::ES::TicketReader INVALID_TICKET;
  static const IOS::ES::TMDReader INVALID_TMD;
  static const std::vector<u8> INVALID_CERT_CHAIN;
};

std::unique_ptr<VolumeDisc> CreateDisc(std::unique_ptr<BlobReader> reader);
std::unique_ptr<VolumeDisc> CreateDisc(const std::string& path);
std::unique_ptr<VolumeWAD> CreateWAD(std::unique_ptr<BlobReader> reader);
std::unique_ptr<VolumeWAD> CreateWAD(const std::string& path);
std::unique_ptr<Volume> CreateVolume(std::unique_ptr<BlobReader> reader);
std::unique_ptr<Volume> CreateVolume(const std::string& path);

}  // namespace DiscIO
