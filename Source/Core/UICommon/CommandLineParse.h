// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <OptionParser.h>

namespace CommandLineParse
{
	std::unique_ptr<optparse::OptionParser> CreateParser(bool gui);
	optparse::Values& ParseArguments(optparse::OptionParser* parser, int argc, char** argv);
}
