// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "Core/Movie.h"
#include "Core/WiiRoot.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Core
{
static std::string s_temp_wii_root;

static void InitializeDeterministicWiiSaves()
{
  std::string save_path =
      Common::GetTitleDataPath(SConfig::GetInstance().m_title_id, Common::FROM_SESSION_ROOT);

  // TODO: Force the game to save to another location, instead of moving the user's save.
  if (Movie::IsPlayingInput() && Movie::IsConfigSaved() && Movie::IsStartingFromClearSave())
  {
    if (File::Exists(save_path + "banner.bin"))
    {
      if (File::Exists(save_path + "../backup/"))
      {
        // The last run of this game must have been to play back a movie, so their save is already
        // backed up.
        File::DeleteDirRecursively(save_path);
      }
      else
      {
#ifdef _WIN32
        MoveFile(UTF8ToTStr(save_path).c_str(), UTF8ToTStr(save_path + "../backup/").c_str());
#else
        File::CopyDir(save_path, save_path + "../backup/");
        File::DeleteDirRecursively(save_path);
#endif
      }
    }
  }
  else if (File::Exists(save_path + "../backup/"))
  {
    // Delete the save made by a previous movie, and copy back the user's save.
    if (File::Exists(save_path + "banner.bin"))
      File::DeleteDirRecursively(save_path);
#ifdef _WIN32
    MoveFile(UTF8ToTStr(save_path + "../backup/").c_str(), UTF8ToTStr(save_path).c_str());
#else
    File::CopyDir(save_path + "../backup/", save_path);
    File::DeleteDirRecursively(save_path + "../backup/");
#endif
  }
}

void InitializeWiiRoot(bool use_temporary)
{
  ShutdownWiiRoot();

  if (use_temporary)
  {
    s_temp_wii_root = File::CreateTempDir();
    if (s_temp_wii_root.empty())
    {
      ERROR_LOG(IOS_FILEIO, "Could not create temporary directory");
      return;
    }
    File::CopyDir(File::GetSysDirectory() + WII_USER_DIR, s_temp_wii_root);
    WARN_LOG(IOS_FILEIO, "Using temporary directory %s for minimal Wii FS",
             s_temp_wii_root.c_str());
    static bool s_registered;
    if (!s_registered)
    {
      s_registered = true;
      atexit(ShutdownWiiRoot);
    }
    File::SetUserPath(D_SESSION_WIIROOT_IDX, s_temp_wii_root);
    // Generate a SYSCONF with default settings for the temporary Wii NAND.
    SysConf sysconf{Common::FromWhichRoot::FROM_SESSION_ROOT};
    sysconf.Save();
  }
  else
  {
    File::SetUserPath(D_SESSION_WIIROOT_IDX, File::GetUserPath(D_WIIROOT_IDX));
  }

  InitializeDeterministicWiiSaves();
}

void ShutdownWiiRoot()
{
  if (!s_temp_wii_root.empty())
  {
    File::DeleteDirRecursively(s_temp_wii_root);
    s_temp_wii_root.clear();
  }
}
}
