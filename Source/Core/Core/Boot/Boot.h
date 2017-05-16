// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdlib>
#include <string>

#include "Common/CommonTypes.h"

namespace DiscIO
{
class IVolume;
struct Partition;
}

struct RegionSetting
{
  const std::string area;
  const std::string video;
  const std::string game;
  const std::string code;
};

class CBoot
{
public:
  static bool BootUp();
  static bool IsElfWii(const std::string& filename);

  // Tries to find a map file for the current game by looking first in the
  // local user directory, then in the shared user directory.
  //
  // If existing_map_file is not nullptr and a map file exists, it is set to the
  // path to the existing map file.
  //
  // If writable_map_file is not nullptr, it is set to the path to where a map
  // file should be saved.
  //
  // If title_id is not nullptr, it is set to the title id
  //
  // Returns true if a map file exists, false if none could be found.
  static bool FindMapFile(std::string* existing_map_file, std::string* writable_map_file,
                          std::string* title_id = nullptr);
  static bool LoadMapFromFilename();

private:
  static bool DVDRead(const DiscIO::IVolume& volume, u64 dvd_offset, u32 output_address, u32 length,
                      const DiscIO::Partition& partition);
  static void RunFunction(u32 address);

  static void UpdateDebugger_MapLoaded();

  static bool Boot_ELF(const std::string& filename);
  static bool Boot_WiiWAD(const std::string& filename);

  static void SetupBAT(bool is_wii);
  static bool RunApploader(bool is_wii, const DiscIO::IVolume& volume);
  static bool EmulatedBS2_GC(const DiscIO::IVolume* volume, bool skip_app_loader = false);
  static bool EmulatedBS2_Wii(const DiscIO::IVolume* volume);
  static bool EmulatedBS2(bool is_wii, const DiscIO::IVolume* volume);
  static bool Load_BS2(const std::string& boot_rom_filename);
  static void Load_FST(bool is_wii, const DiscIO::IVolume* volume);

  static bool SetupWiiMemory(const DiscIO::IVolume* volume, u64 ios_title_id);
};
