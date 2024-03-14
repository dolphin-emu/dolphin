// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/DiscExtractor.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include <DolphinTool/ExtractCommand.h>
#include <OptionParser.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <future>
#include <iostream>

namespace DolphinTool {
std::shared_ptr<DiscIO::Volume> disc_volume;
bool mute;

static std::unique_ptr<DiscIO::FileInfo>
GetFileInfo(const DiscIO::Partition &partition, const std::string &path) {
  const DiscIO::FileSystem *filesystem = disc_volume->GetFileSystem(partition);
  std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path);
  return info;
}

bool ExtractFile(const DiscIO::Partition &partition, const std::string &path,
                 const std::string &out) {
  const DiscIO::FileSystem *filesystem = disc_volume->GetFileSystem(partition);
  if (!filesystem)
    return false;

  return DiscIO::ExportFile(*disc_volume, partition,
                            filesystem->FindFileInfo(path).get(), out);
}

static void ExtractDirectory(const DiscIO::Partition &partition,
                             const std::string &path, const std::string &out) {
  const DiscIO::FileSystem *filesystem = disc_volume->GetFileSystem(partition);
  if (!filesystem)
    return;

  std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path);
  u32 size = info->GetTotalChildren();
  u32 files = 0;
  DiscIO::ExportDirectory(*disc_volume, partition, *info, true, path, out,
                          [&files, &size](const std::string &current) {
                            files++;
                            float prog = ((float)files / (float)size) * 100;
                            if (!mute)
                              fmt::print("Extracting: {} | {}% \n", current,
                                         (int)prog);
                            return false;
                          });
}

static bool ExtractSystemData(const DiscIO::Partition &partition,
                              const std::string &out) {
  return DiscIO::ExportSystemData(*disc_volume, partition, out);
}

static void ExtractPartition(const DiscIO::Partition &partition,
                             const std::string &out) {
  ExtractDirectory(partition, "", out + "/files");
  ExtractSystemData(partition, out);
}

int Extract(const std::vector<std::string> &args) {
  optparse::OptionParser parser;

  parser.usage("usage: extract [options]...");

  parser.add_option("-i", "--input")
      .type("string")
      .action("store")
      .help("Path to disc image FILE. *")
      .metavar("FILE");

  parser.add_option("-o", "--output")
      .type("string")
      .action("store")
      .help("Path to the destination FOLDER. *")
      .metavar("FOLDER");

  parser.add_option("-p", "--partition")
      .type("string")
      .action("store")
      .help("Which specific partition you want to extract.");

  parser.add_option("-s", "--singular")
      .type("string")
      .action("store")
      .help("Which specific file/directory you want to extract.");

  parser.add_option("-m", "--mute")
      .action("store_true")
      .help("Mute all messages except for errors.");

  const optparse::Values &options = parser.parse_args(args);

  mute = static_cast<bool>(options.get("mute"));

  if (!options.is_set("input")) {
    fmt::print(std::cerr, "Error: No input image set\n");
    return EXIT_FAILURE;
  }
  const std::string &input_file_path = options["input"];

  if (!options.is_set("output")) {
    fmt::print(std::cerr, "Error: No output folder set\n");
    return EXIT_FAILURE;
  }
  const std::string &output_folder_path = options["output"];
  const std::string &singular_file_path = options["singular"];
  const std::string &specific_partition = options["partition"];
  disc_volume = DiscIO::CreateVolume(input_file_path);

  if (disc_volume->GetPartitions().empty()) {
    ExtractPartition(DiscIO::PARTITION_NONE, output_folder_path);
  } else {
    bool extracted_one = false;

    for (DiscIO::Partition &p : disc_volume->GetPartitions()) {
      if (const std::optional<u32> partition_type =
              disc_volume->GetPartitionType(p)) {
        const std::string partition_name =
            DiscIO::NameForPartitionType(*partition_type, true);

        if (options.is_set("partition") &&
            !Common::CaseInsensitiveEquals(specific_partition, partition_name))
          continue;

        extracted_one = true;

        if (!options.is_set("singular")) {
          ExtractPartition(p, output_folder_path + '/' + partition_name);
        } else {
          auto f = GetFileInfo(p, singular_file_path);
          if (f != nullptr) {
            const std::string file =
                output_folder_path + "/" + singular_file_path;
            if (f->IsDirectory())
              ExtractDirectory(p, singular_file_path, output_folder_path);
            else
              ExtractFile(p, singular_file_path, file);
          } else {
            extracted_one = false;
          }
        }
      }
    }

    if (!extracted_one) {
      if (options.is_set("singular"))
        fmt::print(std::cerr, "Error: No File/Folder was extracted.\n");
      else
        fmt::print(std::cerr, "Error: No partitions were extracted..\n");

      if (options.is_set("partition"))
        fmt::print(std::cerr,
                   " Maybe you misspelled your specified partition?\n");
      fmt::print(std::cerr, "\n");
      return EXIT_FAILURE;
    }
  }

  fmt::print(std::cerr, "Finished Successfully!\n");
  return EXIT_SUCCESS;
}

} // namespace DolphinTool
