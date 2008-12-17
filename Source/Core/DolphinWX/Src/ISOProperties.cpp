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

#include "Globals.h"

#include "ISOFile.h"
#include "VolumeCreator.h"
#include "Filesystem.h"
#include "ISOProperties.h"
#include "PatchAddEdit.h"
#include "ARCodeAddEdit.h"


DiscIO::IVolume *OpenISO = NULL;
DiscIO::IFileSystem *pFileSystem = NULL;

std::vector<PatchEngine::Patch> onFrame;
std::vector<ActionReplay::ARCode> arCodes;


BEGIN_EVENT_TABLE(CISOProperties, wxDialog)
	EVT_CLOSE(CISOProperties::OnClose)
	EVT_BUTTON(ID_CLOSE, CISOProperties::OnCloseClick)
	EVT_BUTTON(ID_EDITCONFIG, CISOProperties::OnEditConfig)
	EVT_CHOICE(ID_EMUSTATE, CISOProperties::SetRefresh)
	EVT_LISTBOX(ID_PATCHES_LIST, CISOProperties::ListSelectionChanged)
	EVT_BUTTON(ID_EDITPATCH, CISOProperties::PatchButtonClicked)
	EVT_BUTTON(ID_ADDPATCH, CISOProperties::PatchButtonClicked)
	EVT_BUTTON(ID_REMOVEPATCH, CISOProperties::PatchButtonClicked)
	EVT_LISTBOX(ID_CHEATS_LIST, CISOProperties::ListSelectionChanged)
	EVT_BUTTON(ID_EDITCHEAT, CISOProperties::ActionReplayButtonClicked)
	EVT_BUTTON(ID_ADDCHEAT, CISOProperties::ActionReplayButtonClicked)
	EVT_BUTTON(ID_REMOVECHEAT, CISOProperties::ActionReplayButtonClicked)
	EVT_MENU(IDM_BNRSAVEAS, CISOProperties::OnBannerImageSave)
	EVT_TREE_ITEM_RIGHT_CLICK(ID_TREECTRL, CISOProperties::OnRightClickOnTree)
	EVT_MENU(IDM_EXTRACTFILE, CISOProperties::OnExtractFile)
	EVT_MENU(IDM_EXTRACTDIR, CISOProperties::OnExtractDir)
END_EVENT_TABLE()

CISOProperties::CISOProperties(const std::string fileName, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	OpenISO = DiscIO::CreateVolumeFromFilename(fileName);
	pFileSystem = DiscIO::CreateFileSystem(OpenISO);

	pFileSystem->GetFileList(Our_Files);

	GameListItem OpenISO_(fileName);

	bRefreshList = false;
	CreateGUIControls();

	GameIniFile = FULL_GAMECONFIG_DIR + (OpenISO->GetUniqueID()) + ".ini";
	if (GameIni.Load(GameIniFile.c_str()))
		LoadGameConfig();
	else
	{
		FILE *f = fopen(GameIniFile.c_str(), "w");
		fprintf(f, "# %s - %s\n", OpenISO->GetUniqueID().c_str(), OpenISO->GetName().c_str());
		fprintf(f, "[Core]\n#Values set here will override the main dolphin settings.\n");
		fprintf(f, "[EmuState]\n#The Emulation State. 1 is worst, 5 is best, 0 is not set.\n");
		fprintf(f, "[OnFrame]\n#Add memory patches to be applied every frame here.\n");
		fprintf(f, "[ActionReplay]\n#Add action replay cheats here.\n");
		fclose(f);
		if (GameIni.Load(GameIniFile.c_str()))
			LoadGameConfig();
		else
			wxMessageBox(wxString::Format(_("Could not create %s"), GameIniFile.c_str()), _("Error"), wxOK|wxICON_ERROR, this);
	}

	// Disk header and apploader
	m_Name->SetValue(wxString(OpenISO->GetName().c_str(), wxConvUTF8));
	m_GameID->SetValue(wxString(OpenISO->GetUniqueID().c_str(), wxConvUTF8));
	switch (OpenISO->GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_EUROPE:
	case DiscIO::IVolume::COUNTRY_FRANCE:
		m_Country->SetValue(wxString::FromAscii("EUR"));
		break;
	case DiscIO::IVolume::COUNTRY_USA:
		m_Country->SetValue(wxString::FromAscii("USA"));
		break;
	case DiscIO::IVolume::COUNTRY_JAP:
		m_Country->SetValue(wxString::FromAscii("JAP"));
		break;
	default:
		m_Country->SetValue(wxString::FromAscii("UNKNOWN"));
		break;
	}
	wxString temp;
	temp = _T("0x") + wxString::FromAscii(OpenISO->GetMakerID().c_str());
	m_MakerID->SetValue(temp);
	m_Date->SetValue(wxString(OpenISO->GetApploaderDate().c_str(), wxConvUTF8));
	m_FST->SetValue(wxString::Format(_T("%u"), OpenISO->GetFSTSize()));

	// Banner
	// ...all the BannerLoader functions are bool...gross
	//m_Version;
	//if (OpenISO_.GetBNRVersion() == "BNR1")
		m_Lang->Enable(false);
	m_ShortName->SetValue(wxString(OpenISO_.GetName().c_str(), wxConvUTF8));
	//m_LongName->SetValue(wxString(OpenISO_.GetLongName().c_str(), wxConvUTF8));
	m_Maker->SetValue(wxString(OpenISO_.GetCompany().c_str(), wxConvUTF8));//dev too
	m_Comment->SetValue(wxString(OpenISO_.GetDescription().c_str(), wxConvUTF8));
	m_Banner->SetBitmap(OpenISO_.GetImage());
	m_Banner->Connect(wxID_ANY, wxEVT_RIGHT_DOWN,
		wxMouseEventHandler(CISOProperties::RightClickOnBanner), (wxObject*)NULL, this);

	// Filesystem browser/dumper
	fileIter beginning = Our_Files.begin(), end = Our_Files.end(), pos = Our_Files.begin();
	CreateDirectoryTree(RootId, beginning, end, pos, (char *)"\\");	
	m_Treectrl->Expand(RootId);

	std::string filename, extension;
	SplitPath(fileName, 0, &filename, &extension);
	SetTitle(wxString::Format(_("%s%s: %s - %s"), filename.c_str(), extension.c_str(), OpenISO_.GetUniqueID().c_str(), OpenISO_.GetName().c_str()));
}

CISOProperties::~CISOProperties()
{
    if (pFileSystem)
	delete pFileSystem;
    if (OpenISO)
	delete OpenISO;
}

void CISOProperties::CreateDirectoryTree(wxTreeItemId& parent, 
											fileIter& begin,
											fileIter& end,
											fileIter& iterPos,
											char *directory)
{
	bool bRoot = true;

	if(iterPos == begin)
	 	++iterPos;
	else
		bRoot = false;

	char *name = (char *)((*iterPos)->m_FullPath);

	if(iterPos == end)
		return;

	do
	{
		if((*iterPos)->IsDirectory()) {
			char *dirName;
			name[strlen(name) - 1] = '\0';
			dirName = strrchr(name, '\\');
			if(!dirName)
				dirName = name;
			else
				dirName++;

			wxTreeItemId item = m_Treectrl->AppendItem(parent, wxString::FromAscii(dirName));
			CreateDirectoryTree(item, begin, end, ++iterPos, name);
		} else {
			char *fileName = strrchr(name, '\\');
			if(!fileName)
				fileName = name;
			else
				fileName++;

			m_Treectrl->AppendItem(parent, wxString::FromAscii(fileName));
			++iterPos;
		}

		if(iterPos == end)
			break;
		
		name = (char *)((*iterPos)->m_FullPath);

	} while(bRoot || strstr(name, directory));
}

void CISOProperties::CreateGUIControls()
{
	m_Close = new wxButton(this, ID_CLOSE, _("Close"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);

	// Notebook
	m_Notebook = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	m_GameConfig = new wxPanel(m_Notebook, ID_GAMECONFIG, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_GameConfig, _("GameConfig"));
		m_GameConfig_Notebook = new wxNotebook(m_GameConfig, ID_GAMECONFIG_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
		m_PatchPage = new wxPanel(m_GameConfig_Notebook, ID_PATCH_PAGE, wxDefaultPosition, wxDefaultSize);
		m_GameConfig_Notebook->AddPage(m_PatchPage, _("Patches"));
		m_CheatPage = new wxPanel(m_GameConfig_Notebook, ID_ARCODE_PAGE, wxDefaultPosition, wxDefaultSize);
		m_GameConfig_Notebook->AddPage(m_CheatPage, _("AR Codes"));
	m_Information = new wxPanel(m_Notebook, ID_INFORMATION, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_Information, _("Info"));
	m_Filesystem = new wxPanel(m_Notebook, ID_FILESYSTEM, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_Filesystem, _("Filesystem"));

	wxBoxSizer* sButtons;
	sButtons = new wxBoxSizer(wxHORIZONTAL);
	sButtons->Add(0, 0, 1, wxEXPAND, 5);
	sButtons->Add(m_Close, 0, wxALL, 5);
	
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND, 5);
	
	this->SetSizer(sMain);
	this->Layout();

	// GameConfig editing - Core overrides and emulation state
	sbCoreOverrides = new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Game-Specific Settings"));
	sCoreOverrides = new wxBoxSizer(wxVERTICAL);
	EditConfig = new wxButton(m_GameConfig, ID_EDITCONFIG, _("Edit Config"), wxDefaultPosition, wxDefaultSize);
	OverrideText = new wxStaticText(m_GameConfig, ID_OVERRIDE_TEXT, _("These settings override core Dolphin settings.\nThe 3rd state means the game uses Dolphin's setting."), wxDefaultPosition, wxDefaultSize);
	UseDualCore = new wxCheckBox(m_GameConfig, ID_USEDUALCORE, _("Enable Dual Core"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	SkipIdle = new wxCheckBox(m_GameConfig, ID_IDLESKIP, _("Enable Idle Skipping"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	OptimizeQuantizers = new wxCheckBox(m_GameConfig, ID_OPTIMIZEQUANTIZERS, _("Optimize Quantizers"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	EnableProgressiveScan = new wxCheckBox(m_GameConfig, ID_ENABLEPROGRESSIVESCAN, _("[Wii] Enable Progressive Scan"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	EnableWideScreen = new wxCheckBox(m_GameConfig, ID_ENABLEWIDESCREEN, _("[Wii] Enable WideScreen"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	
	sEmuState = new wxBoxSizer(wxHORIZONTAL);
	arrayStringFor_EmuState.Add(_("Not Set"));
	arrayStringFor_EmuState.Add(_("Broken"));
	arrayStringFor_EmuState.Add(_("Problems: Other"));
	arrayStringFor_EmuState.Add(_("Intro"));
	arrayStringFor_EmuState.Add(_("In Game"));
	arrayStringFor_EmuState.Add(_("Perfect"));
	EmuStateText = new wxStaticText(m_GameConfig, ID_EMUSTATE_TEXT, _("Emulation State: "), wxDefaultPosition, wxDefaultSize);
	EmuState = new wxChoice(m_GameConfig, ID_EMUSTATE, wxDefaultPosition, wxDefaultSize, arrayStringFor_EmuState, 0, wxDefaultValidator);

	// Patches
	sbPatches = new wxStaticBoxSizer(wxVERTICAL, m_PatchPage, _("Patches"));
	sPatches = new wxBoxSizer(wxVERTICAL);
	Patches = new wxCheckListBox(m_PatchPage, ID_PATCHES_LIST, wxDefaultPosition, wxDefaultSize, arrayStringFor_Patches, wxLB_HSCROLL, wxDefaultValidator);
	sPatchButtons = new wxBoxSizer(wxHORIZONTAL);
	EditPatch = new wxButton(m_PatchPage, ID_EDITPATCH, _("Edit..."), wxDefaultPosition, wxDefaultSize, 0);
	AddPatch = new wxButton(m_PatchPage, ID_ADDPATCH, _("Add..."), wxDefaultPosition, wxDefaultSize, 0);
	RemovePatch = new wxButton(m_PatchPage, ID_REMOVEPATCH, _("Remove"), wxDefaultPosition, wxDefaultSize, 0);
	EditPatch->Enable(false);
	RemovePatch->Enable(false);

	// Action Replay Cheats
	sbCheats = new wxStaticBoxSizer(wxVERTICAL, m_CheatPage, _("Action Replay Codes"));
	sCheats = new wxBoxSizer(wxVERTICAL);
	Cheats = new wxCheckListBox(m_CheatPage, ID_CHEATS_LIST, wxDefaultPosition, wxDefaultSize, arrayStringFor_Cheats, wxLB_HSCROLL, wxDefaultValidator);
	sCheatButtons = new wxBoxSizer(wxHORIZONTAL);
	EditCheat = new wxButton(m_CheatPage, ID_EDITCHEAT, _("Edit..."), wxDefaultPosition, wxDefaultSize, 0);
	AddCheat = new wxButton(m_CheatPage, ID_ADDCHEAT, _("Add..."), wxDefaultPosition, wxDefaultSize, 0);
	RemoveCheat = new wxButton(m_CheatPage, ID_REMOVECHEAT, _("Remove"), wxDefaultPosition, wxDefaultSize, 0);
	EditCheat->Enable(false);
	RemoveCheat->Enable(false);

	wxBoxSizer* sConfigPage;
	sConfigPage = new wxBoxSizer(wxVERTICAL);
	sCoreOverrides->Add(OverrideText, 0, wxEXPAND|wxALL, 5);
	sCoreOverrides->Add(UseDualCore, 0, wxEXPAND|wxLEFT, 5);
	sCoreOverrides->Add(SkipIdle, 0, wxEXPAND|wxLEFT, 5);
	sCoreOverrides->Add(OptimizeQuantizers, 0, wxEXPAND|wxLEFT, 5);
	sCoreOverrides->Add(EnableProgressiveScan, 0, wxEXPAND|wxLEFT, 5);
	sCoreOverrides->Add(EnableWideScreen, 0, wxEXPAND|wxLEFT, 5);
	sEmuState->Add(EditConfig, 0, wxALL, 0);
	sEmuState->AddStretchSpacer();
	sEmuState->Add(EmuStateText, 0, wxALIGN_CENTER_VERTICAL|wxALL, 0);
	sEmuState->Add(EmuState, 0, wxEXPAND|wxALL, 0);
	sCoreOverrides->Add(sEmuState, 0, wxEXPAND|wxALL, 5);
	sbCoreOverrides->Add(sCoreOverrides, 0, wxEXPAND|wxALL, 0);
	sConfigPage->Add(sbCoreOverrides, 0, wxEXPAND|wxALL, 5);

	wxBoxSizer* sPatchPage;
	sPatchPage = new wxBoxSizer(wxVERTICAL);
	sPatches->Add(Patches, 1, wxEXPAND|wxALL, 0);
	sPatchButtons->Add(EditPatch,  0, wxEXPAND|wxALL, 0);
	sPatchButtons->AddStretchSpacer();
	sPatchButtons->Add(AddPatch,  0, wxEXPAND|wxALL, 0);
	sPatchButtons->Add(RemovePatch,  0, wxEXPAND|wxALL, 0);
	sPatches->Add(sPatchButtons, 0, wxEXPAND|wxALL, 0);
	sbPatches->Add(sPatches, 1, wxEXPAND|wxALL, 0);
	sPatchPage->Add(sbPatches, 1, wxEXPAND|wxALL, 5);
	m_PatchPage->SetSizer(sPatchPage);
	sPatchPage->Layout();

	wxBoxSizer* sCheatPage;
	sCheatPage = new wxBoxSizer(wxVERTICAL);
	sCheats->Add(Cheats, 1, wxEXPAND|wxALL, 0);
	sCheatButtons->Add(EditCheat,  0, wxEXPAND|wxALL, 0);
	sCheatButtons->AddStretchSpacer();
	sCheatButtons->Add(AddCheat,  0, wxEXPAND|wxALL, 0);
	sCheatButtons->Add(RemoveCheat,  0, wxEXPAND|wxALL, 0);
	sCheats->Add(sCheatButtons, 0, wxEXPAND|wxALL, 0);
	sbCheats->Add(sCheats, 1, wxEXPAND|wxALL, 0);
	sCheatPage->Add(sbCheats, 1, wxEXPAND|wxALL, 5);
	m_CheatPage->SetSizer(sCheatPage);
	sCheatPage->Layout();

	sConfigPage->Add(m_GameConfig_Notebook, 1, wxEXPAND|wxALL, 5);

	m_GameConfig->SetSizer(sConfigPage);
	sConfigPage->Layout();
	
	// ISO Details
	sbISODetails = new wxStaticBoxSizer(wxVERTICAL, m_Information, _("ISO Details"));
	sISODetails = new wxGridBagSizer(0, 0);
	sISODetails->AddGrowableCol(1);
	m_NameText = new wxStaticText(m_Information, ID_NAME_TEXT, _("Name:"), wxDefaultPosition, wxDefaultSize);
	m_Name = new wxTextCtrl(m_Information, ID_NAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_GameIDText = new wxStaticText(m_Information, ID_GAMEID_TEXT, _("Game ID:"), wxDefaultPosition, wxDefaultSize);
	m_GameID = new wxTextCtrl(m_Information, ID_GAMEID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_CountryText = new wxStaticText(m_Information, ID_COUNTRY_TEXT, _("Country:"), wxDefaultPosition, wxDefaultSize);
	m_Country = new wxTextCtrl(m_Information, ID_COUNTRY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_MakerIDText = new wxStaticText(m_Information, ID_MAKERID_TEXT, _("Maker ID:"), wxDefaultPosition, wxDefaultSize);
	m_MakerID = new wxTextCtrl(m_Information, ID_MAKERID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_DateText = new wxStaticText(m_Information, ID_DATE_TEXT, _("Date:"), wxDefaultPosition, wxDefaultSize);
	m_Date = new wxTextCtrl(m_Information, ID_DATE, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_FSTText = new wxStaticText(m_Information, ID_FST_TEXT, _("FST Size:"), wxDefaultPosition, wxDefaultSize);	
	m_FST = new wxTextCtrl(m_Information, ID_FST, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);

	// Banner Details
	sbBannerDetails = new wxStaticBoxSizer(wxVERTICAL, m_Information, _("Banner Details"));
	sBannerDetails = new wxGridBagSizer(0, 0);
	sBannerDetails->AddGrowableCol(1); sBannerDetails->AddGrowableCol(2); sBannerDetails->AddGrowableCol(3);
	m_VersionText = new wxStaticText(m_Information, ID_VERSION_TEXT, _("Version:"), wxDefaultPosition, wxDefaultSize);
	m_Version = new wxTextCtrl(m_Information, ID_VERSION, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_LangText = new wxStaticText(m_Information, ID_LANG_TEXT, _("Show Language:"), wxDefaultPosition, wxDefaultSize);
	arrayStringFor_Lang.Add(_("English"));
	arrayStringFor_Lang.Add(_("German"));
	arrayStringFor_Lang.Add(_("French"));
	arrayStringFor_Lang.Add(_("Spanish"));
	arrayStringFor_Lang.Add(_("Italian"));
	arrayStringFor_Lang.Add(_("Dutch"));
	m_Lang = new wxChoice(m_Information, ID_LANG, wxDefaultPosition, wxDefaultSize, arrayStringFor_Lang, 0, wxDefaultValidator);
	m_Lang->SetSelection(0);
	m_ShortText = new wxStaticText(m_Information, ID_SHORTNAME_TEXT, _("Short Name:"), wxDefaultPosition, wxDefaultSize);
	m_ShortName = new wxTextCtrl(m_Information, ID_SHORTNAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_LongText = new wxStaticText(m_Information, ID_LONGNAME_TEXT, _("Long Name:"), wxDefaultPosition, wxDefaultSize);
	m_LongName = new wxTextCtrl(m_Information, ID_LONGNAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_MakerText = new wxStaticText(m_Information, ID_MAKER_TEXT, _("Maker:"), wxDefaultPosition, wxDefaultSize);
	m_Maker = new wxTextCtrl(m_Information, ID_MAKER, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_CommentText = new wxStaticText(m_Information, ID_COMMENT_TEXT, _("Comment:"), wxDefaultPosition, wxDefaultSize);
	m_Comment = new wxTextCtrl(m_Information, ID_COMMENT, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY);
	m_BannerText = new wxStaticText(m_Information, ID_BANNER_TEXT, _("Banner:"), wxDefaultPosition, wxDefaultSize);
	m_Banner = new wxStaticBitmap(m_Information, ID_BANNER, wxNullBitmap, wxDefaultPosition, wxSize(96, 32), 0);

	wxBoxSizer* sInfoPage;
	sInfoPage = new wxBoxSizer(wxVERTICAL);
	sISODetails->Add(m_NameText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Name, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_GameIDText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_GameID, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_CountryText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Country, wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_MakerIDText, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_MakerID, wxGBPosition(3, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_DateText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Date, wxGBPosition(4, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_FSTText, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_FST, wxGBPosition(5, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sbISODetails->Add(sISODetails, 0, wxEXPAND, 5);
	
	sBannerDetails->Add(m_VersionText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_Version, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_LangText, wxGBPosition(0, 2), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_Lang, wxGBPosition(0, 3), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_ShortText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_ShortName, wxGBPosition(1, 1), wxGBSpan(1, 3), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_LongText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_LongName, wxGBPosition(2, 1), wxGBSpan(1, 3), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_MakerText, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_Maker, wxGBPosition(3, 1), wxGBSpan(1, 3), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_CommentText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALL, 5);
	sBannerDetails->Add(m_Comment, wxGBPosition(4, 1), wxGBSpan(1, 3), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_BannerText, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALL, 5);
	sBannerDetails->Add(m_Banner, wxGBPosition(5, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sbBannerDetails->Add(sBannerDetails, 0, wxEXPAND, 0);
	sInfoPage->Add(sbISODetails, 0, wxEXPAND|wxALL, 5);
	sInfoPage->Add(sbBannerDetails, 0, wxEXPAND|wxALL, 5);
	m_Information->SetSizer(sInfoPage);
	sInfoPage->Layout();
	
	// Filesystem tree
	sbTreectrl = new wxStaticBoxSizer(wxVERTICAL, m_Filesystem, _("Filesystem"));
	m_Treectrl = new wxTreeCtrl(m_Filesystem, ID_TREECTRL, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE, wxDefaultValidator);

	RootId = m_Treectrl->AddRoot(_("Root"), -1, -1, 0);
	
	wxBoxSizer* sTreePage;
	sTreePage = new wxBoxSizer(wxVERTICAL);
	sbTreectrl->Add(m_Treectrl, 1, wxEXPAND);
	sTreePage->Add(sbTreectrl, 1, wxEXPAND|wxALL, 5);
	m_Filesystem->SetSizer(sTreePage);
	sTreePage->Layout();

	Fit();
}

void CISOProperties::OnClose(wxCloseEvent& WXUNUSED (event))
{
	if (!SaveGameConfig())
		wxMessageBox(wxString::Format(_("Could not save %s"), GameIniFile.c_str()), _("Error"), wxOK|wxICON_ERROR, this);

	Destroy();
}

void CISOProperties::OnCloseClick(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

void CISOProperties::RightClickOnBanner(wxMouseEvent& event)
{
	wxMenu popupMenu;
	popupMenu.Append(IDM_BNRSAVEAS, _("Save as..."));
	PopupMenu(&popupMenu);

	event.Skip();
}

void CISOProperties::OnBannerImageSave(wxCommandEvent& WXUNUSED (event))
{
	wxString dirHome;

	wxFileDialog dialog(this, _("Save as..."), wxGetHomeDir(&dirHome), wxString::Format(_("%s.png"), m_GameID->GetLabel().c_str()),
		_("*.*"), wxFD_SAVE|wxFD_OVERWRITE_PROMPT, wxDefaultPosition, wxDefaultSize);
	if (dialog.ShowModal() == wxID_OK)
	{
		m_Banner->GetBitmap().ConvertToImage().SaveFile(dialog.GetPath());
	}
}

void CISOProperties::OnRightClickOnTree(wxTreeEvent& event)
{
	m_Treectrl->SelectItem(event.GetItem());

	wxMenu popupMenu;
	if (m_Treectrl->ItemHasChildren(m_Treectrl->GetSelection()))
		;//popupMenu.Append(IDM_EXTRACTDIR, _("Extract Directory..."));
	else
		popupMenu.Append(IDM_EXTRACTFILE, _("Extract File..."));
	PopupMenu(&popupMenu);

	event.Skip();
}

void CISOProperties::OnExtractFile(wxCommandEvent& WXUNUSED (event))
{
	wxString Path;
	wxString File;

	File = m_Treectrl->GetItemText(m_Treectrl->GetSelection());
	
	Path = wxFileSelector(
		_T("Export File"),
		wxEmptyString, File, wxEmptyString,
		wxString::Format
		(
			_T("All files (%s)|%s"),
			wxFileSelectorDefaultWildcardStr,
			wxFileSelectorDefaultWildcardStr
		),
		wxFD_SAVE,
		this);

	if (!Path || !File)
		return;

	while (m_Treectrl->GetItemParent(m_Treectrl->GetSelection()) != m_Treectrl->GetRootItem())
	{
		wxString temp;
		temp = m_Treectrl->GetItemText(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
		File = temp + _T("\\") + File;

		m_Treectrl->SelectItem(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
	}

	pFileSystem->ExportFile(File.mb_str(), Path.mb_str());
}

void CISOProperties::OnExtractDir(wxCommandEvent& WXUNUSED (event))
{
}

void CISOProperties::SetRefresh(wxCommandEvent& WXUNUSED (event))
{
	bRefreshList = true;
}

void CISOProperties::LoadGameConfig()
{
	bool bTemp;
	int iTemp;

	if (GameIni.Get("Core", "UseDualCore", &bTemp))
		UseDualCore->Set3StateValue((wxCheckBoxState)bTemp);
	else
		UseDualCore->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "SkipIdle", &bTemp))
		SkipIdle->Set3StateValue((wxCheckBoxState)bTemp);
	else
		SkipIdle->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "OptimizeQuantizers", &bTemp))
		OptimizeQuantizers->Set3StateValue((wxCheckBoxState)bTemp);
	else
		OptimizeQuantizers->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "EnableProgressiveScan", &bTemp))
		EnableProgressiveScan->Set3StateValue((wxCheckBoxState)bTemp);
	else
		EnableProgressiveScan->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "EnableWideScreen", &bTemp))
		EnableWideScreen->Set3StateValue((wxCheckBoxState)bTemp);
	else
		EnableWideScreen->Set3StateValue(wxCHK_UNDETERMINED);

	GameIni.Get("EmuState", "EmulationStateId", &iTemp, -1);
	if (iTemp == -1)
	{
		iTemp = 0;
		bRefreshList = true;
	}
	EmuState->SetSelection(iTemp);

	PatchList_Load();
	ActionReplayList_Load();
}

bool CISOProperties::SaveGameConfig()
{
	if (UseDualCore->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "UseDualCore");
	else
		GameIni.Set("Core", "UseDualCore", UseDualCore->Get3StateValue());

	if (SkipIdle->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "SkipIdle");
	else
		GameIni.Set("Core", "SkipIdle", SkipIdle->Get3StateValue());

	if (OptimizeQuantizers->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "OptimizeQuantizers");
	else
		GameIni.Set("Core", "OptimizeQuantizers", OptimizeQuantizers->Get3StateValue());

	if (EnableProgressiveScan->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "EnableProgressiveScan");
	else
		GameIni.Set("Core", "EnableProgressiveScan", EnableProgressiveScan->Get3StateValue());

	if (EnableWideScreen->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "EnableWideScreen");
	else
		GameIni.Set("Core", "EnableWideScreen", EnableWideScreen->Get3StateValue());

	GameIni.Set("EmuState", "EmulationStateId", EmuState->GetSelection());

	PatchList_Save();
	ActionReplayList_Save();

	return GameIni.Save(GameIniFile.c_str());
}

void CISOProperties::OnEditConfig(wxCommandEvent& WXUNUSED (event))
{
	if (wxFileExists(wxString::FromAscii(GameIniFile.c_str())))
	{
		SaveGameConfig();

		wxFileType* filetype = wxTheMimeTypesManager->GetFileTypeFromExtension(_("ini"));
		if(filetype == NULL) // From extension failed, trying with MIME type now
		{
			filetype = wxTheMimeTypesManager->GetFileTypeFromMimeType(_("text/plain"));
			if(filetype == NULL) // MIME type failed, aborting mission
			{
				PanicAlert("Filetype 'ini' is unknown! Will not open!");
				return;
			}
		}
		wxString OpenCommand;
		OpenCommand = filetype->GetOpenCommand(wxString::FromAscii(GameIniFile.c_str()));
		if(OpenCommand.IsEmpty())
			PanicAlert("Couldn't find open command for extension 'ini'!");
		else
			if(wxExecute(OpenCommand, wxEXEC_SYNC) == -1)
				PanicAlert("wxExecute returned -1 on application run!");

		GameIni.Load(GameIniFile.c_str());
		LoadGameConfig();

		bRefreshList = true; // Just in case
	}
}

void CISOProperties::ListSelectionChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_PATCHES_LIST:
		if (Patches->GetSelection() != wxNOT_FOUND)
		{
			EditPatch->Enable();
			RemovePatch->Enable();
		}
		break;
	case ID_CHEATS_LIST:
		if (Cheats->GetSelection() != wxNOT_FOUND)
		{
			EditCheat->Enable();
			RemoveCheat->Enable();
		}
		break;
	}
}

void CISOProperties::PatchList_Load()
{
	onFrame.clear();
	Patches->Clear();
	PatchEngine::LoadPatchSection("OnFrame", onFrame, GameIni);

	u32 index = 0;
	for (std::vector<PatchEngine::Patch>::const_iterator it = onFrame.begin(); it != onFrame.end(); ++it)
	{
		PatchEngine::Patch p = *it;
		Patches->Append(wxString::FromAscii(p.name.c_str()));
		Patches->Check(index, p.active);
		++index;
	}
}

void CISOProperties::PatchList_Save()
{
	std::vector<std::string> lines;
	u32 index = 0;
	for (std::vector<PatchEngine::Patch>::const_iterator onFrame_it = onFrame.begin(); onFrame_it != onFrame.end(); ++onFrame_it)
	{
		lines.push_back(Patches->IsChecked(index) ? "+$" + onFrame_it->name : "$" + onFrame_it->name);

		for (std::vector<PatchEngine::PatchEntry>::const_iterator iter2 = onFrame_it->entries.begin(); iter2 != onFrame_it->entries.end(); ++iter2)
		{
			std::string temp;
			ToStringFromFormat(&temp, "0x%08X:%s:0x%08X", iter2->address, PatchEngine::PatchTypeStrings[iter2->type], iter2->value);
			lines.push_back(temp);
		}
		++index;
	}
	GameIni.SetLines("OnFrame", lines);
	lines.clear();
}

void CISOProperties::PatchButtonClicked(wxCommandEvent& event)
{
	int selection = Patches->GetSelection();
	
	switch (event.GetId())
	{
	case ID_EDITPATCH:
		{
		CPatchAddEdit dlg(selection, this);
		dlg.ShowModal();
		}
		break;
	case ID_ADDPATCH:
		{
		// dialog;
		}
		break;
	case ID_REMOVEPATCH:
		onFrame.erase(onFrame.begin() + Patches->GetSelection());
		break;
	}

	PatchList_Save();
	Patches->Clear();
	PatchList_Load();

	EditPatch->Enable(false);
	RemovePatch->Enable(false);
}

void CISOProperties::ActionReplayList_Load()
{
	arCodes.clear();
	Cheats->Clear();
	ActionReplay::LoadCodes(arCodes, GameIni);

	u32 index = 0;
	for (std::vector<ActionReplay::ARCode>::const_iterator it = arCodes.begin(); it != arCodes.end(); ++it)
	{
		ActionReplay::ARCode arCode = *it;
		Cheats->Append(wxString::FromAscii(arCode.name.c_str()));
		Cheats->Check(index, arCode.active);
		++index;
	}
}

void CISOProperties::ActionReplayList_Save()
{
	std::vector<std::string> lines;
	u32 index = 0;
	for (std::vector<ActionReplay::ARCode>::const_iterator iter = arCodes.begin(); iter != arCodes.end(); ++iter)
	{
		ActionReplay::ARCode code = *iter;

		lines.push_back(Cheats->IsChecked(index) ? "+$" + code.name : "$" + code.name);

		for (std::vector<ActionReplay::AREntry>::const_iterator iter2 = code.ops.begin(); iter2 != code.ops.end(); ++iter2)
		{
			lines.push_back(wxString::Format("%08X %08X", iter2->cmd_addr, iter2->value).mb_str());
		}
		++index;
	}
	GameIni.SetLines("ActionReplay", lines);
}

void CISOProperties::ActionReplayButtonClicked(wxCommandEvent& event)
{
	int selection = Cheats->GetSelection();
	
	switch (event.GetId())
	{
	case ID_EDITCHEAT:
		{
		CARCodeAddEdit dlg(selection, this);
		dlg.ShowModal();
		}
		break;
	case ID_ADDCHEAT:
		{
		// dialog
		}
		break;
	case ID_REMOVECHEAT:
		arCodes.erase(arCodes.begin() + Cheats->GetSelection());
		break;
	}

	ActionReplayList_Save();
	Cheats->Clear();
	ActionReplayList_Load();

	EditCheat->Enable(false);
	RemoveCheat->Enable(false);
}
