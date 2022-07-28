// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinTool/VerifyCommand.h"
#include "UICommon/UICommon.h"

#include <OptionParser.h>

namespace DolphinTool
{
int VerifyCommand::Main(const std::vector<std::string>& args)
{
  auto parser = std::make_unique<optparse::OptionParser>();

  parser->usage("usage: verify [options]...");

  parser->add_option("-u", "--user")
      .action("store")
      .help("User folder path, required for temporary processing files. "
            "Will be automatically created if this option is not set.");

  parser->add_option("-i", "--input")
      .type("string")
      .action("store")
      .help("Path to disc image FILE.")
      .metavar("FILE");

  parser->add_option("-a", "--algorithm")
      .type("string")
      .action("store")
      .help("Optional. Compute and print the digest using the selected algorithm, then exit. "
            "[%choices]")
      .choices({"crc32", "md5", "sha1"});

  const optparse::Values& options = parser->parse_args(args);

  // Initialize the dolphin user directory, required for temporary processing files
  // If this is not set, destructive file operations could occur due to path confusion
  std::string user_directory;
  if (options.is_set("user"))
    user_directory = static_cast<const char*>(options.get("user"));

  UICommon::SetUserDirectory(user_directory);
  UICommon::Init();

  // Validate options
  const std::string input_file_path = static_cast<const char*>(options.get("input"));
  if (input_file_path.empty())
  {
    std::cerr << "Error: No input set" << std::endl;
    return 1;
  }

  std::optional<std::string> algorithm;
  if (options.is_set("algorithm"))
  {
    algorithm = static_cast<const char*>(options.get("algorithm"));
  }

  DiscIO::Hashes<bool> hashes_to_calculate{};
  if (algorithm == std::nullopt)
  {
    hashes_to_calculate = DiscIO::VolumeVerifier::GetDefaultHashesToCalculate();
  }
  else
  {
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
    std::cerr << "Error: No algorithms selected for the operation" << std::endl;
    return 1;
  }

  // Open the volume
  std::shared_ptr<DiscIO::VolumeDisc> volume = DiscIO::CreateDisc(input_file_path);
  if (!volume)
  {
    std::cerr << "Error: Unable to open disc image" << std::endl;
    return 1;
  }

  // Verify the volume
  const std::optional<DiscIO::VolumeVerifier::Result> result =
      VerifyVolume(volume, hashes_to_calculate);
  if (!result)
  {
    std::cerr << "Error: Unable to verify volume" << std::endl;
    return 1;
  }

  if (algorithm == std::nullopt)
  {
    PrintFullReport(result);
  }
  else
  {
    if (hashes_to_calculate.crc32 && !result->hashes.crc32.empty())
      std::cout << HashToHexString(result->hashes.crc32) << std::endl;
    else if (hashes_to_calculate.md5 && !result->hashes.md5.empty())
      std::cout << HashToHexString(result->hashes.md5) << std::endl;
    else if (hashes_to_calculate.sha1 && !result->hashes.sha1.empty())
      std::cout << HashToHexString(result->hashes.sha1) << std::endl;
    else
    {
      std::cerr << "Error: No hash computed" << std::endl;
      return 1;
    }
  }

  return 0;
}

void VerifyCommand::PrintFullReport(const std::optional<DiscIO::VolumeVerifier::Result> result)
{
  if (!result->hashes.crc32.empty())
    std::cout << "CRC32: " << HashToHexString(result->hashes.crc32) << std::endl;
  else
    std::cout << "CRC32 not computed" << std::endl;

  if (!result->hashes.md5.empty())
    std::cout << "MD5: " << HashToHexString(result->hashes.md5) << std::endl;
  else
    std::cout << "MD5 not computed" << std::endl;

  if (!result->hashes.sha1.empty())
    std::cout << "SHA1: " << HashToHexString(result->hashes.sha1) << std::endl;
  else
    std::cout << "SHA1 not computed" << std::endl;

  std::cout << "Problems Found: " << (result->problems.size() > 0 ? "Yes" : "No") << std::endl;

  for (int i = 0; i < static_cast<int>(result->problems.size()); ++i)
  {
    const DiscIO::VolumeVerifier::Problem problem = result->problems[i];

    std::cout << std::endl << "Severity: ";
    switch (problem.severity)
    {
    case DiscIO::VolumeVerifier::Severity::Low:
      std::cout << "Low";
      break;
    case DiscIO::VolumeVerifier::Severity::Medium:
      std::cout << "Medium";
      break;
    case DiscIO::VolumeVerifier::Severity::High:
      std::cout << "High";
      break;
    case DiscIO::VolumeVerifier::Severity::None:
      std::cout << "None";
      break;
    default:
      ASSERT(false);
      break;
    }
    std::cout << std::endl;

    std::cout << "Summary: " << problem.text << std::endl << std::endl;
  }
}

std::optional<DiscIO::VolumeVerifier::Result>
VerifyCommand::VerifyVolume(std::shared_ptr<DiscIO::VolumeDisc> volume,
                            const DiscIO::Hashes<bool>& hashes_to_calculate)
{
  if (!volume)
    return std::nullopt;

  DiscIO::VolumeVerifier verifier(*volume, false, hashes_to_calculate);

  verifier.Start();
  while (verifier.GetBytesProcessed() != verifier.GetTotalBytes())
  {
    verifier.Process();
  }
  verifier.Finish();

  const DiscIO::VolumeVerifier::Result result = verifier.GetResult();

  return result;
}

std::string VerifyCommand::HashToHexString(const std::vector<u8>& hash)
{
  std::stringstream ss;
  ss << std::hex;
  for (int i = 0; i < static_cast<int>(hash.size()); ++i)
  {
    ss << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
  }
  return ss.str();
}

}  // namespace DolphinTool
