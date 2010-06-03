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

#ifndef _PLUGINPAD_H_
#define _PLUGINPAD_H_

#include "pluginspecs_pad.h"
#include "Plugin.h"

namespace Common {

typedef void (__cdecl* TPAD_GetStatus)(u8, SPADStatus*);
typedef void (__cdecl* TPAD_Input)(u16, u8);
typedef void (__cdecl* TPAD_Rumble)(u8, unsigned int, unsigned int);

class PluginPAD : public CPlugin {
public:
	PluginPAD(const char *_Filename);
	virtual ~PluginPAD();
	virtual bool IsValid() {return validPAD;};

	TPAD_GetStatus PAD_GetStatus;
	TPAD_Input PAD_Input;
	TPAD_Rumble PAD_Rumble;

private:
	bool validPAD;

};

}  // namespace

#endif // _PLUGINPAD_H_
