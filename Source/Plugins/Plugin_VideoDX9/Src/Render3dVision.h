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

#ifndef _RENDER3DVISION_H
#define _RENDER3DVISION_H

#include "CommonTypes.h"
#include "VideoCommon.h"

//3d vision pipline functions and variables.
//This class provides services needed to run 3d vision.
//It could be replaced by putting the config settings in the
//VideoSettings class, but this way the dx9 plugin can stay portable
//with no reliance on Core being updated. This can be modified if
//these changes are added to the trunk.
class Render3dVision
{
private:
	static bool enable3dVision;

public:
	//Check to see if 3d vision compatable rendering should be used.
	static bool isEnable3dVision();

	//Set 3d vision enabled or disabled, used by the config dialog.
	static void setEnable3dVision(bool value);

	//Lod the specified config file and get enable3dVision out of it.
	static void loadConfig(const char* filename);

	//Save the enable3dVision variable to the specified config file.
	static void saveConfig(const char* filename);

	static void Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight,const EFBRectangle& rc);
};

#endif
