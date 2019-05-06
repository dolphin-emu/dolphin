// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
#include "UICommon/CommandLineParse.h"

namespace CommandLineParse
{
class CommandLineConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  CommandLineConfigLayerLoader(const std::list<std::string>& args, const std::string& video_backend,
                               const std::string& audio_backend)
      : ConfigLayerLoader(Config::LayerType::CommandLine)
  {
    if (!video_backend.empty())
      m_values.emplace_back(std::make_tuple(Config::MAIN_GFX_BACKEND.location, video_backend));

    if (!audio_backend.empty())
      m_values.emplace_back(
          std::make_tuple(Config::MAIN_DSP_HLE.location, ValueToString(audio_backend == "HLE")));

    // Arguments are in the format of <System>.<Section>.<Key>=Value
    for (const auto& arg : args)
    {
      std::istringstream buffer(arg);
      std::string system_str, section, key, value;
      std::getline(buffer, system_str, '.');
      std::getline(buffer, section, '.');
      std::getline(buffer, key, '=');
      std::getline(buffer, value, '=');
      const std::optional<Config::System> system = Config::GetSystemFromName(system_str);
      if (system)
      {
        m_values.emplace_back(
            std::make_tuple(Config::ConfigLocation{*system, section, key}, value));
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
  std::list<std::tuple<Config::ConfigLocation, std::string>> m_values;
};

std::unique_ptr<optparse::OptionParser> CreateParser(ParserOptions options)
{
  auto parser = std::make_unique<optparse::OptionParser>();
  parser->usage("usage: %prog [options]... [FILE]...").version(Common::scm_rev_str);

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

  if (options == ParserOptions::IncludeGUIOptions)
  {
    parser->add_option("-d", "--debugger")
        .action("store_true")
        .help("Show the debugger pane and additional View menu options");
    parser->add_option("-l", "--logger").action("store_true").help("Open the logger");
    parser->add_option("-b", "--batch").action("store_true").help("Exit Dolphin with emulation");
    parser->add_option("-c", "--confirm").action("store_true").help("Set Confirm on Stop");
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
  if (options.is_set_by_user("config"))
  {
    const std::list<std::string>& config_args = options.all("config");
    Config::AddLayer(std::make_unique<CommandLineConfigLayerLoader>(
        config_args, static_cast<const char*>(options.get("video_backend")),
        static_cast<const char*>(options.get("audio_emulation"))));
  }
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
