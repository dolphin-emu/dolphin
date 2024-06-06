// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinTool/ExtractCommand.h"

#include <filesystem>
#include <future>
#include <iostream>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <OptionParser.h>

#include "Common/FileUtil.h"

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
  if (!filesystem)
    return nullptr;

  std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path);
  return info;
}

static bool VolumeSupported(const DiscIO::Volume& disc_volume)
{
  switch (disc_volume.GetVolumeType())
  {
  case DiscIO::Platform::WiiWAD:
    fmt::println(std::cerr, "Error: Wii WADs are not supported.");
    return false;
  case DiscIO::Platform::ELFOrDOL:
    fmt::println(std::cerr,
                 "Error: *.elf or *.dol have no filesystem and are therefore not supported.");
    return false;
  case DiscIO::Platform::WiiDisc:
  case DiscIO::Platform::GameCubeDisc:
    return true;
  default:
    fmt::println(std::cerr, "Error: Unknown volume type.");
    return false;
  }
}

static void ExtractDirectory(const DiscIO::Volume& disc_volume, const DiscIO::Partition& partition,
                             const std::string& path, const std::string& out, bool quiet)
{
  const DiscIO::FileSystem* filesystem = disc_volume.GetFileSystem(partition);
  if (!filesystem)
    return;

  const std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path);
  u32 size = info->GetTotalChildren();
  u32 files = 0;
  ExportDirectory(
      disc_volume, partition, *info, true, "", out,
      [&files, &size, &quiet](const std::string& current) {
        files++;
        const float progress = static_cast<float>(files) / static_cast<float>(size) * 100;
        if (!quiet)
          fmt::println(std::cerr, "Extracting: {} | {}%", current, static_cast<int>(progress));
        return false;
      });
}

static bool ExtractSystemData(const DiscIO::Volume& disc_volume, const DiscIO::Partition& partition,
                              const std::string& out)
{
  return ExportSystemData(disc_volume, partition, out);
}

static void ExtractPartition(const DiscIO::Volume& disc_volume, const DiscIO::Partition& partition,
                             const std::string& out, bool quiet)
{
  ExtractDirectory(disc_volume, partition, "", out + "/files", quiet);
  ExtractSystemData(disc_volume, partition, out);
}

static bool ListPartition(const DiscIO::Volume& disc_volume, const DiscIO::Partition& partition,
                          const std::string& partition_name, const std::string& path,
                          std::string* result_text)
{
  const DiscIO::FileSystem* filesystem = disc_volume.GetFileSystem(partition);
  const std::unique_ptr<DiscIO::FileInfo> info = filesystem->FindFileInfo(path);

  if (!info)
  {
    if (!partition_name.empty())
    {
      fmt::println(std::cerr, "Warning: {} does not exist in this partition.", path);
    }
    return false;
  }

  for (auto it = info->begin(); it != info->end(); ++it)
  {
    const std::string file_name = fmt::format("{}\n", it->GetName());
    fmt::print(std::cout, "{}", file_name);
    result_text->append(file_name);
  }
  return true;
}

static bool ListVolume(const DiscIO::Volume& disc_volume, const std::string& path,
                       const std::string& specific_partition_name, bool quiet,
                       std::string* result_text)
{
  if (disc_volume.GetPartitions().empty())
  {
    return ListPartition(disc_volume, DiscIO::PARTITION_NONE, specific_partition_name, path,
                         result_text);
  }

  bool success = false;
  for (DiscIO::Partition& p : disc_volume.GetPartitions())
  {
    const std::optional<u32> partition_type = disc_volume.GetPartitionType(p);
    if (!partition_type)
    {
      fmt::println(std::cerr, "Error: Could not get partition type.");
      return false;
    }
    const std::string partition_name = DiscIO::NameForPartitionType(*partition_type, true);

    if (!specific_partition_name.empty() &&
        !Common::CaseInsensitiveEquals(partition_name, specific_partition_name))
    {
      continue;
    }

    const std::string partition_start =
        fmt::format("/// PARTITION: {} <{}> ///\n", partition_name, path);
    fmt::print(std::cout, "{}", partition_start);
    result_text->append(partition_start);

    success |= ListPartition(disc_volume, p, specific_partition_name, path, result_text);
  }

  return success;
}

static bool HandleExtractPartition(const std::string& output, const std::string& single_file_path,
                                   const std::string& partition_name, DiscIO::Volume& disc_volume,
                                   const DiscIO::Partition& partition, bool quiet, bool single)
{
  std::string file;
  file.append(output).append("/");
  file.append(partition_name).append("/");
  if (!single)
  {
    ExtractPartition(disc_volume, partition, file, quiet);
    return true;
  }

  if (auto file_info = GetFileInfo(disc_volume, partition, single_file_path); file_info != nullptr)
  {
    file.append("files/").append(single_file_path);
    File::CreateFullPath(file);
    if (file_info->IsDirectory())
    {
      file = PathToString(StringToPath(file).remove_filename());
      ExtractDirectory(disc_volume, partition, single_file_path, file, quiet);
    }
    else
    {
      ExtractFile(disc_volume, partition, single_file_path, file);
    }

    return true;
  }
  return false;
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
      .help("Path to the destination FOLDER.")
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
      .action("store_true")
      .help("List all files in volume/partition. Will print the directory/file specified with "
            "--single if defined.");
  parser.add_option("-q", "--quiet")
      .action("store_true")
      .help("Mute all messages except for errors.");
  parser.add_option("-g", "--gameonly")
      .action("store_true")
      .help("Only extracts the DATA partition.");

  const optparse::Values& options = parser.parse_args(args);

  const bool quiet = options.is_set("quiet");
  const bool gameonly = options.is_set("gameonly");

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
  std::string specific_partition = options["partition"];

  if (options.is_set("output") && !options.is_set("list"))
    File::CreateDirs(output_folder_path);

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
    fmt::println(std::cerr, "Error: Unable to open volume");
    return EXIT_FAILURE;
  }

  if (!VolumeSupported(*disc_volume))
    return EXIT_FAILURE;

  if (options.is_set("list"))
  {
    std::string list_path = options.is_set("single") ? single_file_path : "/";
    if (quiet && !options.is_set("output"))
    {
      fmt::println(std::cerr, "Error: --quiet is set but no output file provided. Please either "
                              "remove the --quiet flag or specify --output");
      return EXIT_FAILURE;
    }

    std::string text;
    if (!ListVolume(*disc_volume, list_path, specific_partition, quiet, &text))
    {
      fmt::println(std::cerr, "Error: Found nothing to list");
      return EXIT_FAILURE;
    }

    if (options.is_set("output"))
    {
      File::CreateFullPath(output_folder_path);
      std::ofstream output_file;
      output_file.open(output_folder_path);
      if (!output_file.is_open())
      {
        fmt::println(std::cerr, "Error: Unable to open output file");
        return EXIT_FAILURE;
      }
      output_file << text;
    }

    return EXIT_SUCCESS;
  }

  bool extracted_one = false;

  if (disc_volume->GetPartitions().empty())
  {
    if (options.is_set("partition"))
    {
      fmt::println(
          std::cerr,
          "Warning: --partition has a value even though this image doesn't have any partitions.");
    }

    extracted_one = HandleExtractPartition(output_folder_path, single_file_path, "", *disc_volume,
                                           DiscIO::PARTITION_NONE, quiet, options.is_set("single"));
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
        {
          continue;
        }

        extracted_one |=
            HandleExtractPartition(output_folder_path, single_file_path, partition_name,
                                   *disc_volume, p, quiet, options.is_set("single"));
      }
    }
  }

  if (!extracted_one)
  {
    if (options.is_set("single"))
      fmt::print(std::cerr, "Error: No file/folder was extracted.");
    else
      fmt::print(std::cerr, "Error: No partitions were extracted.");
    if (options.is_set("partition"))
      fmt::println(std::cerr, " Maybe you misspelled your specified partition?");
    fmt::println(std::cerr, "\n");
    return EXIT_FAILURE;
  }

  if (!quiet)
    fmt::println(std::cerr, "Finished Successfully!");
  return EXIT_SUCCESS;
}
}  // namespace DolphinTool
