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

#ifndef __BREAKPOINTVIEW_h__
#define __BREAKPOINTVIEW_h__

#include <wx/listctrl.h>

#include "Common.h"
#include "BreakpointWindow.h"

class CBreakPointView
	: public wxListCtrl
{
	public:

		CBreakPointView(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style);

		void Update();
		void DeleteCurrentSelection();
};

class CBreakPointBar
	: public wxListCtrl
{
	public:

		CBreakPointBar(CBreakPointWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style);

		void PopulateBar();

	private:
		void OnSelectItem(wxListEvent& event);

		enum
		{
			Toolbar_Delete,
			Toolbar_Add_BP,
			Toolbar_Add_MC,
			Bitmaps_max
		};

		CBreakPointWindow* BPWindow;
		wxBitmap m_Bitmaps[Bitmaps_max];
};

#endif
