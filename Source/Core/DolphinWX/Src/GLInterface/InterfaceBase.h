// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _GLINTERFACEBASE_H_
#define _GLINTERFACEBASE_H_
class cInterfaceBase
{
protected:
	// Window dimensions.
	u32 s_backbuffer_width;
	u32 s_backbuffer_height;
public:
	virtual void Swap() {}
	virtual void UpdateFPSDisplay(const char *Text) {}
	virtual bool Create(void *&window_handle) { return true; }
	virtual bool MakeCurrent() { return true; }
	virtual bool ClearCurrent() { return true; }
	virtual void Shutdown() {} 

	virtual void SwapInterval(int Interval) { }
	virtual u32 GetBackBufferWidth() { return s_backbuffer_width; }
	virtual u32 GetBackBufferHeight() { return s_backbuffer_height; }
	virtual void SetBackBufferDimensions(u32 W, u32 H) {s_backbuffer_width = W; s_backbuffer_height = H; }
	virtual void Update() { } 
	virtual bool PeekMessages() { return false; }
};
#endif
