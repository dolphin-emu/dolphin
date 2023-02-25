// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinTool/HeaderCommand.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDisc.h"

#include <OptionParser.h>
#include <optional>

namespace DolphinTool
{
int HeaderCommand::Main(const std::vector<std::string>& args)
{
  auto parser = std::make_unique<optparse::OptionParser>();

  parser->usage("usage: header [options]...");

  parser->add_option("-i", "--input")
      .type("string")
      .action("store")
      .help("Path to disc image FILE.")
      .metavar("FILE");

  parser->add_option("-b", "--block_size")
      .action("store_true")
      .help("Optional. Print the block size of GCZ/WIA/RVZ formats, then exit.");

  parser->add_option("-c", "--compression")
      .action("store_true")
      .help("Optional. Print the compression method of GCZ/WIA/RVZ formats, then exit.");

  parser->add_option("-l", "--compression_level")
      .action("store_true")
      .help("Optional. Print the level of compression for WIA/RVZ formats, then exit.");

  const optparse::Values& options = parser->parse_args(args);

  // Validate options
  const std::string input_file_path = static_cast<const char*>(options.get("input"));
  if (input_file_path.empty())
  {
    std::cerr << "Error: No input set" << std::endl;
    return 1;
  }

  bool enable_block_size = options.is_set_by_user("block_size");
  bool enable_compression_method = options.is_set_by_user("compression");
  bool enable_compression_level = options.is_set_by_user("compression_level");

  // Open the blob reader, plus get blob type
  std::shared_ptr<DiscIO::BlobReader> blob_reader = DiscIO::CreateBlobReader(input_file_path);
  if (!blob_reader)
  {
    std::cerr << "Error: Unable to open disc image" << std::endl;
    return 1;
  }
  const DiscIO::BlobType blob_type = blob_reader->GetBlobType();

  if (enable_block_size || enable_compression_method || enable_compression_level)
  {
    if (enable_block_size)
    {
      const auto block_size = blob_reader->GetBlockSize();
      if (block_size == 0)
        std::cout << "N/A" << std::endl;
      else
        std::cout << block_size << std::endl;
    }
    if (enable_compression_method)
    {
      const auto compression_method = blob_reader->GetCompressionMethod();
      if (compression_method == "")
        std::cout << "N/A" << std::endl;
      else
        std::cout << compression_method << std::endl;
    }
    if (enable_compression_level)
    {
      const auto compression_level_o = blob_reader->GetCompressionLevel();
      if (compression_level_o == std::nullopt)
        std::cout << "N/A" << std::endl;
      else
        std::cout << compression_level_o.value() << std::endl;
    }
  }
  else
  {
    if (blob_type == DiscIO::BlobType::GCZ)
    {
      std::cout << "Block Size: " << blob_reader->GetBlockSize() << std::endl;
      std::cout << "Compression Method: " << blob_reader->GetCompressionMethod() << std::endl;
    }
    if (blob_type == DiscIO::BlobType::WIA || blob_type == DiscIO::BlobType::RVZ)
    {
      std::cout << "Block Size: " << blob_reader->GetBlockSize() << std::endl;
      std::cout << "Compression Method: " << blob_reader->GetCompressionMethod() << std::endl;
      std::cout << "Compression Level: " << blob_reader->GetCompressionLevel().value() << std::endl;
    }
  }

  return 0;
}

}  // namespace DolphinTool
