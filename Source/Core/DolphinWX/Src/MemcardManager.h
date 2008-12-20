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

#include "IniFile.h"
#include "FileUtil.h"
#include "MemoryCards/GCMemcard.h"

#undef MEMCARD_MANAGER_STYLE
#define MEMCARD_MANAGER_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX | wxRESIZE_BORDER | wxMAXIMIZE_BOX
#define MEMCARDMAN_TITLE "Memory Card Manager WARNING-Make backups before using, should be fixed but could mangle stuff!"
#define E_ALREADYOPENED "Memcard already opened"
#define E_INVALID "Invalid bat.map or dir entry"
#define E_NOMEMCARD "File is not recognized as a memcard"
#define E_TITLEPRESENT "Memcard already has a save for this title"
#define E_INVALIDFILESIZE "The save you are trying to copy has an invalid file size"
#define E_OUTOFBLOCKS "Only %d blocks available"
#define E_OUTOFDIRENTRIES "No free dir index entries"
#define E_LENGTHFAIL "Imported file has invalid length"
#define E_GCSFAIL "Imported file has gsc extension\nbut does not have a correct header"
#define E_SAVFAIL "Imported file has sav extension\nbut does not have a correct header"
#define E_OPENFAIL "File could not be opened\nor does not have a valid extension"
#define E_SAVEFAILED "File write failed"
#define E_UNK "Unknown error"
#define FIRSTPAGE 0
#define SLOT_A 0
#define SLOT_B 1

class CMemcardManager
	: public wxDialog
{
	public:

		CMemcardManager(wxWindow *parent, wxWindowID id = 1, const wxString& title = wxT(MEMCARDMAN_TITLE),
			const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = MEMCARD_MANAGER_STYLE);
		virtual ~CMemcardManager();

	private:
		DECLARE_EVENT_TABLE();

		int page[2],
			itemsPerPage,
			maxPages;
		std::string DefaultMemcard[2];
		IniFile MemcardManagerIni;

		wxBoxSizer *sMain,
				   *sPages_A,
				   *sPages_B;
		wxButton *m_CopyFrom[2],
				 *m_FixChecksum[2],
				 *m_SaveImport[2],
				 *m_SaveExport[2],
				 *m_Delete[2],
				 *m_NextPage[2],
				 *m_PrevPage[2],
				 *m_ConvertToGci;
		wxFilePickerCtrl *m_MemcardPath[2];
		wxStaticBoxSizer *sMemcard_A,
						 *sMemcard_B;
		wxStaticText *t_Status[2];

		enum
		{
			ID_COPYFROM_A = 1000,
			ID_COPYFROM_B,
			ID_FIXCHECKSUM_A,
			ID_FIXCHECKSUM_B,
			ID_DELETE_A,
			ID_DELETE_B,
			ID_SAVEEXPORT_A,
			ID_SAVEEXPORT_B,
			ID_SAVEIMPORT_A,
			ID_SAVEIMPORT_B,
			ID_CONVERTTOGCI,
			ID_NEXTPAGE_A,
			ID_NEXTPAGE_B,
			ID_PREVPAGE_A,
			ID_PREVPAGE_B,
			ID_MEMCARDLIST_A,
			ID_MEMCARDLIST_B,
			ID_MEMCARDPATH_A,
			ID_MEMCARDPATH_B,
			ID_USEPAGES,
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
		bool ReloadMemcard(const char *fileName, int card);
		void OnMenuChange(wxCommandEvent& event);
		void OnPageChange(wxCommandEvent& event);
		void OnPathChange(wxFileDirPickerEvent& event);
		void ChangePath(int id);
		bool CopyDeleteSwitch(u32 error, int slot);

		class CMemcardListCtrl : public wxListCtrl
		{
		public:
			IniFile MemcardManagerIni;

			CMemcardListCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style);
			~CMemcardListCtrl();
			bool twoCardsLoaded,
				 usePages,
				 prevPage,
				 nextPage,
				 column[NUMBER_OF_COLUMN];
		private:
			DECLARE_EVENT_TABLE()
			void OnRightClick(wxMouseEvent& event);	
		};
		
		CMemcardListCtrl *m_MemcardList[2];
};

#endif
