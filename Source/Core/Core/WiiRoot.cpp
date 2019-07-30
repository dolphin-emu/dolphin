// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/WiiRoot.h"

#include <cinttypes>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Core/ConfigManager.h"
#include "Core/HW/WiiSave.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/Uids.h"
#include "Core/Movie.h"
#include "Core/NetPlayClient.h"
#include "Core/SysConf.h"

namespace Core
{
namespace FS = IOS::HLE::FS;

static std::string s_temp_wii_root;

static void CopySave(FS::FileSystem* source, FS::FileSystem* dest, const u64 title_id)
{
  dest->CreateDirectory(IOS::PID_KERNEL, IOS::PID_KERNEL, Common::GetTitleDataPath(title_id), 0,
                        {IOS::HLE::FS::Mode::ReadWrite, IOS::HLE::FS::Mode::ReadWrite,
                         IOS::HLE::FS::Mode::ReadWrite});
  const auto source_save = WiiSave::MakeNandStorage(source, title_id);
  const auto dest_save = WiiSave::MakeNandStorage(dest, title_id);
  WiiSave::Copy(source_save.get(), dest_save.get());
}

static bool CopyNandFile(FS::FileSystem* source_fs, const std::string& source_file,
                         FS::FileSystem* dest_fs, const std::string& dest_file)
{
  const auto last_slash = dest_file.find_last_of('/');
  if (last_slash != std::string::npos && last_slash > 0)
  {
    const std::string dir = dest_file.substr(0, last_slash);
    dest_fs->CreateDirectory(IOS::PID_KERNEL, IOS::PID_KERNEL, dir, 0,
                             {IOS::HLE::FS::Mode::ReadWrite, IOS::HLE::FS::Mode::ReadWrite,
                              IOS::HLE::FS::Mode::ReadWrite});
  }

  auto source_handle =
      source_fs->OpenFile(IOS::PID_KERNEL, IOS::PID_KERNEL, source_file, IOS::HLE::FS::Mode::Read);
  auto dest_handle =
      dest_fs->CreateAndOpenFile(IOS::PID_KERNEL, IOS::PID_KERNEL, source_file,
                                 {IOS::HLE::FS::Mode::ReadWrite, IOS::HLE::FS::Mode::ReadWrite,
                                  IOS::HLE::FS::Mode::ReadWrite});
  if (!source_handle || !dest_handle)
    return false;

  std::vector<u8> buffer(source_handle->GetStatus()->size);
  if (!source_handle->Read(buffer.data(), buffer.size()))
    return false;
  if (!dest_handle->Write(buffer.data(), buffer.size()))
    return false;

  return true;
}

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
    auto* sync_fs = NetPlay::GetWiiSyncFS();
    auto& sync_titles = NetPlay::GetWiiSyncTitles();
    if (sync_fs)
    {
      for (const u64 title : sync_titles)
      {
        CopySave(sync_fs, session_fs, title);
      }

      // Copy Mii data
      if (!CopyNandFile(sync_fs, Common::GetMiiDatabasePath(), session_fs,
                        Common::GetMiiDatabasePath()))
      {
        WARN_LOG(CORE, "Failed to copy Mii database to the NAND");
      }
    }
    else
    {
      if (NetPlay::IsSyncingAllWiiSaves())
      {
        for (const u64 title : sync_titles)
        {
          CopySave(configured_fs.get(), session_fs, title);
        }
      }
      else
      {
        CopySave(configured_fs.get(), session_fs, title_id);
      }

      // Copy Mii data
      if (!CopyNandFile(configured_fs.get(), Common::GetMiiDatabasePath(), session_fs,
                        Common::GetMiiDatabasePath()))
      {
        WARN_LOG(CORE, "Failed to copy Mii database to the NAND");
      }
    }
  }
}

void InitializeWiiRoot(bool use_temporary)
{
  if (use_temporary)
  {
    s_temp_wii_root = File::GetUserPath(D_USER_IDX) + "WiiSession" DIR_SEP;
    WARN_LOG(IOS_FS, "Using temporary directory %s for minimal Wii FS", s_temp_wii_root.c_str());

    // If directory exists, make a backup
    if (File::Exists(s_temp_wii_root))
    {
      const std::string backup_path =
          s_temp_wii_root.substr(0, s_temp_wii_root.size() - 1) + ".backup" DIR_SEP;
      WARN_LOG(IOS_FS, "Temporary Wii FS directory exists, moving to backup...");

      // If backup exists, delete it as we don't want a mess
      if (File::Exists(backup_path))
      {
        WARN_LOG(IOS_FS, "Temporary Wii FS backup directory exists, deleting...");
        File::DeleteDirRecursively(backup_path);
      }

      File::CopyDir(s_temp_wii_root, backup_path, true);
    }

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
  if (s_temp_wii_root.empty() || !SConfig::GetInstance().bEnableMemcardSdWriting ||
      NetPlay::GetWiiSyncFS())
  {
    return;
  }

  IOS::HLE::EmulationKernel* ios = IOS::HLE::GetIOS();
  const auto configured_fs = FS::MakeFileSystem(FS::Location::Configured);

  // Copy back Mii data
  if (!CopyNandFile(ios->GetFS().get(), Common::GetMiiDatabasePath(), configured_fs.get(),
                    Common::GetMiiDatabasePath()))
  {
    WARN_LOG(CORE, "Failed to copy Mii database to the NAND");
  }

  for (const u64 title_id : ios->GetES()->GetInstalledTitles())
  {
    const auto session_save = WiiSave::MakeNandStorage(ios->GetFS().get(), title_id);

    // FS won't write the save if the directory doesn't exist
    const std::string title_path = Common::GetTitleDataPath(title_id);
    if (!configured_fs->GetMetadata(IOS::PID_KERNEL, IOS::PID_KERNEL, title_path))
    {
      configured_fs->CreateDirectory(IOS::PID_KERNEL, IOS::PID_KERNEL, title_path, 0,
                                     {IOS::HLE::FS::Mode::ReadWrite, IOS::HLE::FS::Mode::ReadWrite,
                                      IOS::HLE::FS::Mode::ReadWrite});
    }

    const auto user_save = WiiSave::MakeNandStorage(configured_fs.get(), title_id);

    const std::string backup_path =
        fmt::format("{}/{:016x}.bin", File::GetUserPath(D_BACKUP_IDX), title_id);
    const auto backup_save = WiiSave::MakeDataBinStorage(&ios->GetIOSC(), backup_path, "w+b");

    // Backup the existing save just in case it's still needed.
    WiiSave::Copy(user_save.get(), backup_save.get());
    WiiSave::Copy(session_save.get(), user_save.get());
  }
}
}  // namespace Core
