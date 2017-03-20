// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/FileMonitor.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <unordered_set>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"

#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace FileMonitor
{
static const DiscIO::IVolume* s_volume = nullptr;
static bool s_wii_disc;
static std::unique_ptr<DiscIO::IFileSystem> s_filesystem;
static std::string s_previous_file;

// Filtered files
static bool IsSoundFile(const std::string& filename)
{
  std::string extension;
  SplitPath(filename, nullptr, nullptr, &extension);
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  static const std::unordered_set<std::string> extensions = {
      ".adp",    // 1080 Avalanche, Crash Bandicoot, etc.
      ".adx",    // Sonic Adventure 2 Battle, etc.
      ".afc",    // Zelda WW
      ".ast",    // Zelda TP, Mario Kart
      ".brstm",  // Wii Sports, Wario Land, etc.
      ".dsp",    // Metroid Prime
      ".hps",    // SSB Melee
      ".ogg",    // Tony Hawk's Underground 2
      ".sad",    // Disaster
      ".snd",    // Tales of Symphonia
      ".song",   // Tales of Symphonia
      ".ssm",    // Custom Robo, Kirby Air Ride, etc.
      ".str",    // Harry Potter & the Sorcerer's Stone
  };

  return extensions.find(extension) != extensions.end();
}

void SetFileSystem(const DiscIO::IVolume* volume)
{
  // Instead of creating the file system object right away, we will let Log
  // create it later once we know that it actually will get used
  s_volume = volume;

  // If the volume that was passed in was nullptr, Log won't try to create a
  // file system object later, so we have to set s_filesystem to nullptr right away
  s_filesystem = nullptr;
}

// Logs access to files in the file system set by SetFileSystem
void Log(u64 offset, bool decrypt)
{
  // Do nothing if the log isn't selected
  if (!LogManager::GetInstance()->IsEnabled(LogTypes::FILEMON, LogTypes::LWARNING))
    return;

  // If a new volume has been set, use the file system of that volume
  if (s_volume)
  {
    s_wii_disc = s_volume->GetVolumeType() == DiscIO::Platform::WII_DISC;
    if (decrypt != s_wii_disc)
      return;
    s_filesystem = DiscIO::CreateFileSystem(s_volume);
    s_volume = nullptr;
    s_previous_file.clear();
  }

  // For Wii discs, FileSystemGCWii will only load file systems from encrypted partitions
  if (decrypt != s_wii_disc)
    return;

  // Do nothing if there is no valid file system
  if (!s_filesystem)
    return;

  const std::string filename = s_filesystem->GetFileName(offset);

  // Do nothing if no file was found at that offset
  if (filename.empty())
    return;

  // Do nothing if we found the same file again
  if (s_previous_file == filename)
    return;

  const u64 size = s_filesystem->GetFileSize(filename);
  const std::string size_string = ThousandSeparate(size / 1000, 7);

  const std::string log_string =
      StringFromFormat("%s kB %s", size_string.c_str(), filename.c_str());
  if (IsSoundFile(filename))
    INFO_LOG(FILEMON, "%s", log_string.c_str());
  else
    WARN_LOG(FILEMON, "%s", log_string.c_str());

  // Update the last accessed file
  s_previous_file = filename;
}

}  // namespace FileMonitor
