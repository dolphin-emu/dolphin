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
#ifndef _GLINTERFACEBASE_H_
#define _GLINTERFACEBASE_H_
class cInterfaceBase
{
protected:
	// Window dimensions.
	u32 s_backbuffer_width;
	u32 s_backbuffer_height;
public:
	virtual void Swap() = 0;
	virtual void UpdateFPSDisplay(const char *Text) = 0;
	virtual bool Create(void *&window_handle) = 0;
	virtual bool MakeCurrent() = 0;
	virtual void Shutdown() = 0; 

	virtual u32 GetBackBufferWidth() { return s_backbuffer_width; }
	virtual u32 GetBackBufferHeight() { return s_backbuffer_height; }
	virtual void SetBackBufferDimensions(u32 W, u32 H) {s_backbuffer_width = W; s_backbuffer_height = H; }
	virtual void Update() { } 
	virtual bool PeekMessages() { return false; }
};
#endif
