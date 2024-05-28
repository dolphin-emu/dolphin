// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinTool/VerifyCommand.h"

#include <cstdlib>
#include <string>
#include <vector>

#include <OptionParser.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

#include "Common/StringUtil.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeVerifier.h"
#include "UICommon/UICommon.h"

namespace DolphinTool
{
static std::string HashToHexString(const std::vector<u8>& hash)
{
  std::stringstream ss;
  ss << std::hex;
  for (int i = 0; i < static_cast<int>(hash.size()); ++i)
  {
    ss << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
  }
  return ss.str();
}

static void PrintFullReport(const DiscIO::VolumeVerifier::Result& result)
{
  if (!result.hashes.crc32.empty())
    fmt::print(std::cout, "CRC32: {}\n", HashToHexString(result.hashes.crc32));
  else
    fmt::print(std::cout, "CRC32 not computed\n");

  if (!result.hashes.md5.empty())
    fmt::print(std::cout, "MD5: {}\n", HashToHexString(result.hashes.md5));
  else
    fmt::print(std::cout, "MD5 not computed\n");

  if (!result.hashes.sha1.empty())
    fmt::print(std::cout, "SHA1: {}\n", HashToHexString(result.hashes.sha1));
  else
    fmt::print(std::cout, "SHA1 not computed\n");

  fmt::print(std::cout, "Problems Found: {}\n", result.problems.empty() ? "No" : "Yes");

  for (const auto& problem : result.problems)
  {
    fmt::print(std::cout, "\nSeverity: ");
    switch (problem.severity)
    {
    case DiscIO::VolumeVerifier::Severity::Low:
      fmt::print(std::cout, "Low");
      break;
    case DiscIO::VolumeVerifier::Severity::Medium:
      fmt::print(std::cout, "Medium");
      break;
    case DiscIO::VolumeVerifier::Severity::High:
      fmt::print(std::cout, "High");
      break;
    case DiscIO::VolumeVerifier::Severity::None:
      fmt::print(std::cout, "None");
      break;
    default:
      ASSERT(false);
      break;
    }
    fmt::print(std::cout, "\nSummary: {}\n\n", problem.text);
  }
}

int VerifyCommand(const std::vector<std::string>& args)
{
  optparse::OptionParser parser;

  parser.usage("usage: verify [options]...");

  parser.add_option("-u", "--user")
      .type("string")
      .action("store")
      .help("User folder path, required for temporary processing files. "
            "Will be automatically created if this option is not set.")
      .set_default("");

  parser.add_option("-i", "--input")
      .type("string")
      .action("store")
      .help("Path to input file.")
      .metavar("FILE");

  parser.add_option("-a", "--algorithm")
      .type("string")
      .action("store")
      .help("Optional. Compute and print the digest using the selected algorithm, then exit. "
            "[%choices]")
      .choices({"crc32", "md5", "sha1"});

  const optparse::Values& options = parser.parse_args(args);

  // Initialize the dolphin user directory, required for temporary processing files
  // If this is not set, destructive file operations could occur due to path confusion
  UICommon::SetUserDirectory(options["user"]);
  UICommon::Init();

  // Validate options
  if (!options.is_set("input"))
  {
    fmt::print(std::cerr, "Error: No input set\n");
    return EXIT_FAILURE;
  }
  const std::string& input_file_path = options["input"];

  DiscIO::Hashes<bool> hashes_to_calculate{};
  const bool algorithm_is_set = options.is_set("algorithm");
  if (!algorithm_is_set)
  {
    hashes_to_calculate = DiscIO::VolumeVerifier::GetDefaultHashesToCalculate();
  }
  else
  {
    const std::string& algorithm = options["algorithm"];
    if (algorithm == "crc32")
      hashes_to_calculate.crc32 = true;
    else if (algorithm == "md5")
      hashes_to_calculate.md5 = true;
    else if (algorithm == "sha1")
      hashes_to_calculate.sha1 = true;
  }

  if (!hashes_to_calculate.crc32 && !hashes_to_calculate.md5 && !hashes_to_calculate.sha1)
  {
    // optparse should protect from this
    fmt::print(std::cerr, "Error: No algorithms selected for the operation\n");
    return EXIT_FAILURE;
  }

  // Open the volume
  const std::unique_ptr<DiscIO::Volume> volume = DiscIO::CreateVolume(input_file_path);
  if (!volume)
  {
    fmt::print(std::cerr, "Error: Unable to open input file\n");
    return EXIT_FAILURE;
  }

  // Verify the volume
  DiscIO::VolumeVerifier verifier(*volume, false, hashes_to_calculate);
  verifier.Start();
  while (verifier.GetBytesProcessed() != verifier.GetTotalBytes())
  {
    verifier.Process();
  }
  verifier.Finish();
  const DiscIO::VolumeVerifier::Result& result = verifier.GetResult();

  // Print the report
  if (!algorithm_is_set)
  {
    PrintFullReport(result);
  }
  else
  {
    if (hashes_to_calculate.crc32 && !result.hashes.crc32.empty())
      fmt::print(std::cout, "{}\n", HashToHexString(result.hashes.crc32));
    else if (hashes_to_calculate.md5 && !result.hashes.md5.empty())
      fmt::print(std::cout, "{}\n", HashToHexString(result.hashes.md5));
    else if (hashes_to_calculate.sha1 && !result.hashes.sha1.empty())
      fmt::print(std::cout, "{}\n", HashToHexString(result.hashes.sha1));
    else
    {
      fmt::print(std::cerr, "Error: No hash computed\n");
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
}  // namespace DolphinTool
