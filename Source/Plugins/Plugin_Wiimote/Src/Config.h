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

#ifndef _PLUGIN_WIIMOTE_CONFIG_H
#define _PLUGIN_WIIMOTE_CONFIG_H

#if defined(HAVE_X11) && HAVE_X11
	#include <X11/keysym.h>
#endif

struct Config
{
	Config();
	void Load();
	void Save();

	// For dialog sync
	int CurrentPage;

	// Real Wiimote
	bool bConnectRealWiimote, bUpdateRealWiimote;
	int bFoundRealWiimotes, bNumberRealWiimotes, bNumberEmuWiimotes;
	int iIRLeft, iIRTop, iIRWidth, iIRHeight;
	int iAccNeutralX, iAccNeutralY, iAccNeutralZ;
	int iAccNunNeutralX, iAccNunNeutralY, iAccNunNeutralZ;

	// Screen size settings
	bool bKeepAR43, bKeepAR169, bCrop;
};

extern Config g_Config;
#endif  // _PLUGIN_WIIMOTE_CONFIG_H
