// Copyright (C) 2003-2009 Dolphin Project.

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

#define GH3_CONTROLS 14 
enum
{
	EXT_NONE = 0,
	EXT_NUNCHUCK,
	EXT_CLASSIC_CONTROLLER,
	EXT_GUITARHERO3_CONTROLLER,
	EXT_GUITARHEROWT_DRUMS,
};

struct Config
{
	Config();
	void Load(bool ChangePad = false);
	void Save(int Slot = -1);

	struct PadRange
	{
		int Roll;
		int Pitch;
	};

	struct PadTrigger
	{
		enum ETriggerType
		{
			TRIGGER_OFF = 0,
			KEYBOARD,
			ANALOG1,
			ANALOG2,
			TRIGGER
		};
		int Type;
		PadRange Range;
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
			KEYBOARD,
			ANALOG1,
			ANALOG2
		};
		int LType; // Analog Stick
		int RType;
		int TType; // Whammy bar
	};

	// Emulated Wiimote
	bool bSidewaysDPad;
	bool bWideScreen;
	int iExtensionConnected;

	// Real Wiimote
	bool bConnectRealWiimote, bUseRealWiimote, bUpdateRealWiimote;
	int iIRLeft, iIRTop, iIRWidth, iIRHeight;
	int iAccNeutralX, iAccNeutralY, iAccNeutralZ;
	int iAccNunNeutralX, iAccNunNeutralY, iAccNunNeutralZ;

	// Gamepad
	bool bNoTriggerFilter;
	PadTrigger Trigger;
	PadNunchuck Nunchuck;
	PadClassicController ClassicController;
	PadGH3 GH3Controller;
	// Screen size settings
	bool bKeepAR43, bKeepAR169, bCrop;
};

// Ini Control Names
// Do not change the order unless you change the related arrays
static const char* wmControlNames[] = // 14
{ 
	"WmA",
	"WmB",
	"Wm1",
	"Wm2",
	"WmP",
	"WmM",
	"WmH",
	"WmL",
	"WmR",
	"WmU",
	"WmD",
	"WmShake",
	"WmPitchL",
	"WmPitchR",
 };
static const char* ncControlNames[] = // 7
{
	"NcZ",
	"NcC",
	"NcL",
	"NcR",
	"NcU",
	"NcD",
	"NcShake",
};

static const char* ccControlNames[] = // 23 
{
	"CcA",
	"CcB",
	"CcX",
	"CcY",
	"CcP",
	"CcM",
	"CcH",
	"CcTl",
	"CcZl",
	"CcZr",
	"CcTr",
	"CcDl",
	"CcDu",
	"CcDr",
	"CcDd",
	"CcLl",
	"CcLu",
	"CcLr",
	"CcLd",
	"CcRl",
	"CcRu",
	"CcRr",
	"CcRd",
};

static const char* gh3ControlNames[] =
{
	"GH3Green",
	"GH3Red",
	"GH3Yellow",
	"GH3Blue",
	"GH3Orange",
	"GH3Plus",
	"GH3Minus",
	"GH3Whammy",
	"GH3Al",
	"GH3Au",
	"GH3Ar",
	"GH3Ad",
	"GH3StrumUp",
	"GH3StrumDown",
};
// Default controls
static int wmDefaultControls[] = 
{
	65, // WmA
	66, // WmB
	49, // Wm1
	50, // Wm2
	80, // WmP
	77, // WmM
	72, // WmH
#ifdef _WIN32
	VK_LEFT, // Regular directional keys
	VK_RIGHT,
	VK_UP,
	VK_DOWN,
#else
	0,0,0,0,
#endif
	83, // WmShake (S)
	51, // WmPitchL (3)
	52, // WmPitchR (4)
};

static int nCDefaultControls[] =
{
	90, // NcZ Z
	67, // NcC C
#ifdef _WIN32
	VK_NUMPAD4, // NcL
	VK_NUMPAD6, // NcR
	VK_NUMPAD8, // NcU
	VK_NUMPAD5, // NcD
#else
	0,0,0,0,
#endif
	68, // NcShake D
};

static int ccDefaultControls[] =
{
	90, // CcA (C)
	67, // CcB (Z)
	0x58, // CcX (X)
	0x59, // CcY (Y)
	0x4f, // CcP O instead of P
	0x4e, // CcM N instead of M
	0x55, // CcH U instead of H
	0x37, // CcTl 7
	0x38, // CcZl 8
	0x39, // CcZr 9
	0x30, // CcTr 0
#ifdef _WIN32
	VK_NUMPAD4, //CcDl
	VK_NUMPAD8, // CcDu
	VK_NUMPAD6, // CcDr
	VK_NUMPAD5, // CcDd
#else
	0,0,0,0,
#endif
	0x4a, // CcLl J
	0x49, // CcLu I
	0x4c, // CcLr L
	0x4b, // CcLd K
	0x44, // CcRl D 
	0x52, // CcRu R
	0x47, // CcRr G
	0x46, // CcRd F
};
static int GH3DefaultControls[] =
{
	0, // GH3Green
	0, // GH3Red
	0, // GH3Yellow
	0, // GH3Blue
	0, // GH3Orange
	0, // GH3Plus
	0, // GH3Minus
	0, // GH3Whammy
	0, // GH3Al
	0, // GH3Au
	0, // GH3Ar
	0, // GH3Ad
	13, // GH3StrumUp
	161, // GH3StrumDown
};

extern Config g_Config;
#endif  // _PLUGIN_WIIMOTE_CONFIG_H
