// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __MEMCARD_MANAGER_h__
#define __MEMCARD_MANAGER_h__

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>
#include <wx/fontmap.h>

#include "IniFile.h"
#include "FileUtil.h"
#include "HW/GCMemcard.h"

#undef MEMCARD_MANAGER_STYLE
#define MEMCARD_MANAGER_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX | wxRESIZE_BORDER | wxMAXIMIZE_BOX
#define MEMCARDMAN_TITLE _trans("Memory Card Manager WARNING-Make backups before using, should be fixed but could mangle stuff!")

#define E_SAVEFAILED "File write failed"
#define E_UNK "Unknown error"
#define FIRSTPAGE 0

class CMemcardManager : public wxDialog
{
	public:

		CMemcardManager(wxWindow *parent, wxWindowID id = wxID_ANY, const wxString& title = wxGetTranslation(wxT(MEMCARDMAN_TITLE)),
			const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = MEMCARD_MANAGER_STYLE);
		virtual ~CMemcardManager();

	private:
		DECLARE_EVENT_TABLE();

		int page[2],
			itemsPerPage,
			maxPages;
		std::string DefaultMemcard[2],
					DefaultIOPath;
		IniFile MemcardManagerIni;
		IniFile::Section* iniMemcardSection;

		wxButton *m_CopyFrom[2],
				 *m_SaveImport[2],
				 *m_SaveExport[2],
				 *m_Delete[2],
				 *m_NextPage[2],
				 *m_PrevPage[2],
				 *m_ConvertToGci;
		wxFilePickerCtrl *m_MemcardPath[2];
		wxStaticText *t_Status[2];

		enum
		{
			ID_COPYFROM_A = 1000,	// Do not rearrange these items,
			ID_COPYFROM_B,			// ID_..._B must be 1 more than ID_..._A
			ID_FIXCHECKSUM_A,
			ID_FIXCHECKSUM_B,
			ID_DELETE_A,
			ID_DELETE_B,
			ID_SAVEEXPORT_A,
			ID_SAVEEXPORT_B,
			ID_SAVEIMPORT_A,
			ID_SAVEIMPORT_B,
			ID_EXPORTALL_A,
			ID_EXPORTALL_B,
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
			COLUMN_GAMECODE,
			COLUMN_MAKERCODE,
			COLUMN_FILENAME,
			COLUMN_BIFLAGS,
			COLUMN_MODTIME,
			COLUMN_IMAGEADD,
			COLUMN_ICONFMT,
			COLUMN_ANIMSPEED,
			COLUMN_PERMISSIONS,
			COLUMN_COPYCOUNTER,
			COLUMN_COMMENTSADDRESS,
			NUMBER_OF_COLUMN
		};
		
		GCMemcard *memoryCard[2];

		void CreateGUIControls();
		void CopyDeleteClick(wxCommandEvent& event);
		bool ReloadMemcard(const char *fileName, int card);
		void OnMenuChange(wxCommandEvent& event);
		void OnPageChange(wxCommandEvent& event);
		void OnPathChange(wxFileDirPickerEvent& event);
		void ChangePath(int id);
		bool CopyDeleteSwitch(u32 error, int slot);
		bool LoadSettings();
		bool SaveSettings();

		struct _mcmSettings
		{
			bool twoCardsLoaded;
			bool usePages;
			bool column[NUMBER_OF_COLUMN + 1];
		} mcmSettings;

		class CMemcardListCtrl : public wxListCtrl
		{
//BEGIN_EVENT_TABLE(CMemcardManager::CMemcardListCtrl, wxListCtrl)
//      EVT_RIGHT_DOWN(CMemcardManager::CMemcardListCtrl::OnRightClick)
//END_EVENT_TABLE()
		public:
			CMemcardListCtrl(wxWindow* parent, const wxWindowID id,
				const wxPoint& pos, const wxSize& size,
				long style, _mcmSettings& _mcmSetngs)
				: wxListCtrl(parent, id, pos, size, style)
				, __mcmSettings(_mcmSetngs)
			{
				Bind(wxEVT_RIGHT_DOWN, &CMemcardListCtrl::OnRightClick, this);
			}
			~CMemcardListCtrl()
			{
				Unbind(wxEVT_RIGHT_DOWN, &CMemcardListCtrl::OnRightClick, this);
			}
			_mcmSettings & __mcmSettings;
			bool prevPage,
				 nextPage;
		private:
			void OnRightClick(wxMouseEvent& event);	
		};
		
		CMemcardListCtrl *m_MemcardList[2];
};

#endif
