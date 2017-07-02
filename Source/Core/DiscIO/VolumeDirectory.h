// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

namespace File
{
struct FSTEntry;
}

//
// --- this volume type is used for reading files directly from the hard drive ---
//

namespace DiscIO
{
enum class BlobType;
enum class Country;
enum class Language;
enum class Region;
enum class Platform;

class VolumeDirectory : public Volume
{
public:
  VolumeDirectory(const std::string& directory, bool is_wii, const std::string& apploader = "",
                  const std::string& dol = "");

  ~VolumeDirectory();

  static bool IsValidDirectory(const std::string& directory);

  bool Read(u64 offset, u64 length, u8* buffer, const Partition& partition) const override;
  std::vector<Partition> GetPartitions() const override;
  Partition GetGamePartition() const override;

  std::string GetGameID(const Partition& partition = PARTITION_NONE) const override;
  void SetGameID(const std::string& id);

  std::string GetMakerID(const Partition& partition = PARTITION_NONE) const override;

  std::optional<u16> GetRevision(const Partition& partition = PARTITION_NONE) const override
  {
    return {};
  }
  std::string GetInternalName(const Partition& partition = PARTITION_NONE) const override;
  std::map<Language, std::string> GetLongNames() const override;
  std::vector<u32> GetBanner(int* width, int* height) const override;
  void SetName(const std::string&);

  std::string GetApploaderDate(const Partition& partition = PARTITION_NONE) const override;
  Platform GetVolumeType() const override;

  Region GetRegion() const override;
  Country GetCountry(const Partition& partition = PARTITION_NONE) const override;

  BlobType GetBlobType() const override;
  u64 GetSize() const override;
  u64 GetRawSize() const override;

  void BuildFST();

private:
  static std::string ExtractDirectoryName(const std::string& directory);

  void SetDiskTypeWii();
  void SetDiskTypeGC();

  bool SetApploader(const std::string& apploader);

  void SetDOL(const std::string& dol);

  // writing to read buffer
  void WriteToBuffer(u64 source_start_address, u64 source_length, const u8* source, u64* address,
                     u64* length, u8** buffer) const;

  void PadToAddress(u64 start_address, u64* address, u64* length, u8** buffer) const;

  void Write32(u32 data, u32 offset, std::vector<u8>* const buffer);

  // FST creation
  void WriteEntryData(u32* entry_offset, u8 type, u32 name_offset, u64 data_offset, u64 length,
                      u32 address_shift);
  void WriteEntryName(u32* name_offset, const std::string& name);
  void WriteDirectory(const File::FSTEntry& parent_entry, u32* fst_offset, u32* name_offset,
                      u64* data_offset, u32 parent_entry_index);

  std::string m_root_directory;

  std::map<u64, std::string> m_virtual_disk;

  bool m_is_wii;

  // GameCube has no shift, Wii has 2 bit shift
  u32 m_address_shift;

  // first address on disk containing file data
  u64 m_data_start_address;

  u64 m_fst_name_offset;
  std::vector<u8> m_fst_data;

  std::vector<u8> m_disk_header;

#pragma pack(push, 1)
  struct SDiskHeaderInfo
  {
    u32 debug_monitor_size;
    u32 simulated_mem_size;
    u32 arg_offset;
    u32 debug_flag;
    u32 track_location;
    u32 track_size;
    u32 country_code;
    u32 unknown;
    u32 unknown2;

    // All the data is byteswapped
    SDiskHeaderInfo()
    {
      debug_monitor_size = 0;
      simulated_mem_size = 0;
      arg_offset = 0;
      debug_flag = 0;
      track_location = 0;
      track_size = 0;
      country_code = 0;
      unknown = 0;
      unknown2 = 0;
    }
  };
#pragma pack(pop)
  std::unique_ptr<SDiskHeaderInfo> m_disk_header_info;

  std::vector<u8> m_apploader;
  std::vector<u8> m_dol;

  u64 m_fst_address;
  u64 m_dol_address;

  static constexpr u8 ENTRY_SIZE = 0x0c;
  static constexpr u8 FILE_ENTRY = 0;
  static constexpr u8 DIRECTORY_ENTRY = 1;
  static constexpr u64 DISKHEADER_ADDRESS = 0;
  static constexpr u64 DISKHEADERINFO_ADDRESS = 0x440;
  static constexpr u64 APPLOADER_ADDRESS = 0x2440;
  static const size_t MAX_NAME_LENGTH = 0x3df;
  static const size_t MAX_ID_LENGTH = 6;
};

}  // namespace
