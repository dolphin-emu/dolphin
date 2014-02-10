// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common.h"

#include <X11/X.h>
#include <X11/keysym.h>
#if defined(HAVE_WX) && HAVE_WX
#include <wx/wx.h>
#endif

namespace InputCommon
{
#if defined(HAVE_WX) && HAVE_WX
	KeySym wxCharCodeWXToX(int id);
	int wxKeyModWXToX(int modstate);
#endif
	void XKeyToString(unsigned int keycode, char *keyStr);
}
