// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinTool/ExtractCommand.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <OptionParser.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "Common/StringUtil.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWii.h"

namespace DolphinTool
{
static constexpr bool ProgressStub(const std::string&)
{
  return false;  // Do not cancel extraction.
}

static void ExtractPartition(const bool recursive, const bool overwrite, const bool sys,
                             const bool files, const DiscIO::Volume& volume,
                             const DiscIO::Partition& partition, const std::string& export_path)
{
  if (sys)
  {
    DiscIO::ExportSystemData(volume, partition, export_path);
  }
  if (files)
  {
    static const std::string root_path = "";
    const DiscIO::FileInfo& root_info = volume.GetFileSystem(partition)->GetRoot();
    DiscIO::ExportDirectory(volume, partition, root_info, recursive, true, overwrite, root_path,
                            export_path + "/files", ProgressStub);
  }
}

using ExtractErrand = std::tuple<DiscIO::Partition, std::unique_ptr<const DiscIO::FileInfo>,
                                 std::reference_wrapper<const std::string>>;

static void DoExtractErrands(const bool recursive, const bool overwrite,
                             const DiscIO::Volume& volume,
                             const std::vector<ExtractErrand>& extract_errands)
{
  for (const auto& [partition, file_info, export_path] : extract_errands)
  {
    if (file_info->IsDirectory())
    {
      // Here we (probably) lie about being the top-level directory so the function does
      // not put everything in a sub-folder named file_info->GetName() at the export_path.
      DiscIO::ExportDirectory(volume, partition, *file_info, recursive, true, overwrite,
                              file_info->GetPath(), export_path, ProgressStub);
    }
    else
    {
      // Logging like this might seem weird, but it's to match what DiscIO::ExportDirectory does.
      if (!overwrite && File::Exists(export_path))
      {
        NOTICE_LOG_FMT(DISCIO, "\"{}\" already exists", export_path.get());
      }
      else if (!File::CreateFullPath(export_path.get()) ||
               !DiscIO::ExportFile(volume, partition, file_info.get(), export_path))
      {
        ERROR_LOG_FMT(DISCIO, "Could not export \"{}\"", export_path.get());
      }
    }
  }
}

static bool PartitionFromType(const u32 partition_type, const DiscIO::VolumeWii& volume,
                              const std::vector<DiscIO::Partition>& partitions,
                              DiscIO::Partition& partition)
{
  const auto predicate = [&volume, partition_type](const DiscIO::Partition& partition) -> bool {
    return volume.GetPartitionType(partition) == partition_type;
  };
  if (const auto count = std::count_if(partitions.begin(), partitions.end(), predicate))
  {
    if (count > 1)
    {
      fmt::print(std::cerr,
                 "Warning: Multiple partitions of type {0:} were found. This is abnormal and "
                 "suggests that the Wii disc volume has been tampered with. Only the first "
                 "partition of type {0:} will be recognized.\n",
                 partition_type);
    }
    partition = *std::find_if(partitions.begin(), partitions.end(), predicate);
    return true;
  }
  fmt::print(std::cerr, "Error: Volume does not contain a partition of type {}.\n", partition_type);
  return false;
}

static bool PartitionFromIndex(const std::string_view index_input,
                               const std::vector<DiscIO::Partition>& partitions,
                               DiscIO::Partition& partition)
{
  if (u32 partition_idx; Common::FromChars(index_input, partition_idx).ec == std::errc{})
  {
    if (partition_idx < partitions.size())
    {
      partition = partitions[partition_idx];
      return true;
    }
    fmt::print(std::cerr, "Error: Partition index {} is out of range. Volume has {} partitions.\n",
               partition_idx, partitions.size());
    return false;
  }
  fmt::print(std::cerr, "Error: Partition index input \"{}\" could not be parsed.\n", index_input);
  return false;
}

static bool ParsePartitionInput(const std::string_view input, const DiscIO::VolumeWii& volume,
                                const std::vector<DiscIO::Partition>& partitions,
                                DiscIO::Partition& partition)
{
  // Access by partition type
  if (u32 partition_type; Common::FromChars(input, partition_type).ec == std::error_code{})
    return PartitionFromType(partition_type, volume, partitions, partition);

  // Access by partition index (not recommended, but may be necessary for niche situations).
  if (input.front() == '#')
    return PartitionFromIndex(input.substr(1), partitions, partition);

  // Access by partition type name
  if (Common::CaseInsensitiveEquals(input, "data"))
    return PartitionFromType(DiscIO::PARTITION_DATA, volume, partitions, partition);
  if (Common::CaseInsensitiveEquals(input, "update"))
    return PartitionFromType(DiscIO::PARTITION_UPDATE, volume, partitions, partition);
  if (Common::CaseInsensitiveEquals(input, "channel"))
    return PartitionFromType(DiscIO::PARTITION_CHANNEL, volume, partitions, partition);
  if (Common::CaseInsensitiveEquals(input, "install"))
    return PartitionFromType(DiscIO::PARTITION_INSTALL, volume, partitions, partition);

  fmt::print(std::cerr, "Error: Partition input \"{}\" could not be parsed.\n", input);
  return false;
}

static bool ExtractGCNDisc(const DiscIO::VolumeGC& volume, const optparse::Values& options,
                           const std::vector<std::string>& arguments)
{
  if (options.is_set("partition"))
  {
    fmt::print(std::cerr, "Error: GCN disc volumes do not have partitions.\n");
    return false;
  }

  const std::size_t arguments_size = arguments.size();
  if (arguments_size % 2 != 0)
  {
    fmt::print(std::cerr, "Error: Expected a multiple of 2 arguments, got {}.\n", arguments_size);
    return false;
  }
  std::vector<ExtractErrand> extract_errands;
  extract_errands.reserve(arguments_size / 2);

  const DiscIO::FileSystem* filesystem = volume.GetFileSystem(DiscIO::PARTITION_NONE);
  for (auto iter = arguments.begin(), iter_end = arguments.end(); iter != iter_end;)
  {
    std::unique_ptr<const DiscIO::FileInfo> file_info = filesystem->FindFileInfo(*iter);
    if (file_info == nullptr)
    {
      fmt::print(std::cerr, "Error: Could not find file info for FST path \"{}\".\n", *iter);
      return false;
    }
    ++iter;

    extract_errands.emplace_back(DiscIO::PARTITION_NONE, std::move(file_info), *iter);
    ++iter;
  }

  const bool recursive_is_set = options.is_set("recursive");
  const bool overwrite_is_set = options.is_set("overwrite");
  const bool sys_is_set = options.is_set("sys");
  const bool files_is_set = options.is_set("files");

  if (sys_is_set || files_is_set)
  {
    if (!options.is_set("output"))
    {
      fmt::print("Error: No output path set.\n");
      return false;
    }
    const std::string& export_path = options["output"];

    ExtractPartition(recursive_is_set, overwrite_is_set, sys_is_set, files_is_set, volume,
                     DiscIO::PARTITION_NONE, export_path);
  }

  DoExtractErrands(recursive_is_set, overwrite_is_set, volume, extract_errands);
  return true;
}

static bool ExtractWiiDisc(const DiscIO::VolumeWii& volume, const optparse::Values& options,
                           const std::vector<std::string>& arguments)
{
  const std::size_t arguments_size = arguments.size();
  if (arguments_size % 3 != 0)
  {
    fmt::print(std::cerr, "Error: Expected a multiple of 3 arguments, got {}.\n", arguments_size);
    return false;
  }
  std::vector<ExtractErrand> extract_errands;
  extract_errands.reserve(arguments_size / 3);

  const std::vector<DiscIO::Partition> partitions = volume.GetPartitions();
  for (auto iter = arguments.begin(), iter_end = arguments.end(); iter != iter_end;)
  {
    DiscIO::Partition partition;
    if (!ParsePartitionInput(*iter, volume, partitions, partition))
      return false;
    ++iter;

    std::unique_ptr<const DiscIO::FileInfo> file_info =
        volume.GetFileSystem(partition)->FindFileInfo(*iter);
    if (file_info == nullptr)
    {
      fmt::print(std::cerr, "Error: Could not find file info for FST path \"{}\".\n", *iter);
      return false;
    }
    ++iter;

    extract_errands.emplace_back(partition, std::move(file_info), *iter);
    ++iter;
  }

  const bool recursive_is_set = options.is_set("recursive");
  const bool overwrite_is_set = options.is_set("overwrite");
  const bool sys_is_set = options.is_set("sys");
  const bool files_is_set = options.is_set("files");

  if (sys_is_set || files_is_set)
  {
    if (!options.is_set("output"))
    {
      fmt::print("Error: No output path set.\n");
      return false;
    }
    const std::string& export_path = options["output"];

    if (options.is_set("partition"))
    {
      const std::string& partition_input = options["partition"];
      DiscIO::Partition partition;
      if (!ParsePartitionInput(partition_input, volume, partitions, partition))
        return false;
      ExtractPartition(recursive_is_set, overwrite_is_set, sys_is_set, files_is_set, volume,
                       partition, export_path);
    }
    else
    {
      for (const DiscIO::Partition& partition : partitions)
      {
        const u32 partition_type = *volume.GetPartitionType(partition);
        const std::string partition_name = DiscIO::NameForPartitionType(partition_type, true);
        ExtractPartition(recursive_is_set, overwrite_is_set, sys_is_set, files_is_set, volume,
                         partition, fmt::format("{}/{}", export_path, partition_name));
      }
    }
  }

  DoExtractErrands(recursive_is_set, overwrite_is_set, volume, extract_errands);
  return true;
}

int ExtractCommand(const std::vector<std::string>& args)
{
  optparse::OptionParser parser;

  parser.usage("Usage: extract [options]...\n"
               "               <fst-path-1> <export-path-1>\n"
               "               <fst-path-2> <export-path-2>\n"
               "               ...\n"
               "               <fst-path-N> <export-path-N>\n"
               "\n"
               "Usage: extract [options]...\n"
               "               <partition-1> <fst-path-1> <export-path-1>\n"
               "               <partition-2> <fst-path-2> <export-path-2>\n"
               "               ...\n"
               "               <partition-N> <fst-path-N> <export-path-N>\n"
               "\n"
               "Partition inputs can be a:\n"
               "  type  (0, 1, 2, 3, ...)\n"
               "  name  (data, update, channel, install)\n"
               "  index (#0, #1, #2, #3, ...)");

  parser.add_option("-i", "--input")
      .type("string")
      .action("store")
      .help("Path to volume to extract from.")
      .metavar("FILE");

  parser.add_option("-p", "--partition")
      .type("string")
      .action("store")
      .help("Partition of the Wii disc volume to extract from for the --sys and --files options. "
            "Omit this option to extract from all partitions.")
      .metavar("PARTITION");

  parser.add_option("-o", "--output")
      .type("string")
      .action("store")
      .help("Path to output to for the --sys and --files options.")
      .metavar("DIRECTORY");

  parser.add_option("-r", "--recursive").nargs(0).help("Extract from directories recursively.");

  parser.add_option("-w", "--overwrite").nargs(0).help("Overwrite files if they already exist.");

  parser.add_option("-s", "--sys").nargs(0).help("Extract system data.");

  parser.add_option("-f", "--files").nargs(0).help("Extract all files.");

  const optparse::Values& options = parser.parse_args(args);
  if (!options.is_set("input"))
  {
    fmt::print(std::cerr, "Error: No input set.\n");
    return EXIT_FAILURE;
  }
  const std::string& input_path = options["input"];

  const std::unique_ptr<const DiscIO::Volume> volume = DiscIO::CreateVolume(input_path);
  if (!volume)
  {
    fmt::print(std::cerr, "Error: Unable to open volume \"{}\".\n", input_path);
    return EXIT_FAILURE;
  }

  // Existing code in DiscExtractor.cpp, which this command relies upon, uses the log for warnings.
  Common::Log::LogManager::Init();
  if (Common::Log::LogManager* instance = Common::Log::LogManager::GetInstance())
  {
    instance->SetLogLevel(Common::Log::LogLevel::LINFO);
    instance->SetEnable(Common::Log::LogType::DISCIO, true);
    instance->SetEnable(Common::Log::LogType::COMMON, true);
  }

  const std::vector<std::string>& arguments = parser.args();
  switch (volume->GetVolumeType())
  {
  case DiscIO::Platform::GameCubeDisc:
    if (ExtractGCNDisc(static_cast<const DiscIO::VolumeGC&>(*volume), options, arguments))
      return EXIT_SUCCESS;
    return EXIT_FAILURE;
  case DiscIO::Platform::WiiDisc:
    if (ExtractWiiDisc(static_cast<const DiscIO::VolumeWii&>(*volume), options, arguments))
      return EXIT_SUCCESS;
    return EXIT_FAILURE;
  case DiscIO::Platform::WiiWAD:
    fmt::print(std::cerr, "Error: Wii WADs are not supported yet.\n");
    return EXIT_FAILURE;
  case DiscIO::Platform::ELFOrDOL:
    fmt::print(std::cerr, "Error: *.elf or *.dol have no filesystem.\n");
    return EXIT_FAILURE;
  default:
    fmt::print(std::cerr, "Error: Unsupported volume type.\n");
    return EXIT_FAILURE;
  }
}
}  // namespace DolphinTool
