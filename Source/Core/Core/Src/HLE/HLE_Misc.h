// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef HLE_MISC_H
#define HLE_MISC_H

namespace HLE_Misc
{
	void HLEPanicAlert();
	void UnimplementedFunction();
	void HBReload();
	void OSBootDol();
	void OSGetResetCode();
	void HLEGeckoCodehandler();
}

#endif
