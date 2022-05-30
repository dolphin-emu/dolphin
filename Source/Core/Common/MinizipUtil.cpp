// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/MinizipUtil.h"

#include <ctime>  // required by minizip includes
#include <string>

#include <mz_strm.h>
#include <mz_strm_os.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "Common/ScopeGuard.h"

namespace Common
{
bool PackDirectoryToZip(const std::string& directory_path, const std::string& zip_path)
{
  void* stream = nullptr;
  mz_stream_os_create(&stream);
  if (!stream)
    return false;

  Common::ScopeGuard delete_stream_guard([&stream] { mz_stream_os_delete(&stream); });

  if (mz_stream_os_open(stream, zip_path.c_str(), MZ_OPEN_MODE_CREATE) != MZ_OK)
    return false;

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

  if (mz_zip_writer_add_path(writer, directory_path.c_str(), nullptr, 0, 1) != MZ_OK)
    return false;

  return true;
}

bool UnpackZipToDirectory(const std::string& zip_path, const std::string& directory_path)
{
  void* reader = nullptr;
  mz_zip_reader_create(&reader);
  if (!reader)
    return false;

  Common::ScopeGuard delete_reader_guard([&reader] { mz_zip_reader_delete(&reader); });

  mz_zip_reader_set_encoding(reader, MZ_ENCODING_UTF8);

  if (mz_zip_reader_open_file(reader, zip_path.c_str()) != MZ_OK)
    return false;

  Common::ScopeGuard close_reader_guard([&reader] { mz_zip_reader_close(reader); });

  if (mz_zip_reader_save_all(reader, directory_path.c_str()) != MZ_OK)
    return false;

  return true;
}
}  // namespace Common
