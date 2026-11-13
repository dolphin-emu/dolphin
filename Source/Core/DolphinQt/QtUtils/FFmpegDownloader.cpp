// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(DYNAMICALLY_LOAD_FFMPEG)

#include "DolphinQt/QtUtils/FFmpegDownloader.h"

#include <thread>

#include <QApplication>

#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "Common/FFmpegLoader.h"
#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"

#include "DolphinQt/QtUtils/ParallelProgressDialog.h"
#include "DolphinQt/QtUtils/RunOnObject.h"

namespace
{

class FFmpegDownloader : public QObject
{
public:
  bool Download();
};

bool FFmpegDownloader::Download()
{
  constexpr auto url =
      "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n9.0-latest-"
#if defined(_M_ARM_64)
      "winarm64"
#else
      "win64"
#endif
      "-gpl-shared-9.0.zip";

  Common::HttpRequest::Response result;
  bool canceled = false;

  {
    ParallelProgressDialog dialog{tr("Downloading..."), tr("Cancel"), 0, 100};
    dialog.GetRaw()->setWindowModality(Qt::WindowModal);
    dialog.GetRaw()->setWindowTitle(tr("FFmpeg"));

    const auto update_dialog = [&](s64 dltotal, s64 dlnow, s64, s64) {
      if (dltotal != 0)
        dialog.SetValue(int(dlnow * 100 / dltotal));
      if (dialog.WasCanceled())
      {
        canceled = true;
        return false;
      }
      return true;
    };

    const std::jthread download_thread{[&] {
      constexpr auto timeout = std::chrono::milliseconds{3000};
      Common::HttpRequest request(timeout, update_dialog);
      request.FollowRedirects(1);
      result = request.Get(url);
      dialog.Reset();
    }};

    dialog.GetRaw()->exec();
  }

  Common::ScopeGuard show_failure{[] { PanicAlertFmtT("Failed to download FFmpeg."); }};

  if (!result)
  {
    if (canceled)
      show_failure.Dismiss();
    return false;
  }

  void* zip_reader = mz_zip_reader_create();
  if (!zip_reader)
    return false;

  auto& buffer = *result;
  if (mz_zip_reader_open_buffer(zip_reader, buffer.data(), s32(buffer.size()), 0) != MZ_OK)
  {
    ERROR_LOG_FMT(COMMON, "mz_zip_reader_open_buffer");
    return false;
  };
  Common::ScopeGuard file_guard{[&] { mz_zip_reader_delete(&zip_reader); }};

  bool saved_dlls = false;

  const auto library_dir = Common::GetSharedFFmpegPath();

  do
  {
    mz_zip_file* info{};
    if (mz_zip_reader_entry_get_info(zip_reader, &info) != MZ_OK || !info->uncompressed_size)
      continue;

    const std::string_view path{info->filename};
    if (!path.ends_with(".dll"))
      continue;

    INFO_LOG_FMT(CORE, "Extracting dll: {}", info->filename);

    saved_dlls = true;

    std::string filename;
    std::string ext;
    SplitPath(path, nullptr, &filename, &ext);

    const auto save_path = fmt::format("{}{}{}", library_dir, filename, ext);
    if (mz_zip_reader_entry_save_file(zip_reader, save_path.c_str()) != MZ_OK)
    {
      ERROR_LOG_FMT(COMMON, "mz_zip_reader_entry_save_file");
      return false;
    }

  } while (mz_zip_reader_goto_next_entry(zip_reader) != MZ_END_OF_LIST);

  if (saved_dlls)
    show_failure.Dismiss();

  return saved_dlls;
}

}  // namespace

namespace QtUtils
{

bool AskToDownloadFFmpeg()
{
  Common::SetSharedFFmpegLoadFailureCallback({});

  const bool should_download = AskYesNoFmtT("FFmpeg libraries must be downloaded.\n\n"
                                            "Automatically download from github.com now? (~80 MB)");
  if (!should_download)
    return false;

  RunOnObject(QApplication::instance(), [&] {
    FFmpegDownloader ffmpeg_downloader;
    ffmpeg_downloader.Download();
    return true;
  });

  return true;
}

}  // namespace QtUtils

#endif
