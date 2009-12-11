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

#define AN_CONTROLS 6
#define WM_CONTROLS 16
#define NC_CONTROLS 7
#define CC_CONTROLS 23
#define GH3_CONTROLS 14

enum
{
	EXT_NONE = 0,
	EXT_NUNCHUCK,
	EXT_CLASSIC_CONTROLLER,
	EXT_GUITARHERO3_CONTROLLER,
};

struct Config
{
	Config();
	void Load(bool ChangePad = false);
	void Save(int Slot = -1);

	struct TiltRange
	{
		int Roll;
		int Pitch;
	};

	struct PadTilt
	{
		enum ETiltType
		{
			OFF = 0,
			KEYBOARD,
			ANALOG1,
			ANALOG2,
			TRIGGER
		};
		int Type;
		TiltRange Range;
	};
	struct PadNunchuck
	{
		enum ENunchuckStick
		{
			KEYBOARD,
			ANALOG1,
			ANALOG2
		};
		int Type;
	};
	struct PadClassicController
	{
		enum ECcStick
		{
			KEYBOARD,
			ANALOG1,
			ANALOG2,
			TRIGGER
		};
		int LType;
		int RType;
		int TType;
	};

	struct PadGH3
	{
		enum EGH3Stick
		{
//			KEYBOARD,
			ANALOG1,
			ANALOG2
		};
		int AType; // Analog Stick
		int TType; // Whammy bar
	};

	// Emulated Wiimote
	bool bInputActive;
	bool bSideways;
	bool bUpright;
	bool bWideScreen;
	bool bMotionPlusConnected;
	int iExtensionConnected;

	// Real Wiimote
	bool bConnectRealWiimote, bUseRealWiimote, bUpdateRealWiimote;
	int iIRLeft, iIRTop, iIRWidth, iIRHeight;
	int iAccNeutralX, iAccNeutralY, iAccNeutralZ;
	int iAccNunNeutralX, iAccNunNeutralY, iAccNunNeutralZ;

	// Gamepad
	bool bNoTriggerFilter;
	PadTilt Tilt;
	PadNunchuck Nunchuck;
	PadClassicController ClassicController;
	PadGH3 GH3Controller;
	// Screen size settings
	bool bKeepAR43, bKeepAR169, bCrop;
};

extern Config g_Config;
#endif  // _PLUGIN_WIIMOTE_CONFIG_H
