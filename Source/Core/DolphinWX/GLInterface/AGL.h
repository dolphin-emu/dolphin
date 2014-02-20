// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#endif

#include "DolphinWX/GLInterface/InterfaceBase.h"

class cInterfaceAGL : public cInterfaceBase
{
public:
	void Swap();
	bool Create(void *&window_handle);
	bool MakeCurrent();
	bool ClearCurrent();
	void Shutdown();
	void Update();
	void SwapInterval(int interval);

};
