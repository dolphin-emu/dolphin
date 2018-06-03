// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/WiiRoot.h"

#include <string>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/Uids.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/SysConf.h"

namespace Core
{
static std::string s_temp_wii_root;

static void InitializeDeterministicWiiSaves()
{
  std::string save_path =
      Common::GetTitleDataPath(SConfig::GetInstance().GetTitleID(), Common::FROM_SESSION_ROOT);
  std::string user_save_path =
      Common::GetTitleDataPath(SConfig::GetInstance().GetTitleID(), Common::FROM_CONFIGURED_ROOT);
  if (Movie::IsRecordingInput())
  {
    if (NetPlay::IsNetPlayRunning() && !SConfig::GetInstance().bCopyWiiSaveNetplay)
    {
      Movie::SetClearSave(true);
    }
    else
    {
      // TODO: Check for the actual save data
      Movie::SetClearSave(!File::Exists(user_save_path + "/banner.bin"));
    }
  }

  if ((NetPlay::IsNetPlayRunning() && SConfig::GetInstance().bCopyWiiSaveNetplay) ||
      (Movie::IsMovieActive() && !Movie::IsStartingFromClearSave()))
  {
    // Copy the current user's save to the Blank NAND
    if (File::Exists(user_save_path + "/banner.bin"))
    {
      File::CopyDir(user_save_path, save_path);
    }
  }
}

void InitializeWiiRoot(bool use_temporary)
{
  if (use_temporary)
  {
    s_temp_wii_root = File::CreateTempDir();
    if (s_temp_wii_root.empty())
    {
      ERROR_LOG(IOS_FS, "Could not create temporary directory");
      return;
    }
    WARN_LOG(IOS_FS, "Using temporary directory %s for minimal Wii FS", s_temp_wii_root.c_str());
    File::SetUserPath(D_SESSION_WIIROOT_IDX, s_temp_wii_root);
  }
  else
  {
    File::SetUserPath(D_SESSION_WIIROOT_IDX, File::GetUserPath(D_WIIROOT_IDX));
  }
}

void ShutdownWiiRoot()
{
  if (!s_temp_wii_root.empty())
  {
    File::DeleteDirRecursively(s_temp_wii_root);
    s_temp_wii_root.clear();
  }
}

/// Copy a directory from host_source_path (on the host FS) to nand_target_path on the NAND.
///
/// Both paths should not have trailing slashes. To specify the NAND root, use "".
static bool CopySysmenuFilesToFS(IOS::HLE::FS::FileSystem* fs, const std::string& host_source_path,
                                 const std::string& nand_target_path)
{
  const auto entries = File::ScanDirectoryTree(host_source_path, false);
  for (const File::FSTEntry& entry : entries.children)
  {
    const std::string host_path = host_source_path + '/' + entry.virtualName;
    const std::string nand_path = nand_target_path + '/' + entry.virtualName;
    constexpr IOS::HLE::FS::Mode rw_mode = IOS::HLE::FS::Mode::ReadWrite;
    constexpr IOS::HLE::FS::Modes public_modes{rw_mode, rw_mode, rw_mode};

    if (entry.isDirectory)
    {
      fs->CreateDirectory(IOS::SYSMENU_UID, IOS::SYSMENU_GID, nand_path, 0, public_modes);
      if (!CopySysmenuFilesToFS(fs, host_path, nand_path))
        return false;
    }
    else
    {
      // Do not overwrite any existing files.
      if (fs->GetMetadata(IOS::SYSMENU_UID, IOS::SYSMENU_UID, nand_path).Succeeded())
        continue;

      File::IOFile host_file{host_path, "rb"};
      std::vector<u8> file_data(host_file.GetSize());
      if (!host_file.ReadBytes(file_data.data(), file_data.size()))
        return false;

      const auto nand_file =
          fs->CreateAndOpenFile(IOS::SYSMENU_UID, IOS::SYSMENU_GID, nand_path, public_modes);
      if (!nand_file || !nand_file->Write(file_data.data(), file_data.size()))
        return false;
    }
  }
  return true;
}

void InitializeWiiFileSystemContents()
{
  const auto fs = IOS::HLE::GetIOS()->GetFS();

  // Some games (such as Mario Kart Wii) assume that NWC24 files will always be present
  // even upon the first launch as they are normally created by the system menu.
  // Because we do not require the system menu to be run, WiiConnect24 files must be copied
  // to the NAND manually.
  if (!CopySysmenuFilesToFS(fs.get(), File::GetSysDirectory() + WII_USER_DIR, ""))
    WARN_LOG(CORE, "Failed to copy initial System Menu files to the NAND");

  if (s_temp_wii_root.empty())
    return;

  // Generate a SYSCONF with default settings for the temporary Wii NAND.
  SysConf sysconf{fs};
  sysconf.Save();

  InitializeDeterministicWiiSaves();
}

void CleanUpWiiFileSystemContents()
{
  if (s_temp_wii_root.empty())
    return;

  const u64 title_id = SConfig::GetInstance().GetTitleID();
  std::string save_path = Common::GetTitleDataPath(title_id, Common::FROM_SESSION_ROOT);
  std::string user_save_path = Common::GetTitleDataPath(title_id, Common::FROM_CONFIGURED_ROOT);
  std::string user_backup_path =
      File::GetUserPath(D_BACKUP_IDX) +
      StringFromFormat("%08x/%08x/", static_cast<u32>(title_id >> 32), static_cast<u32>(title_id));
  if (File::Exists(save_path + "/banner.bin") && SConfig::GetInstance().bEnableMemcardSdWriting)
  {
    // Backup the existing save just in case it's still needed.
    if (File::Exists(user_save_path + "/banner.bin"))
    {
      if (File::Exists(user_backup_path))
        File::DeleteDirRecursively(user_backup_path);
      File::CopyDir(user_save_path, user_backup_path);
      File::DeleteDirRecursively(user_save_path);
    }
    File::CopyDir(save_path, user_save_path);
    File::DeleteDirRecursively(save_path);
  }
}
}  // namespace Core
