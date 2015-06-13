// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "DiscIO/Enums.h"

namespace DiscIO
{
enum class BlobType;

struct Partition final
{
  Partition() : offset(-1) {}
  explicit Partition(u64 offset_) : offset(offset_) {}
  bool operator==(const Partition& other) const { return offset == other.offset; }
  bool operator!=(const Partition& other) const { return !(*this == other); }
  bool operator<(const Partition& other) const { return offset < other.offset; }
  bool operator>(const Partition& other) const { return other < *this; }
  bool operator<=(const Partition& other) const { return !(*this < other); }
  bool operator>=(const Partition& other) const { return !(*this > other); }
  u64 offset;
};

const Partition PARTITION_NONE(-2);

class IVolume
{
public:
  IVolume() {}
  virtual ~IVolume() {}
  virtual bool Read(u64 _Offset, u64 _Length, u8* _pBuffer, const Partition& partition) const = 0;
  template <typename T>
  bool ReadSwapped(u64 offset, T* buffer, const Partition& partition) const
  {
    T temp;
    if (!Read(offset, sizeof(T), reinterpret_cast<u8*>(&temp), partition))
      return false;
    *buffer = Common::FromBigEndian(temp);
    return true;
  }
  virtual std::vector<Partition> GetPartitions() const { return {{}}; }
  virtual Partition GetGamePartition() const { return PARTITION_NONE; }
  virtual bool GetTitleID(u64*) const { return false; }
  virtual std::vector<u8> GetTMD(const Partition& partition) const { return {}; }
  virtual std::string GetUniqueID() const = 0;
  virtual std::string GetMakerID() const = 0;
  virtual u16 GetRevision() const = 0;
  virtual std::string GetInternalName() const = 0;
  virtual std::map<Language, std::string> GetShortNames() const { return {{}}; }
  virtual std::map<Language, std::string> GetLongNames() const { return {{}}; }
  virtual std::map<Language, std::string> GetShortMakers() const { return {{}}; }
  virtual std::map<Language, std::string> GetLongMakers() const { return {{}}; }
  virtual std::map<Language, std::string> GetDescriptions() const { return {{}}; }
  virtual std::vector<u32> GetBanner(int* width, int* height) const = 0;
  virtual u64 GetFSTSize() const = 0;
  virtual std::string GetApploaderDate() const = 0;
  // 0 is the first disc, 1 is the second disc
  virtual u8 GetDiscNumber() const { return 0; }
  virtual Platform GetVolumeType() const = 0;
  virtual bool SupportsIntegrityCheck() const { return false; }
  virtual bool CheckIntegrity(const Partition& partition) const { return false; }
  virtual Country GetCountry() const = 0;
  virtual BlobType GetBlobType() const = 0;
  // Size of virtual disc (not always accurate)
  virtual u64 GetSize() const = 0;
  // Size on disc (compressed size)
  virtual u64 GetRawSize() const = 0;

  static std::vector<u32> GetWiiBanner(int* width, int* height, u64 title_id);

protected:
  template <u32 N>
  std::string DecodeString(const char (&data)[N]) const
  {
    // strnlen to trim NULLs
    std::string string(data, strnlen(data, sizeof(data)));

    // There doesn't seem to be any GC discs with the country set to Taiwan...
    // But maybe they would use Shift_JIS if they existed? Not sure
    bool use_shift_jis =
        (Country::COUNTRY_JAPAN == GetCountry() || Country::COUNTRY_TAIWAN == GetCountry());

    if (use_shift_jis)
      return SHIFTJISToUTF8(string);
    else
      return CP1252ToUTF8(string);
  }

  static std::map<Language, std::string> ReadWiiNames(const std::vector<u8>& data);

  static const size_t NUMBER_OF_LANGUAGES = 10;
  static const size_t NAME_STRING_LENGTH = 42;
  static const size_t NAME_BYTES_LENGTH = NAME_STRING_LENGTH * sizeof(u16);
  static const size_t NAMES_TOTAL_BYTES = NAME_BYTES_LENGTH * NUMBER_OF_LANGUAGES;
};

}  // namespace
