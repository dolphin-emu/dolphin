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
#include <wx/imaglist.h>

#include "MemoryCards/GCMemcard.h"
#undef MEMCARD_MANAGER_STYLE
#define MEMCARD_MANAGER_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX | wxRESIZE_BORDER | wxMAXIMIZE_BOX
#define ITEMSPERPAGE 16
#define MAXPAGES (128 / ITEMSPERPAGE) - 1

class CMemcardManager
	: public wxDialog
{
	public:

		CMemcardManager(wxWindow *parent, wxWindowID id = 1, const wxString& title = wxT("Memory Card Manager WARNING-Make backups before using, should be fixed but could mangle stuff!"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = MEMCARD_MANAGER_STYLE);
		virtual ~CMemcardManager();

	private:

		DECLARE_EVENT_TABLE();

		int page0,
			page1;

		wxBoxSizer *sMain;
		wxBoxSizer *sPagesLeft;
		wxBoxSizer *sPagesRight;
		wxStaticText *t_StatusLeft;
		wxStaticText *t_StatusRight;
		wxButton *m_CopyToLeft;
		wxButton *m_CopyToRight;
		wxButton *m_FixChecksumLeft;
		wxButton *m_FixChecksumRight;
		wxButton *m_SaveImportLeft;
		wxButton *m_SaveExportLeft;
		wxButton *m_SaveImportRight;
		wxButton *m_SaveExportRight;
		wxButton *m_ConvertToGci;
		wxButton *m_DeleteLeft;
		wxButton *m_DeleteRight;
		wxButton *m_Memcard1PrevPage;
		wxButton *m_Memcard1NextPage;
		wxButton *m_Memcard2PrevPage;
		wxButton *m_Memcard2NextPage;
		wxStaticBoxSizer *sMemcard1;
		wxStaticBoxSizer *sMemcard2;
		wxFilePickerCtrl *m_Memcard1Path;
		wxFilePickerCtrl *m_Memcard2Path;

		wxListCtrl *m_MemcardList[2];

		enum
		{
			ID_COPYTORIGHT = 1000,
			ID_COPYTOLEFT,
			ID_FIXCHECKSUMRIGHT,
			ID_FIXCHECKSUMLEFT,
			ID_DELETERIGHT,
			ID_DELETELEFT,
			ID_MEMCARD1PATH,
			ID_MEMCARD2PATH,
			ID_MEMCARD1NEXTPAGE,
			ID_MEMCARD1PREVPAGE,
			ID_MEMCARD2NEXTPAGE,
			ID_MEMCARD2PREVPAGE,
			ID_SAVEEXPORTRIGHT,
			ID_SAVEEXPORTLEFT,
			ID_SAVEIMPORTRIGHT,
			ID_SAVEIMPORTLEFT,
			ID_CONVERTTOGCI,
			ID_MEMCARD1LIST,
			ID_MEMCARD2LIST,
			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
		};

		enum
		{
			COLUMN_BANNER = 0,
			COLUMN_TITLE,
			COLUMN_COMMENT,
			COLUMN_ICON,
			COLUMN_BLOCKS,
			COLUMN_FIRSTBLOCK,
			NUMBER_OF_COLUMN
		};
		
		GCMemcard *memoryCard[2];

		void CreateGUIControls();
		void OnClose(wxCloseEvent& event);
		void CopyDeleteClick(wxCommandEvent& event);
		bool ReloadMemcard(const char *fileName, int card, int page);
		void OnPageChange(wxCommandEvent& event);
		void OnPathChange(wxFileDirPickerEvent& event);
		bool ReadError(GCMemcard *memcard);
};

#endif
