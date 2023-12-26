// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinTool/HeaderCommand.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

#include <OptionParser.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <picojson.h>

#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDisc.h"

namespace DolphinTool
{
int HeaderCommand(const std::vector<std::string>& args)
{
  optparse::OptionParser parser;

  parser.usage("usage: header [options]...");

  parser.add_option("-i", "--input")
      .type("string")
      .action("store")
      .help("Path to disc image FILE.")
      .metavar("FILE");

  parser.add_option("-j", "--json")
      .action("store_true")
      .help("Optional. Print the information as JSON, then exit. Overrides other print options.");

  parser.add_option("-b", "--block_size")
      .action("store_true")
      .help("Optional. Print the block size of GCZ/WIA/RVZ formats, then exit.");

  parser.add_option("-c", "--compression")
      .action("store_true")
      .help("Optional. Print the compression method of GCZ/WIA/RVZ formats, then exit.");

  parser.add_option("-l", "--compression_level")
      .action("store_true")
      .help("Optional. Print the level of compression for WIA/RVZ formats, then exit.");

  const optparse::Values& options = parser.parse_args(args);

  // Validate options
  const std::string& input_file_path = options["input"];
  if (input_file_path.empty())
  {
    fmt::print(std::cerr, "Error: No input set\n");
    return EXIT_FAILURE;
  }

  const bool enable_json = options.is_set_by_user("json");
  const bool enable_block_size = options.is_set_by_user("block_size");
  const bool enable_compression_method = options.is_set_by_user("compression");
  const bool enable_compression_level = options.is_set_by_user("compression_level");

  // Open the blob reader
  const std::unique_ptr<DiscIO::BlobReader> blob_reader = DiscIO::CreateBlobReader(input_file_path);
  if (!blob_reader)
  {
    fmt::print(std::cerr, "Error: Unable to open disc image\n");
    return EXIT_FAILURE;
  }
  // Open the volume
  const std::unique_ptr<DiscIO::Volume> volume = DiscIO::CreateVolume(blob_reader->CopyReader());

  if (enable_json)
  {
    auto json = picojson::object();

    // File data
    if (const u64 block_size = blob_reader->GetBlockSize())
      json["block_size"] = picojson::value((double)block_size);

    const std::string compression_method = blob_reader->GetCompressionMethod();
    if (compression_method != "")
      json["compression_method"] = picojson::value(compression_method);

    if (const std::optional<int> compression_level = blob_reader->GetCompressionLevel())
      json["compression_level"] = picojson::value((double)compression_level.value());

    // Game data
    if (volume)
    {
      json["internal_name"] = picojson::value(volume->GetInternalName());

      if (const std::optional<u64> revision = volume->GetRevision())
        json["revision"] = picojson::value((double)revision.value());

      json["game_id"] = picojson::value(volume->GetGameID());

      if (const std::optional<u64> title_id = volume->GetTitleID())
        json["title_id"] = picojson::value((double)title_id.value());

      json["region"] = picojson::value(DiscIO::GetName(volume->GetRegion(), false));

      json["country"] = picojson::value(DiscIO::GetName(volume->GetCountry(), false));
    }

    // Print
    std::cout << picojson::value(json) << '\n';
  }
  else if (enable_block_size || enable_compression_method || enable_compression_level)
  {
    if (enable_block_size)
    {
      const auto block_size = blob_reader->GetBlockSize();
      if (block_size == 0)
        fmt::print(std::cout, "N/A\n");
      else
        fmt::print(std::cout, "{}\n", block_size);
    }
    if (enable_compression_method)
    {
      const auto compression_method = blob_reader->GetCompressionMethod();
      if (compression_method == "")
        fmt::print(std::cout, "N/A\n");
      else
        fmt::print(std::cout, "{}\n", compression_method);
    }
    if (enable_compression_level)
    {
      const auto compression_level_o = blob_reader->GetCompressionLevel();
      if (compression_level_o == std::nullopt)
        fmt::print(std::cout, "N/A\n");
      else
        fmt::print(std::cout, "{}\n", compression_level_o.value());
    }
  }
  else
  {
    // File data
    if (const u64 block_size = blob_reader->GetBlockSize())
      fmt::print(std::cout, "Block Size: {}\n", block_size);

    const std::string compression_method = blob_reader->GetCompressionMethod();
    if (compression_method != "")
      fmt::print(std::cout, "Compression Method: {}\n", compression_method);

    if (const std::optional<int> compression_level = blob_reader->GetCompressionLevel())
      fmt::print(std::cout, "Compression Level: {}\n", compression_level.value());

    // Game data
    if (volume)
    {
      fmt::print(std::cout, "Internal Name: {}\n", volume->GetInternalName());

      if (const std::optional<u64> revision = volume->GetRevision())
        fmt::print(std::cout, "Revision: {}\n", revision.value());

      fmt::print(std::cout, "Game ID: {}\n", volume->GetGameID());

      if (const std::optional<u64> title_id = volume->GetTitleID())
        fmt::print(std::cout, "Title ID: {}\n", title_id.value());

      fmt::print(std::cout, "Region: {}\n", DiscIO::GetName(volume->GetRegion(), false));

      fmt::print(std::cout, "Country: {}\n", DiscIO::GetName(volume->GetCountry(), false));
    }
  }

  return EXIT_SUCCESS;
}
}  // namespace DolphinTool
