// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonFuncs.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"

#ifdef _WIN32
#define _assert_msg_(_t_, _a_, _fmt_, ...) \
	if (!(_a_)) {\
		if (!PanicYesNo(_fmt_, __VA_ARGS__)) \
			Crash(); \
	}

#define _dbg_assert_msg_(_t_, _a_, _msg_, ...)\
	if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG && !(_a_)) {\
		ERROR_LOG(_t_, _msg_, __VA_ARGS__); \
		if (!PanicYesNo(_msg_, __VA_ARGS__)) \
			Crash(); \
	}
#else
#define _assert_msg_(_t_, _a_, _fmt_, ...) \
	if (!(_a_)) {\
		if (!PanicYesNo(_fmt_, ##__VA_ARGS__)) \
			Crash(); \
	}

#define _dbg_assert_msg_(_t_, _a_, _msg_, ...)\
	if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG && !(_a_)) {\
		ERROR_LOG(_t_, _msg_, ##__VA_ARGS__); \
		if (!PanicYesNo(_msg_, ##__VA_ARGS__)) \
			Crash(); \
	}
#endif

#define _assert_(_a_) \
	_assert_msg_(MASTER_LOG, _a_, "Error...\n\n  Line: %d\n  File: %s\n  Time: %s\n\nIgnore and continue?", \
	             __LINE__, __FILE__, __TIME__)

#define _dbg_assert_(_t_, _a_) \
	if (MAX_LOGLEVEL >= LogTypes::LOG_LEVELS::LDEBUG) \
		_assert_(_a_)
