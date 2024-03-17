// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <DolphinTool/ExtractCommand.h>
#include <OptionParser.h>
#include <filesystem>
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

static int VolumeSupported(const DiscIO::Volume& disc_volume)
{
  switch (disc_volume.GetVolumeType())
  {
  case DiscIO::Platform::WiiWAD:
    fmt::println(std::cerr, "Error: Wii WADs are not supported.");
    return EXIT_FAILURE;
  case DiscIO::Platform::ELFOrDOL:
    fmt::println(std::cerr,
                 "Error: *.elf or *.dol have no filesystem and are therefore not supported.");
    return EXIT_FAILURE;
  case DiscIO::Platform::WiiDisc:
  case DiscIO::Platform::GameCubeDisc:
    return EXIT_SUCCESS;
  default:
    fmt::println("Error: Unknown volume type.");
    return EXIT_FAILURE;
  }
}

static void ExtractDirectory(const DiscIO::Volume& disc_volume, const DiscIO::Partition& partition,
                             const std::string& path, const std::string& out, bool mute)
{
  const DiscIO::FileSystem* filesystem = disc_volume.GetFileSystem(partition);
  if (!filesystem)
    return;

  const std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path);
  u32 size = info->GetTotalChildren();
  u32 files = 0;
  ExportDirectory(disc_volume, partition, *info, true, path, out,
                  [&files, &size, &mute](const std::string& current) {
                    files++;
                    const float progress =
                        static_cast<float>(files) / static_cast<float>(size) * 100;
                    if (!mute)
                      fmt::println("Extracting: {} | {}%", current, static_cast<int>(progress));
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

static void ListVolume(const DiscIO::Volume& disc_volume, const std::string& path,
                       std::string& specific_partition_name)
{
  std::ofstream output_file;
  output_file.open("ls.txt");

  for (DiscIO::Partition& p : disc_volume.GetPartitions())
  {
    const std::optional<u32> partition_type = disc_volume.GetPartitionType(p);
    if (!partition_type)
    {
      fmt::println(std::cerr, "Error: Could not get partition type.");
      return;
    }
    const std::string partition_name = DiscIO::NameForPartitionType(*partition_type, true);

    if (!specific_partition_name.empty() &&
        !Common::CaseInsensitiveEquals(partition_name, specific_partition_name))
      continue;

    const std::string partition_start = fmt::format("///PARTITION: {}///\n", partition_name);
    fmt::print("{}", partition_start);
    output_file.write(partition_start.c_str(), static_cast<long>(partition_start.length()));
    const DiscIO::FileSystem* filesystem = disc_volume.GetFileSystem(p);
    const std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path);

    if (!info)
    {
      fmt::println("Error: {} does not exist in this partition.", path);
      return;
    }

    for (auto it = info->begin(); it != info->end(); ++it)
    {
      const std::string file_name = fmt::format("{}\n", it->GetName());
      fmt::print("{}", file_name);
      output_file.write(file_name.c_str(), static_cast<long>(file_name.length()));
    }
  }
  output_file.close();
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
  parser.add_option("-s", "--single")
      .type("string")
      .action("store")
      .help("Which specific file/directory you want to extract.");
  parser.add_option("-l", "--list")
      .type("string")
      .action("store")
      .help("List all files in volume/partition and print it out to a text file (ls.txt).");
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
    fmt::println(std::cerr, "Error: No input image set");
    return EXIT_FAILURE;
  }
  const std::string& input_file_path = options["input"];

  const std::string& output_folder_path = options["output"];

  if (!options.is_set("output") && !options.is_set("list"))
  {
    fmt::println(std::cerr, "Error: No output folder set");
    return EXIT_FAILURE;
  }

  const std::string& single_file_path = options["single"];
  const std::string& list_path = options["list"];
  std::string& specific_partition = const_cast<std::string&>(options["partition"]);

  if (options.is_set("output"))
    std::filesystem::create_directories(output_folder_path);

  if (gameonly)
    specific_partition = std::string("data");

  if (const std::unique_ptr<DiscIO::BlobReader> blob_reader =
          DiscIO::CreateBlobReader(input_file_path);
      !blob_reader)
  {
    fmt::println(std::cerr, "Error: Unable to open disc image");
    return EXIT_FAILURE;
  }

  const std::unique_ptr<DiscIO::Volume> disc_volume = DiscIO::CreateVolume(input_file_path);

  if (!disc_volume)
  {
    fmt::println("Error: Unable to open volume");
    return EXIT_FAILURE;
  }

  if (VolumeSupported(*disc_volume) == EXIT_FAILURE)
    return EXIT_FAILURE;

  if (options.is_set_by_user("list"))
  {
    ListVolume(*disc_volume, list_path, specific_partition);
    return EXIT_SUCCESS;
  }

  bool extracted_one = false;

  if (disc_volume->GetPartitions().empty())
  {
    if (options.is_set("partition"))
      fmt::println(
          std::cerr,
          "Warning: --partition has a value even though this image doesn't have any partitions.");

    if (!options.is_set("single"))
    {
      ExtractPartition(*disc_volume, DiscIO::PARTITION_NONE, output_folder_path, mute);
      extracted_one = true;
    }
    else if (auto file_info = GetFileInfo(*disc_volume, DiscIO::PARTITION_NONE, single_file_path);
             file_info != nullptr)
    {
      std::string file;
      file.append(output_folder_path).append("/");

      file.append(single_file_path);

      if (file_info->IsDirectory())
        ExtractDirectory(*disc_volume, DiscIO::PARTITION_NONE, single_file_path, output_folder_path,
                         mute);
      else
        ExtractFile(*disc_volume, DiscIO::PARTITION_NONE, single_file_path, file);

      extracted_one = true;
    }
  }
  else
  {
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

        if (!options.is_set("single"))
        {
          file.append(partition_name);
          ExtractPartition(*disc_volume, p, file, mute);
          continue;
        }

        if (auto file_info = GetFileInfo(*disc_volume, p, single_file_path); file_info != nullptr)
        {
          file.append(single_file_path);

          if (file_info->IsDirectory())
            ExtractDirectory(*disc_volume, p, single_file_path, output_folder_path, mute);
          else
            ExtractFile(*disc_volume, p, single_file_path, file);
          continue;
        }
        extracted_one = false;
      }
    }
  }

  if (!extracted_one)
  {
    if (options.is_set("single"))
      fmt::print(std::cerr, "Error: No File/Folder was extracted.");
    else
      fmt::print(std::cerr, "Error: No partitions were extracted..");
    if (options.is_set("partition"))
      fmt::println(std::cerr, " Maybe you misspelled your specified partition?");
    fmt::println(std::cerr, "\n");
    return EXIT_FAILURE;
  }

  fmt::println(std::cerr, "Finished Successfully!");
  return EXIT_SUCCESS;
}
}  // namespace DolphinTool
