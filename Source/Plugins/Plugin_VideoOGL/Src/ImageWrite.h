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

#ifndef _IMAGEWRITE_H
#define _IMAGEWRITE_H

bool SaveTGA(const char* filename, int width, int height, void* pdata);
bool SaveTexture(const char* filename, u32 textarget, u32 tex, int width, int height);
bool SaveData(const char* filename, const char* pdata);

#endif  // _IMAGEWRITE_H