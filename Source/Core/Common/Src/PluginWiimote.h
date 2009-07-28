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

#ifndef _PLUGINWIIMOTE_H_
#define _PLUGINWIIMOTE_H_

#include "pluginspecs_wiimote.h"
#include "Plugin.h"

namespace Common {
    
typedef unsigned int (__cdecl* TPAD_GetAttachedPads)();
typedef void (__cdecl* TWiimote_Update)();
typedef void (__cdecl* TWiimote_Output)(u16 _channelID, const void* _pData, u32 _Size);
typedef void (__cdecl* TWiimote_Input)(u16 _channelID, const void* _pData, u32 _Size);
typedef unsigned int (__cdecl* TWiimote_GetAttachedControllers)();

class PluginWiimote : public CPlugin {
public:
	PluginWiimote(const char *_Filename);
	virtual ~PluginWiimote();
	virtual bool IsValid() {return validWiimote;};

	TWiimote_Output Wiimote_ControlChannel;
	TWiimote_Input  Wiimote_InterruptChannel;
	TWiimote_Update Wiimote_Update;
	TWiimote_GetAttachedControllers Wiimote_GetAttachedControllers;

private:
	bool validWiimote;
};

}  // namespace

#endif // _PLUGINWIIMOTE_H_
