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

  const bool enable_block_size = options.is_set_by_user("block_size");
  const bool enable_compression_method = options.is_set_by_user("compression");
  const bool enable_compression_level = options.is_set_by_user("compression_level");

  // Open the blob reader, plus get blob type
  const std::unique_ptr<DiscIO::BlobReader> blob_reader = DiscIO::CreateBlobReader(input_file_path);
  if (!blob_reader)
  {
    fmt::print(std::cerr, "Error: Unable to open disc image\n");
    return EXIT_FAILURE;
  }

  if (enable_block_size || enable_compression_method || enable_compression_level)
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
    const auto blob_type = blob_reader->GetBlobType();
    if (blob_type == DiscIO::BlobType::GCZ)
    {
      fmt::print(std::cout, "Block Size: {}\nCompression Method: {}\n", blob_reader->GetBlockSize(),
                 blob_reader->GetCompressionMethod());
    }
    if (blob_type == DiscIO::BlobType::WIA || blob_type == DiscIO::BlobType::RVZ)
    {
      fmt::print(std::cout, "Block Size: {}\nCompression Method: {}\nCompression Level: {}\n",
                 blob_reader->GetBlockSize(), blob_reader->GetCompressionMethod(),
                 blob_reader->GetCompressionLevel().value());
    }
  }

  return EXIT_SUCCESS;
}
}  // namespace DolphinTool
