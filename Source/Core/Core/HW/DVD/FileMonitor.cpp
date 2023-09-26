// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DVD/FileMonitor.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"

#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace FileMonitor
{
// Filtered files
static bool IsSoundFile(const std::string& filename)
{
  std::string extension;
  SplitPath(filename, nullptr, nullptr, &extension);
  Common::ToLower(&extension);

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

FileLogger::FileLogger() = default;

FileLogger::~FileLogger() = default;

void FileLogger::Log(const DiscIO::Volume& volume, const DiscIO::Partition& partition, u64 offset)
{
  // Do nothing if the log isn't selected
  if (!Common::Log::LogManager::GetInstance()->IsEnabled(Common::Log::LogType::FILEMON,
                                                         Common::Log::LogLevel::LWARNING))
  {
    return;
  }

  const DiscIO::FileSystem* file_system = volume.GetFileSystem(partition);

  // Do nothing if there is no valid file system
  if (!file_system)
    return;

  const std::unique_ptr<DiscIO::FileInfo> file_info = file_system->FindFileInfo(offset);

  // Do nothing if no file was found at that offset
  if (!file_info)
    return;

  const u64 file_offset = file_info->GetOffset();

  // Do nothing if we found the same file again
  if (m_previous_partition == partition && m_previous_file_offset == file_offset)
    return;

  const std::string size_string = Common::ThousandSeparate(file_info->GetSize() / 1000, 7);
  const std::string path = file_info->GetPath();
  const std::string log_string = fmt::format("{} kB {}", size_string, path);
  if (IsSoundFile(path))
    INFO_LOG_FMT(FILEMON, "{}", log_string);
  else
    WARN_LOG_FMT(FILEMON, "{}", log_string);

  // Update the last accessed file
  m_previous_partition = partition;
  m_previous_file_offset = file_offset;
}

}  // namespace FileMonitor
