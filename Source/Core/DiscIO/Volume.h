// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Enums.h"

namespace DiscIO
{
enum class BlobType;

class IVolume
{
public:
  IVolume() {}
  virtual ~IVolume() {}
  // decrypt parameter must be false if not reading a Wii disc
  virtual bool Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt) const = 0;
  template <typename T>
  bool ReadSwapped(u64 offset, T* buffer, bool decrypt) const
  {
    T temp;
    if (!Read(offset, sizeof(T), reinterpret_cast<u8*>(&temp), decrypt))
      return false;
    *buffer = Common::FromBigEndian(temp);
    return true;
  }

  virtual bool GetTitleID(u64*) const { return false; }
  virtual IOS::ES::TicketReader GetTicket() const { return {}; }
  virtual IOS::ES::TMDReader GetTMD() const { return {}; }
  virtual u64 PartitionOffsetToRawOffset(u64 offset) const { return offset; }
  virtual std::string GetGameID() const = 0;
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
  virtual bool CheckIntegrity() const { return false; }
  virtual bool ChangePartition(u64 offset) { return false; }
  virtual Region GetRegion() const = 0;
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

    if (GetRegion() == Region::NTSC_J)
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
