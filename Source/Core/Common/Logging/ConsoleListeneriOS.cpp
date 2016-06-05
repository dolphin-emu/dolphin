// Copyright 2015 Dolphin Emulator Project
// Copyright 2016 Will Cobb
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Logging/ConsoleListener.h"

ConsoleListener::ConsoleListener()
{
}

ConsoleListener::~ConsoleListener()
{
}

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char* text)
{

	// Might be useful to support Xcode colors here eventually
	switch(level)
	{
	case LogTypes::LOG_LEVELS::LDEBUG:
        printf("Debug: ");
		break;
	case LogTypes::LOG_LEVELS::LINFO:
		printf("Info: ");
		break;
	case LogTypes::LOG_LEVELS::LWARNING:
		printf("Warning: ");
		break;
	case LogTypes::LOG_LEVELS::LERROR:
		printf("Error: ");
		break;
	case LogTypes::LOG_LEVELS::LNOTICE:
		printf("Notice: ");
		break;
	}

    printf("%s\n", text);
}
