// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _PLUGIN_NJOY_SDL_CONFIG_H
#define _PLUGIN_NJOY_SDL_CONFIG_H

struct Config
{
    Config();
    void Load(bool ChangePad = false, bool ChangeSaveByID = false);
    void Save(int Slot = -1);
	int CheckForDuplicateJoypads(bool OK);

    // General
	bool bShowAdvanced; // Only allow one of these
	bool bSaveByID;
	bool bCheckFocus;
	bool bNoTriggerFilter;
#ifdef RERECORDING
	bool bRecording;
	bool bPlayback;
#endif
};

extern Config g_Config;

#endif  // _PLUGIN_NJOY_SDL_CONFIG_H
