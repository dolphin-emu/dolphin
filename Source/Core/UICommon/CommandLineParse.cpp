// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <list>
#include <sstream>
#include <string>
#include <tuple>

#include <OptionParser.h>

#include "Common/Common.h"
#include "Common/Config/Config.h"
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
    if (video_backend.size())
      m_values.emplace_back(std::make_tuple("Dolphin", "Core", "GFXBackend", video_backend));

    if (audio_backend.size())
      m_values.emplace_back(
          std::make_tuple("Dolphin", "Core", "DSPHLE", audio_backend == "HLE" ? "True" : "False"));

    // Arguments are in the format of <System>.<Section>.<Key>=Value
    for (const auto& arg : args)
    {
      std::istringstream buffer(arg);
      std::string system, section, key, value;
      std::getline(buffer, system, '.');
      std::getline(buffer, section, '.');
      std::getline(buffer, key, '=');
      std::getline(buffer, value, '=');
      m_values.emplace_back(std::make_tuple(system, section, key, value));
    }
  }

  void Load(Config::Layer* config_layer) override
  {
    for (auto& value : m_values)
    {
      Config::Section* section = config_layer->GetOrCreateSection(
          Config::GetSystemFromName(std::get<0>(value)), std::get<1>(value));
      section->Set(std::get<2>(value), std::get<3>(value));
    }
  }

  void Save(Config::Layer* config_layer) override
  {
    // Save Nothing
  }

private:
  std::list<std::tuple<std::string, std::string, std::string, std::string>> m_values;
};

std::unique_ptr<optparse::OptionParser> CreateParser(ParserOptions options)
{
  auto parser = std::make_unique<optparse::OptionParser>();
  parser->usage("usage: %prog [options]... [FILE]...").version(scm_rev_str);

  parser->add_option("-u", "--user").action("store").help("User folder path");
  parser->add_option("-m", "--movie").action("store").help("Play a movie file");
  parser->add_option("-e", "--exec")
      .action("store")
      .metavar("<file>")
      .type("string")
      .help("Load the specified file");
  parser->add_option("-C", "--config")
      .action("append")
      .metavar("<System>.<Section>.<Key>=<Value>")
      .type("string")
      .help("Set a configuration option");

  if (options == ParserOptions::IncludeGUIOptions)
  {
    parser->add_option("-d", "--debugger").action("store_true").help("Opens the debuger");
    parser->add_option("-l", "--logger").action("store_true").help("Opens the logger");
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

optparse::Values& ParseArguments(optparse::OptionParser* parser, int argc, char** argv)
{
  optparse::Values& options = parser->parse_args(argc, argv);

  const std::list<std::string>& config_args = options.all("config");
  if (config_args.size())
  {
    Config::AddLayer(std::make_unique<CommandLineConfigLayerLoader>(
        config_args, static_cast<const char*>(options.get("video_backend")),
        static_cast<const char*>(options.get("audio_emulation"))));
  }
  return options;
}
}
