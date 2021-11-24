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
  {
    return std::nullopt;
  }

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

  parser->add_option("-f", "--file")
    .type("string")
    .action("store")
    .dest("file_path")
    .help("read disc image from FILE")
    .metavar("FILE");

  parser->add_option("-a", "--algorithm")
    .type("string")
    .action("store")
    .dest("algorithm")
    .help("Digest algorithms to compute (default = sha1) [ [%choices]")
    .choices({"all", "crc32", "md5", "sha1"});

  parser->set_defaults("algorithm", "sha1");

  optparse::Values& options = parser->parse_args(argc, argv);

  // Ensure a path is set
  const std::string file_path = static_cast<const char*>(options.get("file_path"));
  if (file_path.empty())
  {
    std::cout << "Error: No file path set" << "\n";
    return -1;
  }

  // Determine algorithm selection
  const std::string digest_algorithm = static_cast<const char*>(options.get("algorithm"));

  bool enable_all = digest_algorithm == "all";
  bool enable_crc32 = enable_all || digest_algorithm == "crc32";
  bool enable_md5 = enable_all || digest_algorithm == "md5";
  bool enable_sha1 = enable_all || digest_algorithm == "sha1";

  if (!enable_all && !enable_crc32 && !enable_md5 && !enable_sha1)
  {
    // optparse and the booleans above should protect from this scenario
    std::cout << "Error: No algorithms selected for the operation" << "\n";
    return -1;
  }

  // Open the volume
  std::shared_ptr<DiscIO::VolumeDisc> volume = DiscIO::CreateDisc(file_path);

  if (!volume)
  {
    std::cout << "Error: Unable to open disc image" << "\n";
    return -1;
  }

  // Verify the volume
  std::optional<DiscIO::VolumeVerifier::Result> result =
    VerifyVolume(volume, enable_crc32, enable_md5, enable_sha1);

  if (!result)
  {
    std::cout << "Error: Unable to verify volume" << "\n";
    return -1;
  }

  // Print the results
  if (enable_crc32)
    std::cout << "CRC32: " << HashToHexString(result->hashes.crc32) << "\n";

  if (enable_md5)
    std::cout << "MD5: " << HashToHexString(result->hashes.md5) << "\n";

  if (enable_sha1)
    std::cout << "SHA1: " << HashToHexString(result->hashes.sha1) << "\n";

  std::cout << "Problems Detected: " <<
    (result->problems.size() > 0 ? "Yes" : "No") << "\n";

  return 0;
}
