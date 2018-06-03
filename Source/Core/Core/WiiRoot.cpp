// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/WiiRoot.h"

#include <cinttypes>
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
#include "Core/HW/WiiSave.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/Uids.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/SysConf.h"

namespace Core
{
static std::string s_temp_wii_root;

namespace FS = IOS::HLE::FS;

static void InitializeDeterministicWiiSaves(FS::FileSystem* session_fs)
{
  const u64 title_id = SConfig::GetInstance().GetTitleID();
  const auto configured_fs = FS::MakeFileSystem(FS::Location::Configured);
  if (Movie::IsRecordingInput())
  {
    if (NetPlay::IsNetPlayRunning() && !SConfig::GetInstance().bCopyWiiSaveNetplay)
    {
      Movie::SetClearSave(true);
    }
    else
    {
      // TODO: Check for the actual save data
      const std::string path = Common::GetTitleDataPath(title_id) + "/banner.bin";
      Movie::SetClearSave(!configured_fs->GetMetadata(IOS::PID_KERNEL, IOS::PID_KERNEL, path));
    }
  }

  if ((NetPlay::IsNetPlayRunning() && SConfig::GetInstance().bCopyWiiSaveNetplay) ||
      (Movie::IsMovieActive() && !Movie::IsStartingFromClearSave()))
  {
    // Copy the current user's save to the Blank NAND
    const auto user_save = WiiSave::MakeNandStorage(configured_fs.get(), title_id);
    const auto session_save = WiiSave::MakeNandStorage(session_fs, title_id);
    WiiSave::Copy(user_save.get(), session_save.get());
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
static bool CopySysmenuFilesToFS(FS::FileSystem* fs, const std::string& host_source_path,
                                 const std::string& nand_target_path)
{
  const auto entries = File::ScanDirectoryTree(host_source_path, false);
  for (const File::FSTEntry& entry : entries.children)
  {
    const std::string host_path = host_source_path + '/' + entry.virtualName;
    const std::string nand_path = nand_target_path + '/' + entry.virtualName;
    constexpr FS::Modes public_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::ReadWrite};

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

  InitializeDeterministicWiiSaves(fs.get());
}

void CleanUpWiiFileSystemContents()
{
  if (s_temp_wii_root.empty() || !SConfig::GetInstance().bEnableMemcardSdWriting)
    return;

  const u64 title_id = SConfig::GetInstance().GetTitleID();

  IOS::HLE::EmulationKernel* ios = IOS::HLE::GetIOS();
  const auto session_save = WiiSave::MakeNandStorage(ios->GetFS().get(), title_id);

  const auto configured_fs = FS::MakeFileSystem(FS::Location::Configured);
  const auto user_save = WiiSave::MakeNandStorage(configured_fs.get(), title_id);

  const std::string backup_path =
      File::GetUserPath(D_BACKUP_IDX) + StringFromFormat("/%016" PRIx64 ".bin", title_id);
  const auto backup_save = WiiSave::MakeDataBinStorage(&ios->GetIOSC(), backup_path, "w+b");

  // Backup the existing save just in case it's still needed.
  WiiSave::Copy(user_save.get(), backup_save.get());
  WiiSave::Copy(session_save.get(), user_save.get());
}
}  // namespace Core
