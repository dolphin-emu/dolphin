// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/IOSC.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/RiivolutionParser.h"
#include "DiscIO/VolumeDisc.h"
#include "DiscIO/VolumeWad.h"

namespace File
{
class IOFile;
}

namespace IOS::HLE::FS
{
class FileSystem;
}

struct RegionSetting
{
  std::string area;
  std::string video;
  std::string game;
  std::string code;
};

class BootExecutableReader;

enum class DeleteSavestateAfterBoot : u8
{
  No,
  Yes
};

class BootSessionData
{
public:
  BootSessionData();
  BootSessionData(std::optional<std::string> savestate_path,
                  DeleteSavestateAfterBoot delete_savestate);
  BootSessionData(const BootSessionData& other) = delete;
  BootSessionData(BootSessionData&& other);
  BootSessionData& operator=(const BootSessionData& other) = delete;
  BootSessionData& operator=(BootSessionData&& other);
  ~BootSessionData();

  const std::optional<std::string>& GetSavestatePath() const;
  DeleteSavestateAfterBoot GetDeleteSavestate() const;
  void SetSavestateData(std::optional<std::string> savestate_path,
                        DeleteSavestateAfterBoot delete_savestate);

  using WiiSyncCleanupFunction = std::function<void()>;

  IOS::HLE::FS::FileSystem* GetWiiSyncFS() const;
  const std::vector<u64>& GetWiiSyncTitles() const;
  const std::string& GetWiiSyncRedirectFolder() const;
  void InvokeWiiSyncCleanup() const;
  void SetWiiSyncData(std::unique_ptr<IOS::HLE::FS::FileSystem> fs, std::vector<u64> titles,
                      std::string redirect_folder, WiiSyncCleanupFunction cleanup);

private:
  std::optional<std::string> m_savestate_path;
  DeleteSavestateAfterBoot m_delete_savestate = DeleteSavestateAfterBoot::No;

  std::unique_ptr<IOS::HLE::FS::FileSystem> m_wii_sync_fs;
  std::vector<u64> m_wii_sync_titles;
  std::string m_wii_sync_redirect_folder;
  WiiSyncCleanupFunction m_wii_sync_cleanup;
};

struct BootParameters
{
  struct Disc
  {
    std::string path;
    std::unique_ptr<DiscIO::VolumeDisc> volume;
    std::vector<std::string> auto_disc_change_paths;
  };

  struct Executable
  {
    std::string path;
    std::unique_ptr<BootExecutableReader> reader;
  };

  struct NANDTitle
  {
    u64 id;
  };

  struct IPL
  {
    explicit IPL(DiscIO::Region region_);
    IPL(DiscIO::Region region_, Disc&& disc_);
    std::string path;
    DiscIO::Region region;
    // It is possible to boot the IPL with a disc inserted (with "skip IPL" disabled).
    std::optional<Disc> disc;
  };

  struct DFF
  {
    std::string dff_path;
  };

  static std::unique_ptr<BootParameters>
  GenerateFromFile(std::string boot_path, BootSessionData boot_session_data_ = BootSessionData());
  static std::unique_ptr<BootParameters>
  GenerateFromFile(std::vector<std::string> paths,
                   BootSessionData boot_session_data_ = BootSessionData());

  using Parameters = std::variant<Disc, Executable, DiscIO::VolumeWAD, NANDTitle, IPL, DFF>;
  BootParameters(Parameters&& parameters_, BootSessionData boot_session_data_ = BootSessionData());

  Parameters parameters;
  std::vector<DiscIO::Riivolution::Patch> riivolution_patches;
  BootSessionData boot_session_data;
};

class CBoot
{
public:
  static bool BootUp(std::unique_ptr<BootParameters> boot);

  // Tries to find a map file for the current game by looking first in the
  // local user directory, then in the shared user directory.
  //
  // If existing_map_file is not nullptr and a map file exists, it is set to the
  // path to the existing map file.
  //
  // If writable_map_file is not nullptr, it is set to the path to where a map
  // file should be saved.
  //
  // Returns true if a map file exists, false if none could be found.
  static bool FindMapFile(std::string* existing_map_file, std::string* writable_map_file);
  static bool LoadMapFromFilename();

private:
  static bool DVDRead(const DiscIO::VolumeDisc& disc, u64 dvd_offset, u32 output_address,
                      u32 length, const DiscIO::Partition& partition);
  static bool DVDReadDiscID(const DiscIO::VolumeDisc& disc, u32 output_address);
  static void RunFunction(u32 address);

  static void UpdateDebugger_MapLoaded();

  static bool Boot_WiiWAD(const DiscIO::VolumeWAD& wad);
  static bool BootNANDTitle(u64 title_id);

  static void SetupMSR();
  static void SetupBAT(bool is_wii);
  static bool RunApploader(bool is_wii, const DiscIO::VolumeDisc& volume,
                           const std::vector<DiscIO::Riivolution::Patch>& riivolution_patches);
  static bool EmulatedBS2_GC(const DiscIO::VolumeDisc& volume,
                             const std::vector<DiscIO::Riivolution::Patch>& riivolution_patches);
  static bool EmulatedBS2_Wii(const DiscIO::VolumeDisc& volume,
                              const std::vector<DiscIO::Riivolution::Patch>& riivolution_patches);
  static bool EmulatedBS2(bool is_wii, const DiscIO::VolumeDisc& volume,
                          const std::vector<DiscIO::Riivolution::Patch>& riivolution_patches);
  static bool Load_BS2(const std::string& boot_rom_filename);

  static void SetupGCMemory();
  static bool SetupWiiMemory(IOS::HLE::IOSC::ConsoleType console_type);
};

class BootExecutableReader
{
public:
  explicit BootExecutableReader(const std::string& file_name);
  explicit BootExecutableReader(File::IOFile file);
  explicit BootExecutableReader(std::vector<u8> buffer);
  virtual ~BootExecutableReader();

  virtual u32 GetEntryPoint() const = 0;
  virtual bool IsValid() const = 0;
  virtual bool IsWii() const = 0;
  virtual bool LoadIntoMemory(bool only_in_mem1 = false) const = 0;
  virtual bool LoadSymbols() const = 0;

protected:
  std::vector<u8> m_bytes;
};

struct StateFlags
{
  void UpdateChecksum();
  u32 checksum;
  u8 flags;
  u8 type;
  u8 discstate;
  u8 returnto;
  u32 unknown[6];
};

// Reads the state file from the NAND, then calls the passed update function to update the struct,
// and finally writes the updated state file to the NAND.
void UpdateStateFlags(std::function<void(StateFlags*)> update_function);

/// Create title directories for the system menu (if needed).
///
/// Normally, this is automatically done by ES when the System Menu is installed,
/// but we cannot rely on this because we don't require any system titles to be installed.
void CreateSystemMenuTitleDirs();

void AddRiivolutionPatches(BootParameters* boot_params,
                           std::vector<DiscIO::Riivolution::Patch> riivolution_patches);
