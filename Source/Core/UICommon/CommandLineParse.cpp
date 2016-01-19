// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>

#include "Common/OnionConfig.h"

#include "UICommon/CommandLineParse.h"

namespace CommandLineParse
{
	class CommandLineConfigLayerLoader : public OnionConfig::ConfigLayerLoader
	{
	public:
		CommandLineConfigLayerLoader(const std::list<std::string>& args)
			: ConfigLayerLoader(OnionConfig::OnionLayerType::LAYER_COMMANDLINE)
		{
			// Arguments are in the format of <System>.<Petal>.<Key>=Value
			for (const auto& arg : args)
			{
				std::istringstream buffer(arg);
				std::string system, petal, key, value;
				std::getline(buffer, system, '.');
				std::getline(buffer, petal, '.');
				std::getline(buffer, key, '=');
				std::getline(buffer, value, '=');
				m_values.emplace_back(std::make_tuple(system, petal, key, value));
			}
		}

		void Load(OnionConfig::BloomLayer* config_layer) override
		{
			for (auto& value : m_values)
			{
				OnionConfig::OnionPetal* petal = config_layer->GetOrCreatePetal(OnionConfig::GetSystemFromName(std::get<0>(value)), std::get<1>(value));
				petal->Set(std::get<2>(value), std::get<3>(value));
			}
		}

		void Save(OnionConfig::BloomLayer* config_layer) override
		{
			// Save Nothing
		}

	private:
		std::list<std::tuple<std::string, std::string, std::string, std::string>> m_values;
	};

	std::unique_ptr<optparse::OptionParser> CreateParser(bool gui)
	{
		std::unique_ptr<optparse::OptionParser> parser(new optparse::OptionParser());
		parser->usage("usage: %prog [options]... [FILE]...")
			.version(scm_rev_str);

		parser->add_option("-v", "--version")
			.action("version")
			.help("Print version and exit");

		parser->add_option("-u", "--user")
			.action("store")
			.help("User folder path");
		parser->add_option("-m", "--movie")
			.action("store")
			.help("Play a movie file");
		parser->add_option("-e", "--exec")
			.action("store")
			.metavar("<file>")
			.type("string")
			.help("Load the specified file");
		parser->add_option("-C", "--config")
			.action("append")
			.metavar("<System>.<Petal>.<Key>=<Value>")
			.type("string")
			.help("Set a configuration option");

		if (gui)
		{
			parser->add_option("-d", "--debugger")
				.action("store_true")
				.help("Opens the debuger");
			parser->add_option("-l", "--logger")
				.action("store_true")
				.help("Opens the logger");
			parser->add_option("-b", "--batch")
				.action("store_true")
				.help("Exit Dolphin with emulation");
			parser->add_option("-c", "--confirm")
				.action("store_true")
				.help("Set Confirm on Stop");
		}

		// XXX: These two are setting configuration options
		parser->add_option("-v", "--video_backend")
			.action("store")
			.help("Specify a video backend");
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
			OnionConfig::AddLayer(new CommandLineConfigLayerLoader(config_args));

		return options;
	}

}
