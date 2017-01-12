// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/SysConf.h"
#include "Core/WiiRoot.h"

namespace Core
{
static std::string s_temp_wii_root;

void InitializeWiiRoot(bool use_temporary)
{
  ShutdownWiiRoot();
  if (use_temporary)
  {
    s_temp_wii_root = File::CreateTempDir();
    if (s_temp_wii_root.empty())
    {
      ERROR_LOG(WII_IPC_FILEIO, "Could not create temporary directory");
      return;
    }
    File::CopyDir(File::GetSysDirectory() + WII_USER_DIR, s_temp_wii_root);
    WARN_LOG(WII_IPC_FILEIO, "Using temporary directory %s for minimal Wii FS",
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
