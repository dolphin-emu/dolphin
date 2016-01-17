// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/CommandLineParse.h"

namespace CommandLineParse
{
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
		return parser->parse_args(argc, argv);
	}

}
