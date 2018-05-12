// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/WiiRoot.h"

#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/IOS/IOS.h"
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

void InitializeWiiFileSystemContents()
{
  if (s_temp_wii_root.empty())
    return;

  File::CopyDir(File::GetSysDirectory() + WII_USER_DIR, s_temp_wii_root);

  // Generate a SYSCONF with default settings for the temporary Wii NAND.
  SysConf sysconf{IOS::HLE::GetIOS()->GetFS()};
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
