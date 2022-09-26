// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _AMBASEBOARD_H
#define _AMBASEBOARD_H

#include "Common/CommonTypes.h"
class PointerWrap;

namespace AMBaseboard
{
	void	Init( void );
	u32		ExecuteCommand( u32 Command, u32 Length, u32 Address, u32 Offset );
	u32		GetControllerType( void );
	void	Shutdown( void );
	u32 gameid;
};

#endif
