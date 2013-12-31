// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __BREAKPOINTVIEW_h__
#define __BREAKPOINTVIEW_h__

#include <wx/listctrl.h>
#include "Common.h"

class CBreakPointView : public wxListCtrl
{
public:
	CBreakPointView(wxWindow* parent, const wxWindowID id);

	void Update();
	void DeleteCurrentSelection();
};

#endif
