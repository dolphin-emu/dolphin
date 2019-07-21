// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWad.h"

namespace File
{
class IOFile;
}

struct RegionSetting
{
  const std::string area;
  const std::string video;
  const std::string game;
  const std::string code;
};

class BootExecutableReader;

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
  GenerateFromFile(std::string boot_path, const std::optional<std::string>& savestate_path = {});
  static std::unique_ptr<BootParameters>
  GenerateFromFile(std::vector<std::string> paths,
                   const std::optional<std::string>& savestate_path = {});

  using Parameters = std::variant<Disc, Executable, DiscIO::VolumeWAD, NANDTitle, IPL, DFF>;
  BootParameters(Parameters&& parameters_, const std::optional<std::string>& savestate_path_ = {});

  Parameters parameters;
  std::optional<std::string> savestate_path;
  bool delete_savestate = false;
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
  static void RunFunction(u32 address);

  static void UpdateDebugger_MapLoaded();

  static bool Boot_WiiWAD(const DiscIO::VolumeWAD& wad);
  static bool BootNANDTitle(u64 title_id);

  static void SetupMSR();
  static void SetupBAT(bool is_wii);
  static bool RunApploader(bool is_wii, const DiscIO::VolumeDisc& volume);
  static bool EmulatedBS2_GC(const DiscIO::VolumeDisc& volume);
  static bool EmulatedBS2_Wii(const DiscIO::VolumeDisc& volume);
  static bool EmulatedBS2(bool is_wii, const DiscIO::VolumeDisc& volume);
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
