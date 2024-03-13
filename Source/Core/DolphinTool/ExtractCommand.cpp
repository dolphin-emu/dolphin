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
std::shared_ptr<DiscIO::Volume> disc_volume;

static void ExtractDirectory(const DiscIO::Partition& partition, const std::string& path,
                             const std::string& out)
{
  const DiscIO::FileSystem* filesystem = disc_volume->GetFileSystem(partition);
  if (!filesystem)
    return;

  std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path);
  u32 size = info->GetTotalChildren();

  std::future<void> future = std::async(std::launch::async, [&] {
  u32 files = 0;

    DiscIO::ExportDirectory(*disc_volume, partition, *info, true, path, out,
                            [&files, &size](const std::string& current) {
                              files++;
                              float prog = ((float)files / (float)size) * 100;
                              fmt::print("Extracting: {} | {}% \n", current, (int)prog);
                              return false;
                            });
  });
}

static bool ExtractSystemData(const DiscIO::Partition& partition, const std::string& out)
{
  return DiscIO::ExportSystemData(*disc_volume, partition, out);
}

static void ExtractPartition(const DiscIO::Partition& partition, const std::string& out)
{
  ExtractDirectory(partition, "", out + "/files");
  ExtractSystemData(partition, out);
}

int Extract(const std::vector<std::string>& args)
{
  optparse::OptionParser parser;

  parser.usage("usage: extract [options]...");

  parser.add_option("-i", "--input")
      .type("string")
      .action("store")
      .help("Path to disc image FILE.")
      .metavar("FILE");

  parser.add_option("-o", "--output")
      .type("string")
      .action("store")
      .help("Path to the destination FILE.")
      .metavar("FILE");

  const optparse::Values& options = parser.parse_args(args);

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

  disc_volume = DiscIO::CreateVolume(input_file_path);

  if (disc_volume->GetPartitions().empty())
  {
    ExtractPartition(DiscIO::PARTITION_NONE, output_folder_path);
  }
  else
  {
    for (DiscIO::Partition& p : disc_volume->GetPartitions())
    {
      if (const std::optional<u32> partition_type = disc_volume->GetPartitionType(p))
      {
        const std::string partition_name = DiscIO::NameForPartitionType(*partition_type, true);
        ExtractPartition(p, output_folder_path + '/' + partition_name);
      }
    }
  }

  return 0;
}

}  // namespace DolphinTool
