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

#include "PluginWiimote.h"

namespace Common {

PluginWiimote::PluginWiimote(const char *_Filename)
	: CPlugin(_Filename), validWiimote(false)
{
	Wiimote_ControlChannel = reinterpret_cast<TWiimote_Output>   
		(LoadSymbol("Wiimote_ControlChannel"));
	Wiimote_InterruptChannel = reinterpret_cast<TWiimote_Input>
		(LoadSymbol("Wiimote_InterruptChannel"));
	Wiimote_Update = reinterpret_cast<TWiimote_Update>
		(LoadSymbol("Wiimote_Update"));
	Wiimote_GetAttachedControllers = reinterpret_cast<TWiimote_GetAttachedControllers>
		(LoadSymbol("Wiimote_GetAttachedControllers"));

	if ((Wiimote_ControlChannel != 0) &&
		(Wiimote_InterruptChannel != 0) &&
		(Wiimote_Update != 0) &&
		(Wiimote_GetAttachedControllers != 0))
		validWiimote = true;
}

PluginWiimote::~PluginWiimote() { }

} // Namespace
