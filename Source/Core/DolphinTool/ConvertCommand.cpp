// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinTool/ConvertCommand.h"

#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <OptionParser.h>

#include "Common/CommonTypes.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/ScrubbedBlob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDisc.h"
#include "DiscIO/WIABlob.h"
#include "DolphinTool/Command.h"
#include "UICommon/UICommon.h"

namespace DolphinTool
{
int ConvertCommand::Main(const std::vector<std::string>& args)
{
  optparse::OptionParser parser;

  parser.usage("usage: convert [options]... [FILE]...");

  parser.add_option("-u", "--user")
      .action("store")
      .help("User folder path, required for temporary processing files. "
            "Will be automatically created if this option is not set.");

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

  parser.add_option("-f", "--format")
      .type("string")
      .action("store")
      .help("Container format to use. Default is RVZ. [%choices]")
      .choices({"iso", "gcz", "wia", "rvz"});

  parser.add_option("-s", "--scrub")
      .action("store_true")
      .help("Scrub junk data as part of conversion.");

  parser.add_option("-b", "--block_size")
      .type("int")
      .action("store")
      .help("Block size for GCZ/WIA/RVZ formats, as an integer. Suggested value for RVZ: 131072 "
            "(128 KiB)");

  parser.add_option("-c", "--compression")
      .type("string")
      .action("store")
      .help("Compression method to use when converting to WIA/RVZ. Suggested value for RVZ: zstd "
            "[%choices]")
      .choices({"none", "zstd", "bzip", "lzma", "lzma2"});

  parser.add_option("-l", "--compression_level")
      .type("int")
      .action("store")
      .help("Level of compression for the selected method. Ignored if 'none'. Suggested value for "
            "zstd: 5");

  const optparse::Values& options = parser.parse_args(args);

  // Initialize the dolphin user directory, required for temporary processing files
  // If this is not set, destructive file operations could occur due to path confusion
  std::string user_directory;
  if (options.is_set("user"))
    user_directory = static_cast<const char*>(options.get("user"));

  UICommon::SetUserDirectory(user_directory);
  UICommon::Init();

  // Validate options

  // --input
  const std::string input_file_path = static_cast<const char*>(options.get("input"));
  if (input_file_path.empty())
  {
    std::cerr << "Error: No input set" << std::endl;
    return 1;
  }

  // --output
  const std::string output_file_path = static_cast<const char*>(options.get("output"));
  if (output_file_path.empty())
  {
    std::cerr << "Error: No output set" << std::endl;
    return 1;
  }

  // --format
  const std::optional<DiscIO::BlobType> format_o =
      ParseFormatString(static_cast<const char*>(options.get("format")));
  if (!format_o.has_value())
  {
    std::cerr << "Error: No output format set" << std::endl;
    return 1;
  }
  const DiscIO::BlobType format = format_o.value();

  // Open the blob reader
  std::unique_ptr<DiscIO::BlobReader> blob_reader = DiscIO::CreateBlobReader(input_file_path);
  if (!blob_reader)
  {
    std::cerr << "Error: The input file could not be opened." << std::endl;
    return 1;
  }

  // --scrub
  const bool scrub = static_cast<bool>(options.get("scrub"));

  // Open the volume
  std::unique_ptr<DiscIO::Volume> volume = DiscIO::CreateDisc(input_file_path);
  if (!volume)
  {
    if (scrub)
    {
      std::cerr << "Error: Scrubbing is only supported for GC/Wii disc images." << std::endl;
      return 1;
    }

    std::cerr << "Warning: The input file is not a GC/Wii disc image. Continuing anyway."
              << std::endl;
  }

  if (scrub)
  {
    if (volume->IsDatelDisc())
    {
      std::cerr << "Error: Scrubbing a Datel disc is not supported." << std::endl;
      return 1;
    }

    blob_reader = DiscIO::ScrubbedBlob::Create(input_file_path);

    if (!blob_reader)
    {
      std::cerr << "Error: Unable to process disc image. Try again without --scrub." << std::endl;
      return 1;
    }
  }

  if (scrub && format == DiscIO::BlobType::RVZ)
  {
    std::cerr << "Warning: Scrubbing an RVZ container does not offer significant space advantages. "
                 "Continuing anyway."
              << std::endl;
  }

  if (scrub && format == DiscIO::BlobType::PLAIN)
  {
    std::cerr << "Warning: Scrubbing does not save space when converting to ISO unless using "
                 "external compression. Continuing anyway."
              << std::endl;
  }

  if (!scrub && format == DiscIO::BlobType::GCZ && volume &&
      volume->GetVolumeType() == DiscIO::Platform::WiiDisc && !volume->IsDatelDisc())
  {
    std::cerr << "Warning: Converting Wii disc images to GCZ without scrubbing may not offer space "
                 "advantages over ISO. Continuing anyway."
              << std::endl;
  }

  if (volume && volume->IsNKit())
  {
    std::cerr << "Warning: Converting an NKit file, output will still be NKit! Continuing anyway."
              << std::endl;
  }

  // --block_size
  std::optional<int> block_size_o;
  if (options.is_set("block_size"))
    block_size_o = static_cast<int>(options.get("block_size"));

  if (format == DiscIO::BlobType::GCZ || format == DiscIO::BlobType::WIA ||
      format == DiscIO::BlobType::RVZ)
  {
    if (!block_size_o.has_value())
    {
      std::cerr << "Error: Block size must be set for GCZ/RVZ/WIA" << std::endl;
      return 1;
    }

    if (!DiscIO::IsDiscImageBlockSizeValid(block_size_o.value(), format))
    {
      std::cerr << "Error: Block size is not valid for this format" << std::endl;
      return 1;
    }

    if (block_size_o.value() < DiscIO::PREFERRED_MIN_BLOCK_SIZE ||
        block_size_o.value() > DiscIO::PREFERRED_MAX_BLOCK_SIZE)
    {
      std::cerr << "Warning: Block size is not ideal for performance. Continuing anyway."
                << std::endl;
    }

    if (format == DiscIO::BlobType::GCZ && volume &&
        !DiscIO::IsGCZBlockSizeLegacyCompatible(block_size_o.value(), volume->GetDataSize()))
    {
      std::cerr << "Warning: For GCZs to be compatible with Dolphin < 5.0-11893, "
                   "the file size must be an integer multiple of the block size "
                   "and must not be an integer multiple of the block size multiplied by 32. "
                   "Continuing anyway."
                << std::endl;
    }
  }

  // --compress, --compress_level
  std::optional<DiscIO::WIARVZCompressionType> compression_o =
      ParseCompressionTypeString(static_cast<const char*>(options.get("compression")));

  std::optional<int> compression_level_o;
  if (options.is_set("compression_level"))
    compression_level_o = static_cast<int>(options.get("compression_level"));

  if (format == DiscIO::BlobType::WIA || format == DiscIO::BlobType::RVZ)
  {
    if (!compression_o.has_value())
    {
      std::cerr << "Error: Compression format must be set for WIA or RVZ" << std::endl;
      return 1;
    }

    if ((format == DiscIO::BlobType::WIA &&
         compression_o.value() == DiscIO::WIARVZCompressionType::Zstd) ||
        (format == DiscIO::BlobType::RVZ &&
         compression_o.value() == DiscIO::WIARVZCompressionType::Purge))
    {
      std::cerr << "Error: Compression type is not supported for the container format" << std::endl;
      return 1;
    }

    if (compression_o.value() == DiscIO::WIARVZCompressionType::None)
    {
      compression_level_o = 0;
    }
    else
    {
      if (!compression_level_o.has_value())
      {
        std::cerr << "Error: Compression level must be set when compression type is not 'none'"
                  << std::endl;
        return 1;
      }

      const std::pair<int, int> range =
          DiscIO::GetAllowedCompressionLevels(compression_o.value(), false);
      if (compression_level_o.value() < range.first || compression_level_o.value() > range.second)
      {
        std::cerr << "Error: Compression level not in acceptable range" << std::endl;
        return 1;
      }
    }
  }

  // Perform the conversion
  const auto NOOP_STATUS_CALLBACK = [](const std::string& text, float percent) { return true; };

  bool success = false;

  switch (format)
  {
  case DiscIO::BlobType::PLAIN:
  {
    success = DiscIO::ConvertToPlain(blob_reader.get(), input_file_path, output_file_path,
                                     NOOP_STATUS_CALLBACK);
    break;
  }

  case DiscIO::BlobType::GCZ:
  {
    u32 sub_type = std::numeric_limits<u32>::max();
    if (volume)
    {
      if (volume->GetVolumeType() == DiscIO::Platform::GameCubeDisc)
        sub_type = 0;
      else if (volume->GetVolumeType() == DiscIO::Platform::WiiDisc)
        sub_type = 1;
    }
    success = DiscIO::ConvertToGCZ(blob_reader.get(), input_file_path, output_file_path, sub_type,
                                   block_size_o.value(), NOOP_STATUS_CALLBACK);
    break;
  }

  case DiscIO::BlobType::WIA:
  case DiscIO::BlobType::RVZ:
  {
    success = DiscIO::ConvertToWIAOrRVZ(blob_reader.get(), input_file_path, output_file_path,
                                        format == DiscIO::BlobType::RVZ, compression_o.value(),
                                        compression_level_o.value(), block_size_o.value(),
                                        NOOP_STATUS_CALLBACK);
    break;
  }

  default:
  {
    ASSERT(false);
    break;
  }
  }

  if (!success)
  {
    std::cerr << "Error: Conversion failed" << std::endl;
    return 1;
  }

  return 0;
}

std::optional<DiscIO::WIARVZCompressionType>
ConvertCommand::ParseCompressionTypeString(const std::string& compression_str)
{
  if (compression_str == "none")
    return DiscIO::WIARVZCompressionType::None;
  else if (compression_str == "purge")
    return DiscIO::WIARVZCompressionType::Purge;
  else if (compression_str == "bzip2")
    return DiscIO::WIARVZCompressionType::Bzip2;
  else if (compression_str == "lzma")
    return DiscIO::WIARVZCompressionType::LZMA;
  else if (compression_str == "lzma2")
    return DiscIO::WIARVZCompressionType::LZMA2;
  else if (compression_str == "zstd")
    return DiscIO::WIARVZCompressionType::Zstd;
  else
    return std::nullopt;
}

std::optional<DiscIO::BlobType> ConvertCommand::ParseFormatString(const std::string& format_str)
{
  if (format_str == "iso")
    return DiscIO::BlobType::PLAIN;
  else if (format_str == "gcz")
    return DiscIO::BlobType::GCZ;
  else if (format_str == "wia")
    return DiscIO::BlobType::WIA;
  else if (format_str == "rvz")
    return DiscIO::BlobType::RVZ;
  else
    return std::nullopt;
}

}  // namespace DolphinTool
