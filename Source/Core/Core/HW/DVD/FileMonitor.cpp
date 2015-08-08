// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DVD/FileMonitor.h"

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
static const DiscIO::Volume* s_volume;
static bool s_new_volume = false;
static std::unique_ptr<DiscIO::FileSystem> s_filesystem;
static DiscIO::Partition s_partition;
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

void SetFileSystem(const DiscIO::Volume* volume)
{
  // Instead of creating the file system object right away, we will let Log
  // create it later once we know that it actually will get used
  s_volume = volume;
  s_new_volume = true;
}

// Logs access to files in the file system set by SetFileSystem
void Log(u64 offset, const DiscIO::Partition& partition)
{
  // Do nothing if the log isn't selected
  if (!LogManager::GetInstance()->IsEnabled(LogTypes::FILEMON, LogTypes::LWARNING))
    return;

  // If the volume or partition changed, load the filesystem of the new partition
  if (s_new_volume || s_partition != partition)
  {
    // Discs with partitions don't have PARTITION_NONE filesystems,
    // so let's not waste time trying to read one
    const bool reading_from_partition = partition != DiscIO::PARTITION_NONE;
    const bool disc_has_partitions = !s_volume->GetPartitions().empty();
    if (reading_from_partition != disc_has_partitions)
      return;

    s_new_volume = false;
    s_filesystem = DiscIO::CreateFileSystem(s_volume, partition);
    s_partition = partition;
    s_previous_file.clear();
  }

  // Do nothing if there is no valid file system
  if (!s_filesystem)
    return;

  const std::unique_ptr<DiscIO::FileInfo> file_info = s_filesystem->FindFileInfo(offset);

  // Do nothing if no file was found at that offset
  if (!file_info)
    return;

  const std::string path = file_info->GetPath();

  // Do nothing if we found the same file again
  if (s_previous_file == path)
    return;

  const std::string size_string = ThousandSeparate(file_info->GetSize() / 1000, 7);

  const std::string log_string = StringFromFormat("%s kB %s", size_string.c_str(), path.c_str());
  if (IsSoundFile(path))
    INFO_LOG(FILEMON, "%s", log_string.c_str());
  else
    WARN_LOG(FILEMON, "%s", log_string.c_str());

  // Update the last accessed file
  s_previous_file = path;
}

}  // namespace FileMonitor
