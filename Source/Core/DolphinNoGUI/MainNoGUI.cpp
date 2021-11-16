// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinNoGUI/Platform.h"

#include <OptionParser.h>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#else
#include <Windows.h>
#endif

#include "Common/StringUtil.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/Host.h"

#include "UICommon/CommandLineParse.h"
#ifdef USE_DISCORD_PRESENCE
#include "UICommon/DiscordPresence.h"
#endif
#include "UICommon/UICommon.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoBackendBase.h"

#include <DiscIO/DiscConverter.cpp>

static std::unique_ptr<Platform> s_platform;

static void signal_handler(int)
{
  const char message[] = "A signal was received. A second signal will force Dolphin to stop.\n";
#ifdef _WIN32
  puts(message);
#else
  if (write(STDERR_FILENO, message, sizeof(message)) < 0)
  {
  }
#endif

  s_platform->RequestShutdown();
}
namespace fs = std::filesystem;
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
#ifdef USE_DISCORD_PRESENCE
  Discord::UpdateDiscordPresence();
#endif
}

std::unique_ptr<GBAHostInterface> Host_CreateGBAHost(std::weak_ptr<HW::GBA::Core> core)
{
  return nullptr;
}

static std::unique_ptr<Platform> GetPlatform(const optparse::Values& options)
{
  std::string platform_name = static_cast<const char*>(options.get("platform"));

#if HAVE_X11
  if (platform_name == "x11" || platform_name.empty())
    return Platform::CreateX11Platform();
#endif

#ifdef __linux__
  if (platform_name == "fbdev" || platform_name.empty())
    return Platform::CreateFBDevPlatform();
#endif

#ifdef _WIN32
  if (platform_name == "win32" || platform_name.empty())
    return Platform::CreateWin32Platform();
#endif

  if (platform_name == "headless" || platform_name.empty())
    return Platform::CreateHeadlessPlatform();

  return nullptr;
}

int main(int argc, char* argv[])
{
  auto parser = CommandLineParse::CreateParser(CommandLineParse::ParserOptions::OmitGUIOptions);
  parser->add_option("-f", "--format")
      .action("store")
      .help("Format to convert the file to [%choices]")
      .type("string")
      .set_default("RVZ")
      .choices({"rvz", "wia", "gcz", "iso"});

  parser->add_option("-c", "--compression")
      .action("store")
      .metavar("<string>")
      .type("string")
      .set_default("lzma2")
      .choices({"lzma2", "zstd", "lzma", "bzip2"})
      .help("Choose the compression type [%choices]");

  parser->add_option("-l", "--compression_level")
      .action("store")
      .metavar("<int>")
      .type("int")
      .set_default(9)
      .help("Choose the compression level, for RVZ max is 9 (default: %default) [1-10]");

  parser->add_option("-b", "--block_size")
      .action("store")
      .metavar("<int>")
      .type("int")
      .set_default(128)
      .choices({"32", "64", "128", "256", "512", "1024", "2048"})
      .help("Choose the block size in kb for RVZ compression [%choices]");

  parser->add_option("-j", "--remove_junk")
      .action("store_true")
      .help("Removes junk data (irreversable) when converting.");

  parser->add_option("-x", "--delete_original")
      .action("store_true")
      .help("Removes original file when sucessfully converted. This will cause data loss, use at "
            "your own risk!");

  parser->add_option("-i", "--input")
      .action("store")
      .metavar("<file>")
      .type("string")
      .help("The path to a single file or a directory containing files to convert.");

  parser->add_option("-o", "--output")
      .action("store")
      .metavar("<file>")
      .type("string")
      .help("(Optional) The path to save converted file(s). If --input is a folder, --output must be one too.");

  parser->add_option("-p", "--platform")
      .action("store")
      .help("Window platform to use [%choices]")
      .choices({
        "headless"
#ifdef __linux__
            ,
            "fbdev"
#endif
#if HAVE_X11
            ,
            "x11"
#endif
#ifdef _WIN32
            ,
            "win32"
#endif
      });

  optparse::Values& options = CommandLineParse::ParseArguments(parser.get(), argc, argv);
  std::vector<std::string> args = parser->args();

  std::optional<std::string> save_state_path;
  if (options.is_set("save_state"))
  {
    save_state_path = static_cast<const char*>(options.get("save_state"));
  }

  std::unique_ptr<BootParameters> boot;
  bool game_specified = false;
  if (options.is_set("exec"))
  {
    const std::list<std::string> paths_list = options.all("exec");
    const std::vector<std::string> paths{std::make_move_iterator(std::begin(paths_list)),
                                         std::make_move_iterator(std::end(paths_list))};
    boot = BootParameters::GenerateFromFile(paths, save_state_path);
    game_specified = true;
  }
  else if (options.is_set("nand_title"))
  {
    const std::string hex_string = static_cast<const char*>(options.get("nand_title"));
    if (hex_string.length() != 16)
    {
      fprintf(stderr, "Invalid title ID\n");
      parser->print_help();
      return 1;
    }
    const u64 title_id = std::stoull(hex_string, nullptr, 16);
    boot = std::make_unique<BootParameters>(BootParameters::NANDTitle{title_id});
  }
  else if (args.size())
  {
    boot = BootParameters::GenerateFromFile(args.front(), save_state_path);
    args.erase(args.begin());
    game_specified = true;
  }
  else if (options.is_set("input"))
  {
    bool delete_original = static_cast<bool>(options.get("delete_original"));

    int compression_level = static_cast<int>(options.get("compression_level"));
    std::string format = static_cast<const char*>(options.get("format"));
    std::string compression = static_cast<const char*>(options.get("compression"));
    DiscIO::WIARVZCompressionType compression_type = DiscIO::WIARVZCompressionType::LZMA2;

    bool remove_junk = static_cast<bool>(options.get("remove_junk"));

    int block_size = static_cast<int>(options.get("block_size"));
    if (block_size == 32)
    {
      block_size = 0x8000;
    }
    else if (block_size == 64)
    {
      block_size = 0x10000;
    }
    else if (block_size == 128)
    {
      block_size = 0x20000;
    }
    else if (block_size == 256)
    {
      block_size = 0x40000;
    }
    else if (block_size == 512)
    {
      block_size = 0x80000;
    }
    else if (block_size == 2048)
    {
      block_size = 0x100000;
    }
    else if (block_size == 2048)
    {
      block_size = 0x200000;
    }

    if (compression == "lzma" || compression == "LZMA")
    {
      compression_type = DiscIO::WIARVZCompressionType::LZMA;
    }
    if (compression == "lzma2" || compression == "LZMA2")
    {
      compression_type = DiscIO::WIARVZCompressionType::LZMA2;
    }
    if (compression == "zstd" || compression == "ZSTD")
    {
      compression_type = DiscIO::WIARVZCompressionType::Zstd;
    }
    if (compression == "bzip2" || compression == "BZIP2")
    {
      compression_type = DiscIO::WIARVZCompressionType::Bzip2;
    }

    cout << "format: " + format << endl;

    DiscIO::BlobType blob_type = DiscIO::BlobType::RVZ;

    if (format == "RVZ" || format == "rvz")
    {
      blob_type = DiscIO::BlobType::RVZ;
      format = "rvz";

      if (compression_level > 9)
      {
        fprintf(stderr, "Compression level invalid for RVZ Compression.\n");
        return 1;
      }
    }
    else if (format == "ISO" || format == "iso")
    {
      blob_type = DiscIO::BlobType::PLAIN;
      format = "iso";
    }
    else if (format == "GCZ" || format == "gcz")
    {
      blob_type = DiscIO::BlobType::GCZ;
      format = "gcz";
    }

    std::string input = static_cast<const char*>(options.get("input"));
    std::string output;

    if (options.is_set("output"))
      output = static_cast<const char*>(options.get("output"));
    else
    {
      output = static_cast<const char*>(options.get("input"));
      output = output + "." + format;
    }

    std::error_code ec;
    if (fs::is_directory(input, ec))
    {
      // Process as directory batch job.
      std::string ext(".iso");
      for (auto& p : fs::recursive_directory_iterator(input))
      {
        if (p.path().extension() == ext)
        {
          fs::path filename = p.path().stem().string() + "." + format;
          fs::path dst = output / filename;
          // cout << output << endl;
          // cout << dst.string() << endl;

          DiscIO::ConvertDisc(p.path().string(), dst.string(), blob_type, compression_type,
                              compression_level, block_size, remove_junk, delete_original);
        }
      }
    }
    if (ec)  // Optional handling of possible errors.
    {
      std::cerr << "Error in is_directory: " << ec.message();
    }
    if (fs::is_regular_file(input, ec))
    {
      cout << output << endl;
      DiscIO::ConvertDisc(input, output, blob_type, compression_type, compression_level, block_size,
                          remove_junk, delete_original);

      // Process a regular file.
    }
    if (ec)  // Optional handling of possible errors. Usage of the same ec object works since fs
             // functions are calling ec.clear() if no errors occur.
    {
      std::cerr << "Error in is_regular_file: " << ec.message();
    }

    return 0;
  }
  else
  {
    parser->print_help();
    return 0;
  }

  std::string user_directory;
  if (options.is_set("user"))
    user_directory = static_cast<const char*>(options.get("user"));

  UICommon::SetUserDirectory(user_directory);
  UICommon::Init();

  s_platform = GetPlatform(options);
  if (!s_platform || !s_platform->Init())
  {
    fprintf(stderr, "No platform found, or failed to initialize.\n");
    return 1;
  }

  if (save_state_path && !game_specified)
  {
    fprintf(stderr, "A save state cannot be loaded without specifying a game to launch.\n");
    return 1;
  }

  Core::AddOnStateChangedCallback([](Core::State state) {
    if (state == Core::State::Uninitialized)
      s_platform->Stop();
  });

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

  DolphinAnalytics::Instance().ReportDolphinStart("nogui");

  if (!BootManager::BootCore(std::move(boot), s_platform->GetWindowSystemInfo()))
  {
    fprintf(stderr, "Could not boot the specified file\n");
    return 1;
  }

#ifdef USE_DISCORD_PRESENCE
  Discord::UpdateDiscordPresence();
#endif

  s_platform->MainLoop();
  Core::Stop();

  Core::Shutdown();
  s_platform.reset();
  UICommon::Shutdown();

  return 0;
}
