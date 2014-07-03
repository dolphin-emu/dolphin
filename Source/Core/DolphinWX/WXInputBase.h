// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#if defined(HAVE_WX) && HAVE_WX
#include <wx/string.h>
#endif

namespace InputCommon
{
#if defined(HAVE_WX) && HAVE_WX
const wxString WXKeyToString(int keycode);
const wxString WXKeymodToString(int modifier);
#endif
}
