// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "PluginPAD.h"

namespace Common
{

PluginPAD::PluginPAD(const char *_Filename) : CPlugin(_Filename), validPAD(false)
{  
	PAD_GetStatus = reinterpret_cast<TPAD_GetStatus>
		(LoadSymbol("PAD_GetStatus"));
	PAD_Input = reinterpret_cast<TPAD_Input>
		(LoadSymbol("PAD_Input"));
	PAD_Rumble = reinterpret_cast<TPAD_Rumble>
		(LoadSymbol("PAD_Rumble"));

	if ((PAD_GetStatus != 0) &&
		(PAD_Input != 0) &&
		(PAD_Rumble != 0))
		validPAD = true;
}

PluginPAD::~PluginPAD() {}

} // Namespace
