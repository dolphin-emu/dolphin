// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscConverter.h"
#include <DiscIO/VolumeGC.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include "Blob.h"
#include "ScrubbedBlob.h"
#include "WIABlob.h"

using namespace std;
namespace DiscIO
{
void ConvertDisc(
    std::string original_path, std::string dst_path,
    DiscIO::BlobType format,
    DiscIO::WIARVZCompressionType compression_type,
    int compression_level, int block_size, bool remove_junk,
    bool delete_original)
{
  cout << "Converting " + original_path + " to " + dst_path + " ..." << endl;

  // dst_path = dst_path + ".rvz";

  std::unique_ptr<DiscIO::BlobReader> blob_reader;
  bool scrub_current_file = false; /*hardcoded not to scrub for now*/

  if (scrub_current_file)
  {
    blob_reader = DiscIO::ScrubbedBlob::Create(original_path);
    if (!blob_reader)
    {
      /*TODO ADD CLI PROMPT*/
      scrub_current_file = false;

      // const int result =
      //    ModalMessageBox::warning(this, tr("Question"),
      //        tr("Failed to remove junk data from file \"%1\".\n\n"
      //            "Would you like to convert it without removing junk data?")
      //        .arg(QString::fromStdString(original_path)),
      //        QMessageBox::Ok | QMessageBox::Abort);

      // if (result == QMessageBox::Ok)
      //    scrub_current_file = false;
      // else
      //    return;
    }
  }

  if (!scrub_current_file)
    blob_reader = DiscIO::CreateBlobReader(original_path);

  if (!blob_reader)
  {
    std::cout << "Error: Failed to open the input file: " << endl;

    return;
  }
  else
  {
    const auto callback = [](const std::string& text, float percent) {
      std::string progress = text + ". Progress " + fmt::format("{:.0f}", percent * 100);
      std::cout << (progress) << "%" << '\r';
      // progress_dialog.SetValue(percent * 100);
      return true;
      //! progress_dialog.WasCanceled();
    };

    std::future<bool> success;
    // DiscIO::Platform platform = DiscIO::Platform::WiiDisc;
    switch (format)
    {
    case DiscIO::BlobType::PLAIN:
      success = std::async(std::launch::async, [&] {
        cout << "Converting to PLAIN ISO" << endl;
        const bool good =
            DiscIO::ConvertToPlain(blob_reader.get(), original_path, dst_path, callback);
        /*       progress_dialog.Reset();*/
        return good;
      });
      break;

    case DiscIO::BlobType::GCZ:
      success = std::async(std::launch::async, [&] {
        std::string d = original_path.find("wii") != std::string::npos ? "Wii" : "GC";
        cout << "Converting to GCZ " + d;
        const bool good = DiscIO::ConvertToGCZ(
            blob_reader.get(), original_path, dst_path,
            original_path.find("wii") != std::string::npos ? 1 : 0, block_size, callback);
        /*  progress_dialog.Reset();*/
        return good;
      });
      break;

    case DiscIO::BlobType::WIA:
    case DiscIO::BlobType::RVZ:
      cout << "Converting to RVZ" << endl;
      success = std::async(std::launch::async, [&] {
        const bool good = DiscIO::ConvertToWIAOrRVZ(
            blob_reader.get(), original_path, dst_path, format == DiscIO::BlobType::RVZ,
            compression_type, compression_level, block_size, callback);
        /*    progress_dialog.Reset();*/
        return good;
      });
      break;

    default:
      /* ASSERT(false);*/
      break;
    }

    /*progress_dialog.GetRaw()->exec();*/
    if (!success.get())
    {
      // ModalMessageBox::critical(this, tr("Error"),
      //    tr("Dolphin failed to complete the requested action."));
      return;
    }

    if (delete_original)
    {
      cout << "deleting: " + original_path << endl;
      std::filesystem::remove(original_path);
    }

    // DiscIO::ConvertToWIAOrRVZ(blob_reader.get(), original_path, dst_path,
    //                          format == DiscIO::BlobType::RVZ, compression, compression_level,
    //                          block_size, callback);
    // return;
  }
}
}  // namespace DiscIO
