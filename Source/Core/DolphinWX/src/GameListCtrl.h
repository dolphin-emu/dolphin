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

#ifndef __GAMELIST_CTRL_H_
#define __GAMELIST_CTRL_H_

#include <vector>

#include "ISOFile.h"

// Define a new application
class CGameListCtrl
	: public wxListCtrl
{
	public:

		CGameListCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style);

		void Update();

		void BrowseForDirectory();


	private:

		enum
		{
			COLUMN_BANNER = 0,
			COLUMN_TITLE,
			COLUMN_COMPANY,
			COLUMN_COUNTRY,
			COLUMN_SIZE,
			COLUMN_EMULATION_STATE,
			NUMBER_OF_COLUMN
		};

		std::vector<int>m_FlagImageIndex;

		bool m_test;
		std::vector<CISOFile>m_ISOFiles;

		void InitBitmaps();
		void InsertItemInReportView(size_t _Index);
		void ScanForISOs();


		DECLARE_EVENT_TABLE()

		// events
		void OnRightClick(wxMouseEvent& event);

		void OnColBeginDrag(wxListEvent& event);
		void OnColEndDrag(wxListEvent& event);
		void OnSelected(wxListEvent& event);
		void OnActivated(wxListEvent& event);
		void OnSize(wxSizeEvent& event);

		virtual bool MSWDrawSubItem(wxPaintDC& rPainDC, int item, int subitem);

		void AutomaticColumnWidth();
};


#endif

