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

#ifndef __ISOPROPERTIES_h__
#define __ISOPROPERTIES_h__

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbmp.h>
#include <wx/imaglist.h>
#include <wx/treectrl.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/mimetype.h>
#include "WxUtils.h"
#include <string>

#include "ISOFile.h"
#include "Filesystem.h"
#include "IniFile.h"
#include "PatchEngine.h"
#include "ActionReplay.h"

class CISOProperties : public wxDialog
{
	public:

		CISOProperties(const std::string fileName,
			wxWindow* parent,
			wxWindowID id = 1,
			const wxString& title = wxT("Properties"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
		virtual ~CISOProperties();

		bool bRefreshList;

	private:

		DECLARE_EVENT_TABLE();
		
		wxStaticBoxSizer *sbGameConfig;
		wxStaticBoxSizer *sbCoreOverrides;
		wxStaticBoxSizer *sbWiiOverrides;
		wxStaticBoxSizer *sbVideoOverrides;
		wxStaticBoxSizer *sbHLEaudioOverrides;
		wxBoxSizer *sEmuState;
		wxBoxSizer *sPatches;
		wxBoxSizer *sPatchButtons;
		wxBoxSizer *sCheats;
		wxBoxSizer *sCheatButtons;
		wxStaticBoxSizer *sbISODetails;
		wxGridBagSizer *sISODetails;
		wxStaticBoxSizer *sbBannerDetails;
		wxGridBagSizer *sBannerDetails;

		wxButton *m_Close;

		wxNotebook *m_Notebook;
		wxPanel *m_GameConfig;
		wxPanel *m_PatchPage;
		wxPanel *m_CheatPage;
		wxPanel *m_Information;
		wxPanel *m_Filesystem;

		wxStaticText *OverrideText;
		// Core
		wxCheckBox *UseDualCore, *SkipIdle, *OptimizeQuantizers, *TLBHack;
		// Wii
		wxCheckBox *EnableProgressiveScan, *EnableWideScreen;
		// Video
		wxCheckBox *ForceFiltering,
			*EFBCopyDisable, *EFBCopyDisableHotKey, *EFBToTextureEnable,
			*SafeTextureCache, *DstAlphaPass, *UseXFB;
		wxStaticText *Hacktext;
		wxArrayString arrayStringFor_Hack;
		wxChoice *Hack;
		// HLE Audio
		wxCheckBox *UseRE0Fix;

		wxButton *EditConfig;
		wxStaticText *EmuStateText;
		wxArrayString arrayStringFor_EmuState;
		wxChoice *EmuState;
		wxTextCtrl *EmuIssues;
		wxArrayString arrayStringFor_Patches;
		wxCheckListBox *Patches;
		wxButton *EditPatch;
		wxButton *AddPatch;
		wxButton *RemovePatch;
		wxArrayString arrayStringFor_Cheats;
		wxCheckListBox *Cheats;
		wxButton *EditCheat;
		wxButton *AddCheat;
		wxButton *RemoveCheat;
		wxArrayString arrayStringFor_Speedhacks;
		wxCheckListBox *Speedhacks;
		wxButton *EditSpeedhack;
		wxButton *AddSpeedhack;
		wxButton *RemoveSpeedhack;

		wxStaticText *m_NameText;
		wxStaticText *m_GameIDText;
		wxStaticText *m_CountryText;
		wxStaticText *m_MakerIDText;
		wxStaticText *m_DateText;
		wxStaticText *m_FSTText;
		wxStaticText *m_LangText;
		wxStaticText *m_ShortText;
		wxStaticText *m_MakerText;
		wxStaticText *m_CommentText;
		wxStaticText *m_BannerText;
		wxTextCtrl *m_Name;
		wxTextCtrl *m_GameID;
		wxTextCtrl *m_Country;
		wxTextCtrl *m_MakerID;
		wxTextCtrl *m_Date;
		wxTextCtrl *m_FST;
		wxArrayString arrayStringFor_Lang;
		wxChoice *m_Lang;
		wxTextCtrl *m_ShortName;
		wxTextCtrl *m_Maker;
		wxTextCtrl *m_Comment;
		wxStaticBitmap *m_Banner;

		wxTreeCtrl *m_Treectrl;
		wxTreeItemId RootId;

		enum
		{
			ID_CLOSE = 1000,
			ID_TREECTRL,

			ID_NOTEBOOK,
			ID_GAMECONFIG,
			ID_PATCH_PAGE,
			ID_ARCODE_PAGE,
			ID_SPEEDHACK_PAGE,
			ID_INFORMATION,
			ID_FILESYSTEM,

			ID_OVERRIDE_TEXT,
			ID_USEDUALCORE,
			ID_IDLESKIP,
			ID_TLBHACK,
			ID_FORCEFILTERING,
			ID_EFBCOPYDISABLE,
			ID_EFBCOPYDISABLEHOTKEY,
			ID_EFBTOTEXTUREENABLE,
			ID_SAFETEXTURECACHE,
			ID_DSTALPHAPASS,
			ID_USEXFB,
			ID_HACK_TEXT,
			ID_HACK,
			ID_RE0FIX,
			ID_ENABLEPROGRESSIVESCAN,
			ID_ENABLEWIDESCREEN,
			ID_OPTIMIZEQUANTIZERS,
			ID_EDITCONFIG,
			ID_EMUSTATE_TEXT,
			ID_EMUSTATE,
			ID_EMUISSUES_TEXT,
			ID_EMU_ISSUES,
			ID_PATCHES_LIST,
			ID_EDITPATCH,
			ID_ADDPATCH,
			ID_REMOVEPATCH,
			ID_CHEATS_LIST,
			ID_EDITCHEAT,
			ID_ADDCHEAT,
			ID_REMOVECHEAT,
			
			ID_NAME_TEXT,
			ID_GAMEID_TEXT,
			ID_COUNTRY_TEXT,
			ID_MAKERID_TEXT,
			ID_DATE_TEXT,
			ID_FST_TEXT,
			ID_VERSION_TEXT,
			ID_LANG_TEXT,
			ID_SHORTNAME_TEXT,
			ID_LONGNAME_TEXT,
			ID_MAKER_TEXT,
			ID_COMMENT_TEXT,
			ID_BANNER_TEXT,
			
			ID_NAME,
			ID_GAMEID,
			ID_COUNTRY,
			ID_MAKERID,
			ID_DATE,
			ID_FST,
			ID_VERSION,
			ID_LANG,
			ID_SHORTNAME,
			ID_LONGNAME,
			ID_MAKER,
			ID_COMMENT,
			ID_BANNER,
			IDM_EXTRACTDIR,
			IDM_EXTRACTFILE,
			IDM_BNRSAVEAS
		};

		void CreateGUIControls(bool);
		void OnClose(wxCloseEvent& event);
		void OnCloseClick(wxCommandEvent& event);
		void OnEditConfig(wxCommandEvent& event);
		void ListSelectionChanged(wxCommandEvent& event);
		void PatchButtonClicked(wxCommandEvent& event);
		void ActionReplayButtonClicked(wxCommandEvent& event);
		void RightClickOnBanner(wxMouseEvent& event);
		void OnBannerImageSave(wxCommandEvent& event);
		void OnRightClickOnTree(wxTreeEvent& event);
		void OnExtractFile(wxCommandEvent& event);
		void OnExtractDir(wxCommandEvent& event);
		void SetRefresh(wxCommandEvent& event);
		void OnChangeBannerLang(wxCommandEvent& event);

		GameListItem *OpenGameListItem;

		std::vector<const DiscIO::SFileInfo *> GCFiles;
		typedef std::vector<const DiscIO::SFileInfo *>::iterator fileIter;

		void CreateDirectoryTree(wxTreeItemId& parent,fileIter& begin,
								 fileIter& end,
								 fileIter& iterPos, char *directory);

		IniFile GameIni;
		std::string GameIniFile;

		void LoadGameConfig();
		bool SaveGameConfig();
		void PatchList_Load();
		void PatchList_Save();
		void ActionReplayList_Load();
		void ActionReplayList_Save();
		void ChangeBannerDetails(int lang);
};
#endif
