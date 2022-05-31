// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/ImportExportUserdir.h"

#include <ctime>  // required by minizip includes
#include <string>

#include <mz_strm.h>
#include <mz_strm_os.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/Flag.h"
#include "Common/MinizipUtil.h"
#include "Common/ScopeGuard.h"

namespace UICommon
{
bool ImportUserDir(const std::string& archive_path)
{
  const auto& directory_path = File::GetUserPath(D_USER_IDX);

  const auto root = File::ScanDirectoryTree(directory_path, false);
  for (const auto& file : root.children)
  {
    // Skip the Logs directory, it likely contains a file that we're currently writing to.
    if (file.isDirectory && Common::CaseInsensitiveEquals(file.virtualName, "Logs"))
      continue;

    if (file.isDirectory)
    {
      if (!File::DeleteDirRecursively(file.physicalName))
        return false;
    }
    else
    {
      if (!File::Delete(file.physicalName))
        return false;
    }
  }

  if (!Common::UnpackZipToDirectory(archive_path, directory_path))
    return false;

  // Need to reload the configuration now
  Config::Reload();
  return true;
}

static bool ExportUserDirRecursive(void** writer, const File::FSTEntry& file,
                                   const std::string& root_path,
                                   Common::Flag* cancel_requested_flag)
{
  if (cancel_requested_flag->IsSet())
    return false;

  if (file.isDirectory)
  {
    const auto dir = File::ScanDirectoryTree(file.physicalName, false);

    // if the directory has no children we need to explicitly add it to the zip, otherwise it will
    // get implicitly added by its children
    if (!dir.children.empty())
    {
      for (const auto& child : dir.children)
      {
        if (!ExportUserDirRecursive(writer, child, root_path, cancel_requested_flag))
          return false;
      }
      return true;
    }
  }

  auto error_code =
      mz_zip_writer_add_path(*writer, file.physicalName.c_str(), root_path.c_str(), 0, 0);
  if (error_code != MZ_OK)
    return false;

  return true;
}

bool ExportUserDir(const std::string& archive_path, Common::Flag* cancel_requested_flag)
{
  // Make sure the current settings are saved to disk so they'll get packed up.
  Config::Save();

  const std::string& directory_path = File::GetUserPath(D_USER_IDX);

  void* stream = nullptr;
  mz_stream_os_create(&stream);
  if (!stream)
    return false;

  Common::ScopeGuard delete_stream_guard([&stream] { mz_stream_os_delete(&stream); });

  if (mz_stream_os_open(stream, archive_path.c_str(), MZ_OPEN_MODE_CREATE) != MZ_OK)
    return false;

  Common::ScopeGuard delete_zip_on_failure_guard([&archive_path] { File::Delete(archive_path); });
  Common::ScopeGuard close_stream_guard([&stream] { mz_stream_os_close(stream); });

  void* writer = nullptr;
  mz_zip_writer_create(&writer);
  if (!writer)
    return false;

  Common::ScopeGuard delete_writer_guard([&writer] { mz_zip_writer_delete(&writer); });

  mz_zip_writer_set_compress_method(writer, Z_DEFLATED);
  mz_zip_writer_set_compress_level(writer, MZ_COMPRESS_LEVEL_NORMAL);

  if (mz_zip_writer_open(writer, stream, 0) != MZ_OK)
    return false;

  Common::ScopeGuard close_writer_guard([&writer] { mz_zip_writer_close(writer); });

  const auto root = File::ScanDirectoryTree(directory_path, false);

  for (const auto& file : root.children)
  {
    // Skip the Logs directory, it's not particularly useful to backup and likely contains a file
    // that we're currently writing to.
    if (file.isDirectory && Common::CaseInsensitiveEquals(file.virtualName, "Logs"))
      continue;

    if (!ExportUserDirRecursive(&writer, file, directory_path, cancel_requested_flag))
      return false;
  }

  delete_zip_on_failure_guard.Dismiss();
  return true;
}
}  // namespace UICommon
