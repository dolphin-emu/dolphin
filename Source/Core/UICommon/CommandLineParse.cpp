// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/CommandLineParse.h"

#include <fmt/ostream.h>
#include <list>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>

#include <OptionParser.h>

#include "Common/Config/Config.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/NetplaySettings.h"

namespace CommandLineParse
{
class CommandLineConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  CommandLineConfigLayerLoader(const std::list<std::string>& args, const std::string& video_backend,
                               const std::string& audio_backend,
                               const std::optional<u16> netplay_host,
                               const std::string& netplay_join, bool batch, bool debugger)
      : ConfigLayerLoader(Config::LayerType::CommandLine)
  {
    if (!video_backend.empty())
      m_values.emplace_back(Config::MAIN_GFX_BACKEND.GetLocation(), video_backend);

    if (!audio_backend.empty())
    {
      m_values.emplace_back(Config::MAIN_DSP_HLE.GetLocation(),
                            ValueToString(audio_backend == "HLE"));
    }

    if (netplay_host.has_value())
    {
      const std::string traversal_choice = Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE);
      const bool is_traversal = traversal_choice == "traversal";

      Config::Location config = is_traversal ? Config::NETPLAY_TRAVERSAL_PORT.GetLocation() :
                                               Config::NETPLAY_HOST_PORT.GetLocation();

      m_values.emplace_back(config, ValueToString(netplay_host.value()));
    }

    if (!netplay_join.empty())
    {
      std::vector<std::string> join_parts = SplitString(netplay_join, ':');
      if (join_parts.size() < 2)
      {
        // The user is submitting a host code
        const std::string& host_code = join_parts[0];
        m_values.emplace_back(Config::NETPLAY_TRAVERSAL_CHOICE.GetLocation(), "traversal");
        m_values.emplace_back(Config::NETPLAY_HOST_CODE.GetLocation(), host_code);
      }
      else
      {
        // Ther user is submitting an IP address
        const std::string& host = join_parts[0];
        const std::string& port_str = join_parts[1];
        if (!std::all_of(port_str.begin(), port_str.end(), ::isdigit) || port_str.length() > 5)
        {
          fmt::println(std::cerr, "Error: the port must be a number between 0-{}.",
                       std::numeric_limits<uint16_t>::max());
          std::exit(EXIT_FAILURE);
        }

        const u64 port = std::stoul(port_str);
        if (port > std::numeric_limits<uint16_t>::max() || port < 1)
        {
          fmt::println(std::cerr, "Error: the port must be a number between 0-{}.",
                       std::numeric_limits<uint16_t>::max());
          std::exit(EXIT_FAILURE);
        }

        const u16 cast_port = static_cast<uint16_t>(port);

        m_values.emplace_back(Config::NETPLAY_ADDRESS.GetLocation(), host);
        m_values.emplace_back(Config::NETPLAY_CONNECT_PORT.GetLocation(), ValueToString(cast_port));
      }
    }

    // Batch mode hides the main window, and render to main hides the render window. To avoid a
    // situation where we would have no window at all, disable render to main when using batch mode.
    if (batch)
      m_values.emplace_back(Config::MAIN_RENDER_TO_MAIN.GetLocation(), ValueToString(false));

    if (debugger)
      m_values.emplace_back(Config::MAIN_ENABLE_DEBUGGING.GetLocation(), ValueToString(true));

    // Arguments are in the format of <System>.<Section>.<Key>=Value
    for (const auto& arg : args)
    {
      std::istringstream buffer(arg);
      std::string system_str, section, key, value;
      std::getline(buffer, system_str, '.');
      std::getline(buffer, section, '.');
      std::getline(buffer, key, '=');
      std::getline(buffer, value, '=');
      std::optional<Config::System> system = Config::GetSystemFromName(system_str);
      if (system)
      {
        m_values.emplace_back(
            Config::Location{std::move(*system), std::move(section), std::move(key)},
            std::move(value));
      }
    }
  }

  void Load(Config::Layer* layer) override
  {
    for (auto& value : m_values)
    {
      layer->Set(std::get<0>(value), std::get<1>(value));
    }
  }

  void Save(Config::Layer* layer) override
  {
    // Save Nothing
  }

private:
  std::list<std::tuple<Config::Location, std::string>> m_values;
};

std::unique_ptr<optparse::OptionParser> CreateParser(ParserOptions options)
{
  auto parser = std::make_unique<optparse::OptionParser>();
  parser->usage("usage: %prog [options]... [FILE]...").version(Common::GetScmRevStr());

  parser->add_option("-u", "--user").action("store").help("User folder path");
  parser->add_option("-m", "--movie").action("store").help("Play a movie file");
  parser->add_option("-e", "--exec")
      .action("append")
      .metavar("<file>")
      .type("string")
      .help("Load the specified file");
  parser->add_option("-n", "--nand_title")
      .action("store")
      .metavar("<16-character ASCII title ID>")
      .type("string")
      .help("Launch a NAND title");
  parser->add_option("-C", "--config")
      .action("append")
      .metavar("<System>.<Section>.<Key>=<Value>")
      .type("string")
      .help("Set a configuration option");
  parser->add_option("-s", "--save_state")
      .action("store")
      .metavar("<file>")
      .type("string")
      .help("Load the initial save state");

  if (options == ParserOptions::IncludeGUIOptions)
  {
    parser->add_option("-d", "--debugger")
        .action("store_true")
        .help("Show the debugger pane and additional View menu options");
    parser->add_option("-l", "--logger").action("store_true").help("Open the logger");
    parser->add_option("-b", "--batch")
        .action("store_true")
        .help("Run Dolphin without the user interface (Requires --exec or --nand-title)");
    parser->add_option("-c", "--confirm").action("store_true").help("Set Confirm on Stop");
    parser->add_option("--netplay_host")
        .action("store")
        .metavar("<port>")
        .type("int")
        .help("Host a netplay session on the specified port (Requires --exec)");
    parser->add_option("--netplay_join")
        .action("store")
        .metavar("<ip:port> OR <host code>")
        .type("string")
        .help("Join a netplay session at the specified IP address and port or using a host code");
  }

  parser->set_defaults("video_backend", "");
  parser->set_defaults("audio_emulation", "");
  parser->add_option("-v", "--video_backend").action("store").help("Specify a video backend");
  parser->add_option("-a", "--audio_emulation")
      .choices({"HLE", "LLE"})
      .help("Choose audio emulation from [%choices]");

  return parser;
}

static void AddConfigLayer(const optparse::Values& options)
{
  std::list<std::string> config_args;
  if (options.is_set_by_user("config"))
    config_args = options.all("config");

  Config::AddLayer(std::make_unique<CommandLineConfigLayerLoader>(
      std::move(config_args), static_cast<const char*>(options.get("video_backend")),
      static_cast<const char*>(options.get("audio_emulation")),
      options.is_set("netplay_host") ?
          std::optional<u16>(static_cast<u16>(options.get("netplay_host"))) :
          std::nullopt,
      static_cast<const char*>(options.get("netplay_join")),
      static_cast<bool>(options.get("batch")), static_cast<bool>(options.get("debugger"))));
}

optparse::Values& ParseArguments(optparse::OptionParser* parser, int argc, char** argv)
{
  optparse::Values& options = parser->parse_args(argc, argv);
  AddConfigLayer(options);
  return options;
}

optparse::Values& ParseArguments(optparse::OptionParser* parser,
                                 const std::vector<std::string>& arguments)
{
  optparse::Values& options = parser->parse_args(arguments);
  AddConfigLayer(options);
  return options;
}
}  // namespace CommandLineParse
