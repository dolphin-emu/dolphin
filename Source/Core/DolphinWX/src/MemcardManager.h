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

#ifndef __MEMCARD_MANAGER_h__
#define __MEMCARD_MANAGER_h__

#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/listctrl.h>

#include "MemoryCards/GCMemcard.h"

#undef MEMCARD_MANAGER_STYLE
#define MEMCARD_MANAGER_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX | wxRESIZE_BORDER

class CMemcardManager
	: public wxDialog
{
	public:

		CMemcardManager(wxWindow* parent, wxWindowID id = 1, const wxString& title = wxT("Memory Card Manager WARNING-In development, make backups first!"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = MEMCARD_MANAGER_STYLE);
		virtual ~CMemcardManager();

	private:

		DECLARE_EVENT_TABLE();

		wxBoxSizer* sMain;
		wxButton* m_CopyRight;
		wxButton* m_CopyLeft;
		wxButton* m_DeleteRight;
		wxButton* m_DeleteLeft;
		wxStaticBoxSizer* sMemcard1;
		wxStaticBoxSizer* sMemcard2;
		wxFilePickerCtrl* m_Memcard1Path;
		wxFilePickerCtrl* m_Memcard2Path;
		wxListCtrl* m_Memcard1List;
		wxListCtrl* m_Memcard2List;

		enum
		{
			ID_COPYRIGHT = 1000,
			ID_COPYLEFT = 1001,
			ID_DELETERIGHT = 1002,
			ID_DELETELEFT = 1003,
			ID_MEMCARD1PATH = 1004,
			ID_MEMCARD2PATH = 1005,
			ID_MEMCARD1LIST = 1006,
			ID_MEMCARD2LIST = 1007,
			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
		};

		enum
		{
			COLUMN_FILENAME = 0,
			COLUMN_COMMENT1,
			COLUMN_COMMENT2,
			NUMBER_OF_COLUMN
		};

		GCMemcard *memoryCard1;
		GCMemcard *memoryCard2;

		void LoadMemcard1(const char *card1);
		void LoadMemcard2(const char *card2);
		void OnPathChange(wxFileDirPickerEvent& event);

		void CreateGUIControls();
		void OnRightClick(wxMouseEvent& event);
		void OnClose(wxCloseEvent& event);
		void CopyClick(wxCommandEvent& event);
		void DeleteClick(wxCommandEvent& event);
};

#endif
