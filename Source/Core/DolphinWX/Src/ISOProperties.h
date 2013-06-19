// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __ISOPROPERTIES_h__
#define __ISOPROPERTIES_h__

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/statbmp.h>
#include <wx/imaglist.h>
#include <wx/fontmap.h>
#include <wx/treectrl.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/mimetype.h>
#include <string>

#include "ISOFile.h"
#include "Filesystem.h"
#include "IniFile.h"
#include "PatchEngine.h"
#include "ActionReplay.h"
#include "GeckoCodeDiag.h"

struct PHackData
{
	bool PHackSZNear;
	bool PHackSZFar;
	bool PHackExP;
	std::string PHZNear;
	std::string PHZFar;
};

class CISOProperties : public wxDialog
{
public:
	CISOProperties(const std::string fileName,
			wxWindow* parent,
			wxWindowID id = 1,
			const wxString& title = _("Properties"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
	virtual ~CISOProperties();

	bool bRefreshList;

	void ActionReplayList_Load();
	bool SaveGameConfig();

	PHackData PHack_Data;

private:
	DECLARE_EVENT_TABLE();

	// Core
	wxCheckBox *CPUThread, *SkipIdle, *MMU, *DCBZOFF, *TLBHack;
	wxCheckBox *VBeam, *SyncGPU, *FastDiscSpeed, *BlockMerging, *DSPHLE;
	// Wii
	wxCheckBox *EnableWideScreen;
	// Video
	wxCheckBox *UseZTPSpeedupHack, *PHackEnable, *UseBBox;
	wxButton *PHSettings;

	wxArrayString arrayStringFor_EmuState;
	wxChoice *EmuState;
	wxTextCtrl *EmuIssues;
	wxArrayString arrayStringFor_Patches;
	wxCheckListBox *Patches;
	wxButton *EditPatch;
	wxButton *RemovePatch;
	wxArrayString arrayStringFor_Cheats;
	wxCheckListBox *Cheats;
	wxButton *EditCheat;
	wxButton *RemoveCheat;
	wxArrayString arrayStringFor_Speedhacks;
	wxCheckListBox *Speedhacks;
	wxButton *EditSpeedhack;
	wxButton *AddSpeedhack;
	wxButton *RemoveSpeedhack;

	wxTextCtrl *m_Name;
	wxTextCtrl *m_GameID;
	wxTextCtrl *m_Country;
	wxTextCtrl *m_MakerID;
	wxTextCtrl *m_Revision;
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

	Gecko::CodeConfigPanel *m_geckocode_panel;

	enum
	{
		ID_TREECTRL = 1000,

		ID_NOTEBOOK,
		ID_GAMECONFIG,
		ID_PATCH_PAGE,
		ID_ARCODE_PAGE,
		ID_SPEEDHACK_PAGE,
		ID_INFORMATION,
		ID_FILESYSTEM,

		ID_USEDUALCORE,
		ID_IDLESKIP,
		ID_MMU,
		ID_DCBZOFF,
		ID_TLBHACK,
		ID_VBEAM,
		ID_SYNCGPU,
		ID_DISCSPEED,
		ID_MERGEBLOCKS,
		ID_AUDIO_DSP_HLE,
		ID_USE_BBOX,
		ID_ZTP_SPEEDUP,
		ID_PHACKENABLE,
		ID_PHSETTINGS,
		ID_ENABLEPROGRESSIVESCAN,
		ID_ENABLEWIDESCREEN,
		ID_EDITCONFIG,
		ID_EMUSTATE,
		ID_EMU_ISSUES,
		ID_PATCHES_LIST,
		ID_EDITPATCH,
		ID_ADDPATCH,
		ID_REMOVEPATCH,
		ID_CHEATS_LIST,
		ID_EDITCHEAT,
		ID_ADDCHEAT,
		ID_REMOVECHEAT,

		ID_NAME,
		ID_GAMEID,
		ID_COUNTRY,
		ID_MAKERID,
		ID_REVISION,
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
		IDM_EXTRACTALL,
		IDM_EXTRACTFILE,
		IDM_EXTRACTAPPLOADER,
		IDM_EXTRACTDOL,
		IDM_CHECKINTEGRITY,
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
	void OnExtractDataFromHeader(wxCommandEvent& event);
	void CheckPartitionIntegrity(wxCommandEvent& event);
	void SetRefresh(wxCommandEvent& event);
	void OnChangeBannerLang(wxCommandEvent& event);
	void PHackButtonClicked(wxCommandEvent& event);

	GameListItem *OpenGameListItem;

	std::vector<const DiscIO::SFileInfo *> GCFiles;
	typedef std::vector<const DiscIO::SFileInfo *>::iterator fileIter;

	size_t CreateDirectoryTree(wxTreeItemId& parent,
			std::vector<const DiscIO::SFileInfo*> fileInfos,
			const size_t _FirstIndex, 
			const size_t _LastIndex);
	void ExportDir(const char* _rFullPath, const char* _rExportFilename,
			const int partitionNum = 0);

	IniFile GameIni;
	std::string GameIniFile;

	void LoadGameConfig();
	void PatchList_Load();
	void PatchList_Save();
	void ActionReplayList_Save();
	void ChangeBannerDetails(int lang);
};
#endif
