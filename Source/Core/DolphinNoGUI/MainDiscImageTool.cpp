// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinNoGUI/Platform.h"

#include <OptionParser.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <memory>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#else
#include <Windows.h>
#endif

#include "Common/Version.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/Host.h"

#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDisc.h"
#include "DiscIO/VolumeVerifier.h"

// Begin stubs to satisfy Core dependencies
static std::unique_ptr<Platform> s_platform = Platform::CreateHeadlessPlatform();

std::vector<std::string> Host_GetPreferredLocales()
{
  return {};
}

void Host_NotifyMapLoaded()
{
}

void Host_RefreshDSPDebuggerWindow()
{
}

bool Host_UIBlocksControllerState()
{
  return false;
}

static Common::Event s_update_main_frame_event;
void Host_Message(HostMessageID id)
{
  if (id == HostMessageID::WMUserStop)
    s_platform->Stop();
}

void Host_UpdateTitle(const std::string& title)
{
  s_platform->SetTitle(title);
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateMainFrame()
{
  s_update_main_frame_event.Set();
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

bool Host_RendererHasFocus()
{
  return s_platform->IsWindowFocused();
}

bool Host_RendererHasFullFocus()
{
  // Mouse capturing isn't implemented
  return Host_RendererHasFocus();
}

bool Host_RendererIsFullscreen()
{
  return s_platform->IsWindowFullscreen();
}

void Host_YieldToUI()
{
}

void Host_TitleChanged()
{
}

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  return nullptr;
}
// End stubs to satisfy Core dependencies

static void signal_handler(int)
{
  s_platform->RequestShutdown();
}

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

static std::optional<DiscIO::VolumeVerifier::Result> VerifyVolume(
  std::shared_ptr<DiscIO::VolumeDisc> volume,
  bool enable_crc32,
  bool enable_md5,
  bool enable_sha1
) {
  if (!volume)
    return std::nullopt;

  DiscIO::VolumeVerifier verifier(*volume, false, 
    {enable_crc32, enable_md5, enable_sha1});

  auto future = std::async(std::launch::async, 
    [&verifier]() -> std::optional<DiscIO::VolumeVerifier::Result> {
      verifier.Start();
      while (verifier.GetBytesProcessed() != verifier.GetTotalBytes())
      {
        verifier.Process();
      }
      verifier.Finish();

      const DiscIO::VolumeVerifier::Result result = verifier.GetResult();

      return result;
    }
  );

  return future.get();
}

static void PrintProblemReport(const std::optional<DiscIO::VolumeVerifier::Result> result)
{
    std::cout << "Problems Found: " <<
      (result->problems.size() > 0 ? "Yes\n" : "No") << "\n";

  for (int i = 0; i < static_cast<int>(result->problems.size()); ++i)
  {
    const DiscIO::VolumeVerifier::Problem problem = result->problems[i];

    std::cout << "Severity: ";
    switch (problem.severity)
    {
    case DiscIO::VolumeVerifier::Severity::Low:
      std::cout << "Low\n";
      break;
    case DiscIO::VolumeVerifier::Severity::Medium:
      std::cout << "Medium\n";
      break;
    case DiscIO::VolumeVerifier::Severity::High:
      std::cout << "High\n";
      break;
    case DiscIO::VolumeVerifier::Severity::None:
      std::cout << "None\n";
      break;
    }

    std::cout << "Summary: " << problem.text << "\n\n";
  }
}

static int DoVerify(const std::string input_file_path, const optparse::Values& options)
{
  // Validate options
  bool simple_out = static_cast<bool>(options.get("verify_simple_out"));
  
  const std::string algorithm = static_cast<const char*>(options.get("verify_algorithm"));

  bool enable_all = algorithm == "all";
  bool enable_crc32 = enable_all || algorithm == "crc32";
  bool enable_md5 = enable_all || algorithm == "md5";
  bool enable_sha1 = enable_all || algorithm == "sha1";

  if (!enable_all && !enable_crc32 && !enable_md5 && !enable_sha1)
  {
    std::cerr << "Error: No algorithms selected for the operation" << "\n";
    return -1;
  }
  
  if (simple_out && enable_all)
  {
    std::cerr << "Error: Simple output incompatible with 'all' algorithm option" << "\n";
    return -1;
  }

  // Open the volume
  std::shared_ptr<DiscIO::VolumeDisc> volume = DiscIO::CreateDisc(input_file_path);

  if (!volume)
  {
    std::cerr << "Error: Unable to open disc image" << "\n";
    return -1;
  }

  // Verify the volume
  const std::optional<DiscIO::VolumeVerifier::Result> result =
    VerifyVolume(volume, enable_crc32, enable_md5, enable_sha1);

  if (!result)
  {
    std::cerr << "Error: Unable to verify volume" << "\n";
    return -1;
  }

  // If simple output mode is enabled, only print the hash
  if (simple_out)
  {
    if (enable_crc32 && !result->hashes.crc32.empty())
      std::cout << HashToHexString(result->hashes.crc32) << "\n";
    else if (enable_md5 && !result->hashes.md5.empty())
      std::cout << HashToHexString(result->hashes.md5) << "\n";
    else if (enable_sha1 && !result->hashes.sha1.empty())
      std::cout << HashToHexString(result->hashes.sha1) << "\n";
    else
    {
      std::cerr << "Error: No hash computed\n";
      return -1;
    }
  }
  else
  {
    if (enable_crc32 && !result->hashes.crc32.empty())
      std::cout << "CRC32: " << HashToHexString(result->hashes.crc32) << "\n";

    if (enable_md5 && !result->hashes.md5.empty())
      std::cout << "MD5: " << HashToHexString(result->hashes.md5) << "\n";

    if (enable_sha1 && !result->hashes.sha1.empty())
      std::cout << "SHA1: " << HashToHexString(result->hashes.sha1) << "\n";

    PrintProblemReport(result);
  }

  return 0;
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
#else
  // Shut down cleanly on SIGINT and SIGTERM
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
#endif

  auto parser = std::make_unique<optparse::OptionParser>();

  parser->usage("usage: %prog [options]... [FILE]...")
    .version(Common::scm_rev_str);

  parser->add_option("-m", "--mode")
    .type("string")
    .action("store")
    .help("Operating mode [%choices]")
    .choices({"verify"});

  parser->add_option("-i", "--input")
    .type("string")
    .action("store")
    .help("Path to disc image FILE")
    .metavar("FILE");

  parser->add_option("--verify_simple_out")
    .action("store_true")
    .help("Print the digest using the selected algorithm and exit. Incompatible with 'all' option.");

  parser->add_option("--verify_algorithm")
    .type("string")
    .action("store")
    .help("Digest algorithm to compute (default = sha1) [%choices]")
    .choices({"all", "crc32", "md5", "sha1"});

  parser->set_defaults("verify_algorithm", "sha1");

  const optparse::Values& options = parser->parse_args(argc, argv);

  // Ensure an input is set
  const std::string input_file_path = static_cast<const char*>(options.get("input"));
  if (input_file_path.empty())
  {
    std::cerr << "Error: No input set" << "\n";
    return -1;
  }

  // Validate the mode (and only ensure 'verify' for now)
  const std::string mode = static_cast<const char*>(options.get("mode"));
  if (mode != "verify")
  {
    std::cerr << "Error: Mode not set or not implemented, please check --help" << "\n";
    return -1;
  }

  return DoVerify(input_file_path, options);
}
