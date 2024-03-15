// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <DolphinTool/ExtractCommand.h>
#include <OptionParser.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <future>
#include <iostream>
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DolphinTool
{
static void ExtractFile(const DiscIO::Volume& disc_volume, const DiscIO::Partition& partition,
                        const std::string& path, const std::string& out)
{
  const DiscIO::FileSystem* filesystem = disc_volume.GetFileSystem(partition);
  if (!filesystem)
    return;

  ExportFile(disc_volume, partition, filesystem->FindFileInfo(path).get(), out);
}

static std::unique_ptr<DiscIO::FileInfo> GetFileInfo(const DiscIO::Volume& disc_volume,
                                                     const DiscIO::Partition& partition,
                                                     const std::string& path)
{
  const DiscIO::FileSystem* filesystem = disc_volume.GetFileSystem(partition);
  std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path);
  return info;
}

static void ExtractDirectory(const DiscIO::Volume& disc_volume, const DiscIO::Partition& partition,
                             const std::string& path, const std::string& out, bool mute)
{
  const DiscIO::FileSystem* filesystem = disc_volume.GetFileSystem(partition);
  if (!filesystem)
    return;

  std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path);
  u32 size = info->GetTotalChildren();
  u32 files = 0;
  ExportDirectory(disc_volume, partition, *info, true, path, out,
                  [&files, &size, &mute](const std::string& current) {
                    files++;
                    const float progress =
                        static_cast<float>(files) / static_cast<float>(size) * 100;
                    if (!mute)
                      fmt::print("Extracting: {} | {}% \n", current, static_cast<int>(progress));
                    return false;
                  });
}

static bool ExtractSystemData(const DiscIO::Volume& disc_volume, const DiscIO::Partition& partition,
                              const std::string& out)
{
  return ExportSystemData(disc_volume, partition, out);
}

static void ExtractPartition(const DiscIO::Volume& disc_volume, const DiscIO::Partition& partition,
                             const std::string& out, bool mute)
{
  ExtractDirectory(disc_volume, partition, "", out + "/files", mute);
  ExtractSystemData(disc_volume, partition, out);
}

int Extract(const std::vector<std::string>& args)
{
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

  parser.add_option("-g", "--gameonly")
      .action("store_true")
      .help("Only extracts the DATA partition.");

  const optparse::Values& options = parser.parse_args(args);

  const bool mute = options.is_set_by_user("mute");
  const bool gameonly = options.is_set_by_user("gameonly");

  if (!options.is_set("input"))
  {
    fmt::print(std::cerr, "Error: No input image set\n");
    return EXIT_FAILURE;
  }
  const std::string& input_file_path = options["input"];

  if (!options.is_set("output"))
  {
    fmt::print(std::cerr, "Error: No output folder set\n");
    return EXIT_FAILURE;
  }
  const std::string& output_folder_path = options["output"];
  const std::string& singular_file_path = options["singular"];
  std::string& specific_partition = const_cast<std::string&>(options["partition"]);

  if (gameonly)
    specific_partition = std::string("data");

  if (const std::unique_ptr<DiscIO::Volume> disc_volume = DiscIO::CreateVolume(input_file_path);
      disc_volume->GetPartitions().empty())
  {
    ExtractPartition(*disc_volume, DiscIO::PARTITION_NONE, output_folder_path, mute);
  }
  else
  {
    bool extracted_one = false;

    for (DiscIO::Partition& p : disc_volume->GetPartitions())
    {
      if (const std::optional<u32> partition_type = disc_volume->GetPartitionType(p))
      {
        const std::string partition_name = DiscIO::NameForPartitionType(*partition_type, true);

        if (!specific_partition.empty() &&
            !Common::CaseInsensitiveEquals(specific_partition, partition_name))
          continue;

        extracted_one = true;

        std::string file;
        file.append(output_folder_path).append("/");

        if (!options.is_set("singular"))
        {
          file.append(partition_name);
          ExtractPartition(*disc_volume, p, file, mute);
        }
        else
        {
          if (auto file_info = GetFileInfo(*disc_volume, p, singular_file_path);
              file_info != nullptr)
          {
            file.append(singular_file_path);

            if (file_info->IsDirectory())
              ExtractDirectory(*disc_volume, p, singular_file_path, output_folder_path, mute);
            else
              ExtractFile(*disc_volume, p, singular_file_path, file);
          }
          else
          {
            extracted_one = false;
          }
        }
      }
    }

    if (!extracted_one)
    {
      if (options.is_set("singular"))
        fmt::print(std::cerr, "Error: No File/Folder was extracted.\n");
      else
        fmt::print(std::cerr, "Error: No partitions were extracted..\n");

      if (options.is_set("partition"))
        fmt::print(std::cerr, " Maybe you misspelled your specified partition?\n");
      fmt::print(std::cerr, "\n");
      return EXIT_FAILURE;
    }
  }

  fmt::print(std::cerr, "Finished Successfully!\n");
  return EXIT_SUCCESS;
}
}  // namespace DolphinTool
