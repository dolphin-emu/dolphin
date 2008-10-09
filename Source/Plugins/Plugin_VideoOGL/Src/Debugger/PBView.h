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

#ifndef __PBView_h__
#define __PBView_h__

#include <wx/listctrl.h>
#include <wx/dcclient.h>

#include "Common.h"

class CPBView
	: public wxListCtrl
{
	public:

		CPBView(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style);

		void Update();

		u32 m_CachedRegs[10][10];		
	

	private:

		DECLARE_EVENT_TABLE()
		
		bool m_CachedRegHasChanged[64];

		virtual bool MSWDrawSubItem(wxPaintDC& rPainDC, int item, int subitem);
};

#endif
