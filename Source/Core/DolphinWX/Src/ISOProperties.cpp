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

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#endif

#include "Common.h"
#include "CommonPaths.h"
#include "Globals.h"

#include "VolumeCreator.h"
#include "Filesystem.h"
#include "ISOProperties.h"
#include "PHackSettings.h"
#include "PatchAddEdit.h"
#include "ARCodeAddEdit.h"
#include "GeckoCodeDiag.h"
#include "ConfigManager.h"
#include "StringUtil.h"

#include "../resources/isoprop_file.xpm"
#include "../resources/isoprop_folder.xpm"
#include "../resources/isoprop_disc.xpm"

struct WiiPartition
{
	DiscIO::IVolume *Partition;
	DiscIO::IFileSystem *FileSystem;
	std::vector<const DiscIO::SFileInfo *> Files;
};
std::vector<WiiPartition> WiiDisc;

DiscIO::IVolume *OpenISO = NULL;
DiscIO::IFileSystem *pFileSystem = NULL;

std::vector<PatchEngine::Patch> onFrame;
std::vector<ActionReplay::ARCode> arCodes;
PHackData PHack_Data;


BEGIN_EVENT_TABLE(CISOProperties, wxDialog)
	EVT_CLOSE(CISOProperties::OnClose)
	EVT_BUTTON(wxID_OK, CISOProperties::OnCloseClick)
	EVT_BUTTON(ID_EDITCONFIG, CISOProperties::OnEditConfig)
	EVT_CHOICE(ID_EMUSTATE, CISOProperties::SetRefresh)
	EVT_CHOICE(ID_EMU_ISSUES, CISOProperties::SetRefresh)
	EVT_BUTTON(ID_PHSETTINGS, CISOProperties::PHackButtonClicked)
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
	EVT_MENU(IDM_EXTRACTALL, CISOProperties::OnExtractDir)
	EVT_MENU(IDM_EXTRACTAPPLOADER, CISOProperties::OnExtractDataFromHeader)
	EVT_MENU(IDM_EXTRACTDOL, CISOProperties::OnExtractDataFromHeader)
	EVT_MENU(IDM_CHECKINTEGRITY, CISOProperties::CheckPartitionIntegrity)
	EVT_CHOICE(ID_LANG, CISOProperties::OnChangeBannerLang)
END_EVENT_TABLE()

CISOProperties::CISOProperties(const std::string fileName, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	OpenISO = DiscIO::CreateVolumeFromFilename(fileName);
	if (DiscIO::IsVolumeWiiDisc(OpenISO))
	{
		for (u32 i = 0; i < 0xFFFFFFFF; i++) // yes, technically there can be OVER NINE THOUSAND partitions...
		{
			WiiPartition temp;
			if ((temp.Partition = DiscIO::CreateVolumeFromFilename(fileName, 0, i)) != NULL)
			{
				if ((temp.FileSystem = DiscIO::CreateFileSystem(temp.Partition)) != NULL)
				{
					temp.FileSystem->GetFileList(temp.Files);
					WiiDisc.push_back(temp);
				}
			}
			else
				break;
		}
	}
	else
	{
		// TODO : Should we add a way to browse the wad file ?
		if (!DiscIO::IsVolumeWadFile(OpenISO))
		{
			GCFiles.clear();
			pFileSystem = DiscIO::CreateFileSystem(OpenISO);
			if (pFileSystem)
				pFileSystem->GetFileList(GCFiles);
		}
	}

	OpenGameListItem = new GameListItem(fileName);

	bRefreshList = false;

	CreateGUIControls(DiscIO::IsVolumeWadFile(OpenISO));

	std::string _iniFilename = OpenISO->GetUniqueID();

	if (!_iniFilename.length())
	{
		char tmp[17];
		u8 _tTitleID[8];
		if(OpenISO->GetTitleID(_tTitleID))
		{
			snprintf(tmp, 17, "%016llx", Common::swap64(_tTitleID));
			_iniFilename = tmp;
		}
	}
	GameIniFile = File::GetUserPath(D_GAMECONFIG_IDX) + _iniFilename + ".ini";
	if (GameIni.Load(GameIniFile.c_str()))
		LoadGameConfig();
	else
	{
		// Will fail out if GameConfig folder doesn't exist
		std::ofstream f(GameIniFile.c_str());
		if (f)
		{
			f << "# " << OpenISO->GetUniqueID() << " - " << OpenISO->GetName() << '\n'
				<< "[Core] Values set here will override the main dolphin settings.\n"
				<< "[EmuState] The Emulation State. 1 is worst, 5 is best, 0 is not set.\n"
				<< "[OnFrame] Add memory patches to be applied every frame here.\n"
				<< "[ActionReplay] Add action replay cheats here.\n";
			f.close();
		}
		if (GameIni.Load(GameIniFile.c_str()))
			LoadGameConfig();
		else
			wxMessageBox(wxString::Format(_("Could not create %s"),
						wxString::From8BitData(GameIniFile.c_str()).c_str()),
					_("Error"), wxOK|wxICON_ERROR, this);
	}

	// Disk header and apploader

	std::wstring wname;
	wxString name;
	if (OpenGameListItem->GetName(wname))
		name = wname.c_str();
	else
		name = wxString(OpenISO->GetName().c_str(), wxConvUTF8);
	m_Name->SetValue(name);

	m_GameID->SetValue(wxString(OpenISO->GetUniqueID().c_str(), wxConvUTF8));
	switch (OpenISO->GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_EUROPE:
		m_Country->SetValue(_("EUROPE"));
		break;
	case DiscIO::IVolume::COUNTRY_FRANCE:
		m_Country->SetValue(_("FRANCE"));
		break;
	case DiscIO::IVolume::COUNTRY_ITALY:
		m_Country->SetValue(_("ITALY"));
		break;
	case DiscIO::IVolume::COUNTRY_RUSSIA:
		m_Country->SetValue(_("RUSSIA"));
		break;
	case DiscIO::IVolume::COUNTRY_USA:
		m_Country->SetValue(_("USA"));
		m_Lang->SetSelection(0);
		m_Lang->Disable(); // For NTSC Games, there's no multi lang
		break;
	case DiscIO::IVolume::COUNTRY_JAPAN:
		m_Country->SetValue(_("JAPAN"));
		m_Lang->SetSelection(-1);
		m_Lang->Disable(); // For NTSC Games, there's no multi lang
		break;
	case DiscIO::IVolume::COUNTRY_KOREA:
		m_Country->SetValue(_("KOREA"));
		break;
	case DiscIO::IVolume::COUNTRY_TAIWAN:
		m_Country->SetValue(_("TAIWAN"));
		m_Lang->SetSelection(-1);
		m_Lang->Disable(); // For NTSC Games, there's no multi lang
		break;
	case DiscIO::IVolume::COUNTRY_SDK:
		m_Country->SetValue(_("No Country (SDK)"));
		break;
	default:
		m_Country->SetValue(_("UNKNOWN"));
		break;
	}
	wxString temp = _T("0x") + wxString::From8BitData(OpenISO->GetMakerID().c_str());
	m_MakerID->SetValue(temp);
	m_Date->SetValue(wxString::From8BitData(OpenISO->GetApploaderDate().c_str()));
	m_FST->SetValue(wxString::Format(wxT("%u"), OpenISO->GetFSTSize()));

	// Here we set all the info to be shown (be it SJIS or Ascii) + we set the window title
	ChangeBannerDetails((int)SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage);
	m_Banner->SetBitmap(OpenGameListItem->GetImage());
	m_Banner->Connect(wxID_ANY, wxEVT_RIGHT_DOWN,
		wxMouseEventHandler(CISOProperties::RightClickOnBanner), (wxObject*)NULL, this);

	// Filesystem browser/dumper
	// TODO : Should we add a way to browse the wad file ?
	if (!DiscIO::IsVolumeWadFile(OpenISO))
	{
		if (DiscIO::IsVolumeWiiDisc(OpenISO))
		{
			for (u32 i = 0; i < WiiDisc.size(); i++)
			{
				WiiPartition partition = WiiDisc.at(i);
				wxTreeItemId PartitionRoot =
					m_Treectrl->AppendItem(RootId, wxString::Format(_("Partition %i"), i), 0, 0);
				CreateDirectoryTree(PartitionRoot, partition.Files, 1, partition.Files.at(0)->m_FileSize);	
				if (i == 1)
					m_Treectrl->Expand(PartitionRoot);
			}
		}
		else if (!GCFiles.empty())
			CreateDirectoryTree(RootId, GCFiles, 1, GCFiles.at(0)->m_FileSize);

		m_Treectrl->Expand(RootId);
	}
}

CISOProperties::~CISOProperties()
{
	if (!IsVolumeWiiDisc(OpenISO) && !IsVolumeWadFile(OpenISO) && pFileSystem)
		delete pFileSystem;
	// two vector's items are no longer valid after deleting filesystem
	WiiDisc.clear();
	GCFiles.clear();
	delete OpenGameListItem;
	delete OpenISO;
}

size_t CISOProperties::CreateDirectoryTree(wxTreeItemId& parent,
		std::vector<const DiscIO::SFileInfo*> fileInfos,
		const size_t _FirstIndex, 
		const size_t _LastIndex)
{
	size_t CurrentIndex = _FirstIndex;

	while (CurrentIndex < _LastIndex)
	{
		const DiscIO::SFileInfo *rFileInfo = fileInfos[CurrentIndex];
		char *name = (char*)rFileInfo->m_FullPath;

		if (rFileInfo->IsDirectory()) name[strlen(name) - 1] = '\0';
		char *itemName = strrchr(name, DIR_SEP_CHR);

		if(!itemName)
			itemName = name;
		else
			itemName++;

		// check next index
		if (rFileInfo->IsDirectory())
		{
			wxTreeItemId item = m_Treectrl->AppendItem(parent, wxString::From8BitData(itemName), 1, 1);
			CurrentIndex = CreateDirectoryTree(item, fileInfos, CurrentIndex + 1, (size_t)rFileInfo->m_FileSize);
		}
		else
		{
			m_Treectrl->AppendItem(parent, wxString::From8BitData(itemName), 2, 2);
			CurrentIndex++;
		}
	}

	return CurrentIndex;
}

void CISOProperties::CreateGUIControls(bool IsWad)
{
	wxButton * const EditConfig =
		new wxButton(this, ID_EDITCONFIG, _("Edit Config"), wxDefaultPosition, wxDefaultSize);
	EditConfig->SetToolTip(_("This will let you Manually Edit the INI config file"));

	// Notebook
	wxNotebook * const m_Notebook =
		new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize);
	wxPanel * const m_GameConfig =
		new wxPanel(m_Notebook, ID_GAMECONFIG, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_GameConfig, _("GameConfig"));
	wxPanel * const m_PatchPage =
		new wxPanel(m_Notebook, ID_PATCH_PAGE, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_PatchPage, _("Patches"));
	wxPanel * const m_CheatPage =
		new wxPanel(m_Notebook, ID_ARCODE_PAGE, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_CheatPage, _("AR Codes"));
	m_geckocode_panel = new Gecko::CodeConfigPanel(m_Notebook);
	m_Notebook->AddPage(m_geckocode_panel, _("Gecko Codes"));
	wxPanel * const m_Information =
		new wxPanel(m_Notebook, ID_INFORMATION, wxDefaultPosition, wxDefaultSize);
	m_Notebook->AddPage(m_Information, _("Info"));

	// GameConfig editing - Overrides and emulation state
	wxStaticText * const OverrideText = new wxStaticText(m_GameConfig, wxID_ANY, _("These settings override core Dolphin settings.\nUndetermined means the game uses Dolphin's setting."));
	// Core
	CPUThread = new wxCheckBox(m_GameConfig, ID_USEDUALCORE, _("Enable Dual Core"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	SkipIdle = new wxCheckBox(m_GameConfig, ID_IDLESKIP, _("Enable Idle Skipping"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	MMU = new wxCheckBox(m_GameConfig, ID_MMU, _("Enable MMU"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	MMU->SetToolTip(_("Enables the Memory Management Unit, needed for some games. (ON = Compatible, OFF = Fast)"));
	MMUBAT = new wxCheckBox(m_GameConfig, ID_MMUBAT, _("Enable BAT"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	MMUBAT->SetToolTip(_("Enables Block Address Translation (BAT); a function of the Memory Management Unit. Accurate to the hardware, but slow to emulate. (ON = Compatible, OFF = Fast)"));
	TLBHack = new wxCheckBox(m_GameConfig, ID_TLBHACK, _("MMU Speed Hack"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	TLBHack->SetToolTip(_("Fast version of the MMU.  Does not work for every game."));
	VBeam = new wxCheckBox(m_GameConfig, ID_VBEAM, _("Accurate VBeam emulation"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	VBeam->SetToolTip(_("If the FPS is erratic, this option may help. (ON = Compatible, OFF = Fast)"));
	FastDiscSpeed = new wxCheckBox(m_GameConfig, ID_DISCSPEED, _("Speed up Disc Transfer Rate"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	FastDiscSpeed->SetToolTip(_("Enable fast disc access.  Needed for a few games. (ON = Fast, OFF = Compatible)"));
	BlockMerging = new wxCheckBox(m_GameConfig, ID_MERGEBLOCKS, _("Enable Block Merging"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	DSPHLE = new wxCheckBox(m_GameConfig, ID_AUDIO_DSP_HLE, _("DSP HLE emulation (fast)"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);

	// Wii Console
	EnableProgressiveScan = new wxCheckBox(m_GameConfig, ID_ENABLEPROGRESSIVESCAN, _("Enable Progressive Scan"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	EnableWideScreen = new wxCheckBox(m_GameConfig, ID_ENABLEWIDESCREEN, _("Enable WideScreen"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	DisableWiimoteSpeaker = new wxCheckBox(m_GameConfig, ID_DISABLEWIIMOTESPEAKER, _("Alternate Wiimote Timing"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER, wxDefaultValidator);
	DisableWiimoteSpeaker->SetToolTip(_("Mutes the Wiimote speaker. Fixes random disconnections on real wiimotes. No effect on emulated wiimotes."));

	// Video
	UseBBox = new wxCheckBox(m_GameConfig, ID_USE_BBOX, _("Enable Bounding Box Calculation"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER);
	UseBBox->SetToolTip(_("If checked, the bounding box registers will be updated. Used by the Paper Mario games."));

	UseZTPSpeedupHack = new wxCheckBox(m_GameConfig, ID_ZTP_SPEEDUP, _("ZTP hack"), wxDefaultPosition, wxDefaultSize, wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER);
	UseZTPSpeedupHack->SetToolTip(_("Enable this to speed up The Legend of Zelda: Twilight Princess. Disable for ANY other game."));
	
	// Hack
	wxFlexGridSizer * const szrPHackSettings = new wxFlexGridSizer(0);
	PHackEnable = new wxCheckBox(m_GameConfig, ID_PHACKENABLE, _("Custom Projection Hack"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE);
	PHackEnable->SetToolTip(_("Enables Custom Projection Hack"));
	PHSettings = new wxButton(m_GameConfig, ID_PHSETTINGS, _("Settings..."));
	PHSettings->SetToolTip(_("Customize some Orthographic Projection parameters."));

	wxBoxSizer * const sEmuState = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText * const EmuStateText =
		new wxStaticText(m_GameConfig, wxID_ANY, _("Emulation State: "));
	arrayStringFor_EmuState.Add(_("Not Set"));
	arrayStringFor_EmuState.Add(_("Broken"));
	arrayStringFor_EmuState.Add(_("Intro"));
	arrayStringFor_EmuState.Add(_("In Game"));
	arrayStringFor_EmuState.Add(_("Playable"));
	arrayStringFor_EmuState.Add(_("Perfect"));
	EmuState = new wxChoice(m_GameConfig, ID_EMUSTATE,
			wxDefaultPosition, wxDefaultSize, arrayStringFor_EmuState);
	EmuIssues = new wxTextCtrl(m_GameConfig, ID_EMU_ISSUES, wxEmptyString);

	wxBoxSizer * const sConfigPage = new wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer * const sbCoreOverrides =
		new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Core"));
	sbCoreOverrides->Add(CPUThread, 0, wxLEFT, 5);
	sbCoreOverrides->Add(SkipIdle, 0, wxLEFT, 5);
	sbCoreOverrides->Add(MMU, 0, wxLEFT, 5);
	sbCoreOverrides->Add(MMUBAT, 0, wxLEFT, 5);
	sbCoreOverrides->Add(TLBHack, 0, wxLEFT, 5);
	sbCoreOverrides->Add(VBeam, 0, wxLEFT, 5);
	sbCoreOverrides->Add(FastDiscSpeed, 0, wxLEFT, 5);	
	sbCoreOverrides->Add(BlockMerging, 0, wxLEFT, 5);
	sbCoreOverrides->Add(DSPHLE, 0, wxLEFT, 5);

	wxStaticBoxSizer * const sbWiiOverrides =
		new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Wii Console"));
	if (!DiscIO::IsVolumeWiiDisc(OpenISO) && !DiscIO::IsVolumeWadFile(OpenISO))
	{
		sbWiiOverrides->ShowItems(false);
		EnableProgressiveScan->Hide();
		EnableWideScreen->Hide();
		DisableWiimoteSpeaker->Hide();
	}
	else
	{
		// Progressive Scan is not used by Dolphin itself, and changing it on a per-game
		// basis would have the side-effect of changing the SysConf, making this setting
		// rather useless.
		EnableProgressiveScan->Disable();
	}
	sbWiiOverrides->Add(EnableProgressiveScan, 0, wxLEFT, 5);
	sbWiiOverrides->Add(EnableWideScreen, 0, wxLEFT, 5);
	sbWiiOverrides->Add(DisableWiimoteSpeaker, 0, wxLEFT, 5);

	wxStaticBoxSizer * const sbVideoOverrides =
		new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Video"));
	sbVideoOverrides->Add(UseBBox, 0, wxLEFT, 5);
	sbVideoOverrides->Add(UseZTPSpeedupHack, 0, wxLEFT, 5);
	szrPHackSettings->Add(PHackEnable, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 5);
	szrPHackSettings->Add(PHSettings, 0, wxLEFT, 5);

	sbVideoOverrides->Add(szrPHackSettings, 0, wxEXPAND);
	wxStaticBoxSizer * const sbGameConfig =
		new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Game-Specific Settings"));
	sbGameConfig->Add(OverrideText, 0, wxEXPAND|wxALL, 5);
	sbGameConfig->Add(sbCoreOverrides, 0, wxEXPAND);
	sbGameConfig->Add(sbWiiOverrides, 0, wxEXPAND);
	sbGameConfig->Add(sbVideoOverrides, 0, wxEXPAND);
	sConfigPage->Add(sbGameConfig, 0, wxEXPAND|wxALL, 5);
	sEmuState->Add(EmuStateText, 0, wxALIGN_CENTER_VERTICAL);
	sEmuState->Add(EmuState, 0, wxEXPAND);
	sEmuState->Add(EmuIssues, 1, wxEXPAND);
	sConfigPage->Add(sEmuState, 0, wxEXPAND|wxALL, 5);
	m_GameConfig->SetSizer(sConfigPage);

	
	// Patches
	wxBoxSizer * const sPatches = new wxBoxSizer(wxVERTICAL);
	Patches = new wxCheckListBox(m_PatchPage, ID_PATCHES_LIST, wxDefaultPosition,
			wxDefaultSize, arrayStringFor_Patches, wxLB_HSCROLL);
	wxBoxSizer * const sPatchButtons = new wxBoxSizer(wxHORIZONTAL);
	EditPatch = new wxButton(m_PatchPage, ID_EDITPATCH, _("Edit..."));
	wxButton * const AddPatch = new wxButton(m_PatchPage, ID_ADDPATCH, _("Add..."));
	RemovePatch = new wxButton(m_PatchPage, ID_REMOVEPATCH, _("Remove"));
	EditPatch->Enable(false);
	RemovePatch->Enable(false);

	wxBoxSizer* sPatchPage = new wxBoxSizer(wxVERTICAL);
	sPatches->Add(Patches, 1, wxEXPAND|wxALL, 0);
	sPatchButtons->Add(EditPatch,  0, wxEXPAND|wxALL, 0);
	sPatchButtons->AddStretchSpacer();
	sPatchButtons->Add(AddPatch,  0, wxEXPAND|wxALL, 0);
	sPatchButtons->Add(RemovePatch,  0, wxEXPAND|wxALL, 0);
	sPatches->Add(sPatchButtons, 0, wxEXPAND|wxALL, 0);
	sPatchPage->Add(sPatches, 1, wxEXPAND|wxALL, 5);
	m_PatchPage->SetSizer(sPatchPage);

	
	// Action Replay Cheats
	wxBoxSizer * const sCheats = new wxBoxSizer(wxVERTICAL);
	Cheats = new wxCheckListBox(m_CheatPage, ID_CHEATS_LIST, wxDefaultPosition,
			wxDefaultSize, arrayStringFor_Cheats, wxLB_HSCROLL);
	wxBoxSizer * const sCheatButtons = new wxBoxSizer(wxHORIZONTAL);
	EditCheat = new wxButton(m_CheatPage, ID_EDITCHEAT, _("Edit..."));
	wxButton * const AddCheat = new wxButton(m_CheatPage, ID_ADDCHEAT, _("Add..."));
	RemoveCheat = new wxButton(m_CheatPage, ID_REMOVECHEAT, _("Remove"),
			wxDefaultPosition, wxDefaultSize, 0);
	EditCheat->Enable(false);
	RemoveCheat->Enable(false);

	wxBoxSizer* sCheatPage = new wxBoxSizer(wxVERTICAL);
	sCheats->Add(Cheats, 1, wxEXPAND|wxALL, 0);
	sCheatButtons->Add(EditCheat,  0, wxEXPAND|wxALL, 0);
	sCheatButtons->AddStretchSpacer();
	sCheatButtons->Add(AddCheat,  0, wxEXPAND|wxALL, 0);
	sCheatButtons->Add(RemoveCheat,  0, wxEXPAND|wxALL, 0);
	sCheats->Add(sCheatButtons, 0, wxEXPAND|wxALL, 0);
	sCheatPage->Add(sCheats, 1, wxEXPAND|wxALL, 5);
	m_CheatPage->SetSizer(sCheatPage);

	
	wxStaticText * const m_NameText =
		new wxStaticText(m_Information, wxID_ANY, _("Name:"));
	m_Name = new wxTextCtrl(m_Information, ID_NAME, wxEmptyString,
			wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText * const m_GameIDText =
		new wxStaticText(m_Information, wxID_ANY, _("Game ID:"));
	m_GameID = new wxTextCtrl(m_Information, ID_GAMEID, wxEmptyString,
			wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText * const m_CountryText =
		new wxStaticText(m_Information, wxID_ANY, _("Country:"));
	m_Country = new wxTextCtrl(m_Information, ID_COUNTRY, wxEmptyString,
			wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText * const m_MakerIDText =
		new wxStaticText(m_Information, wxID_ANY, _("Maker ID:"));
	m_MakerID = new wxTextCtrl(m_Information, ID_MAKERID, wxEmptyString,
			wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText * const m_DateText =
		new wxStaticText(m_Information, wxID_ANY, _("Date:"));
	m_Date = new wxTextCtrl(m_Information, ID_DATE, wxEmptyString,
			wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText * const m_FSTText =
		new wxStaticText(m_Information, wxID_ANY, _("FST Size:"));	
	m_FST = new wxTextCtrl(m_Information, ID_FST, wxEmptyString,
			wxDefaultPosition, wxDefaultSize, wxTE_READONLY);

	wxStaticText * const m_LangText = new wxStaticText(m_Information, wxID_ANY, _("Show Language:"));
	arrayStringFor_Lang.Add(_("English"));
	arrayStringFor_Lang.Add(_("German"));
	arrayStringFor_Lang.Add(_("French"));
	arrayStringFor_Lang.Add(_("Spanish"));
	arrayStringFor_Lang.Add(_("Italian"));
	arrayStringFor_Lang.Add(_("Dutch"));
	m_Lang = new wxChoice(m_Information, ID_LANG, wxDefaultPosition, wxDefaultSize, arrayStringFor_Lang);
	m_Lang->SetSelection((int)SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage);
	wxStaticText * const m_ShortText = new wxStaticText(m_Information, wxID_ANY, _("Short Name:"));
	m_ShortName = new wxTextCtrl(m_Information, ID_SHORTNAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText * const m_MakerText = new wxStaticText(m_Information, wxID_ANY, _("Maker:"));
	m_Maker = new wxTextCtrl(m_Information, ID_MAKER, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText * const m_CommentText = new wxStaticText(m_Information, wxID_ANY, _("Comment:"));
	m_Comment = new wxTextCtrl(m_Information, ID_COMMENT, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY);
	wxStaticText * const m_BannerText = new wxStaticText(m_Information, wxID_ANY, _("Banner:"));
	m_Banner = new wxStaticBitmap(m_Information, ID_BANNER, wxNullBitmap, wxDefaultPosition, wxSize(96, 32), 0);

	// ISO Details
	wxGridBagSizer * const sISODetails = new wxGridBagSizer(0, 0);
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
	sISODetails->AddGrowableCol(1);
	wxStaticBoxSizer * const sbISODetails =
		new wxStaticBoxSizer(wxVERTICAL, m_Information, _("ISO Details"));
	sbISODetails->Add(sISODetails, 0, wxEXPAND, 5);

	// Banner Details
	wxGridBagSizer * const sBannerDetails = new wxGridBagSizer(0, 0);
	sBannerDetails->Add(m_LangText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_Lang, wxGBPosition(0, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_ShortText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_ShortName, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_MakerText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_Maker, wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_CommentText, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALL, 5);
	sBannerDetails->Add(m_Comment, wxGBPosition(3, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sBannerDetails->Add(m_BannerText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALL, 5);
	sBannerDetails->Add(m_Banner, wxGBPosition(4, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sBannerDetails->AddGrowableCol(1);
	wxStaticBoxSizer * const sbBannerDetails =
		new wxStaticBoxSizer(wxVERTICAL, m_Information, _("Banner Details"));
	sbBannerDetails->Add(sBannerDetails, 0, wxEXPAND, 5);

	wxBoxSizer * const sInfoPage = new wxBoxSizer(wxVERTICAL);
	sInfoPage->Add(sbISODetails, 0, wxEXPAND|wxALL, 5);
	sInfoPage->Add(sbBannerDetails, 0, wxEXPAND|wxALL, 5);
	m_Information->SetSizer(sInfoPage);

	if (!IsWad)
	{
		wxPanel * const m_Filesystem =
			new wxPanel(m_Notebook, ID_FILESYSTEM, wxDefaultPosition, wxDefaultSize);
		m_Notebook->AddPage(m_Filesystem, _("Filesystem"));

		// Filesystem icons
		wxImageList * const m_iconList = new wxImageList(16, 16);
		m_iconList->Add(wxBitmap(disc_xpm), wxNullBitmap);	// 0
		m_iconList->Add(wxBitmap(folder_xpm), wxNullBitmap);	// 1
		m_iconList->Add(wxBitmap(file_xpm), wxNullBitmap);	// 2

		// Filesystem tree
		m_Treectrl = new wxTreeCtrl(m_Filesystem, ID_TREECTRL,
				wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE);
		m_Treectrl->AssignImageList(m_iconList);
		RootId = m_Treectrl->AddRoot(_("Disc"), 0, 0, 0);

		wxBoxSizer* sTreePage = new wxBoxSizer(wxVERTICAL);
		sTreePage->Add(m_Treectrl, 1, wxEXPAND|wxALL, 5);
		m_Filesystem->SetSizer(sTreePage);
	}

	wxSizer* sButtons = CreateButtonSizer(wxNO_DEFAULT);
	sButtons->Prepend(EditConfig);
	sButtons->Add(new wxButton(this, wxID_OK, _("Close")));

	// Add notebook and buttons to the dialog
	wxBoxSizer* sMain;
	sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(m_Notebook, 1, wxEXPAND|wxALL, 5);
	sMain->Add(sButtons, 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5);
	sMain->SetMinSize(wxSize(500, -1));

	SetSizerAndFit(sMain);
	Center();
	SetFocus();
}

void CISOProperties::OnClose(wxCloseEvent& WXUNUSED (event))
{
	if (!SaveGameConfig())
		PanicAlertT("Could not save %s", GameIniFile.c_str());

	EndModal(bRefreshList ? wxID_OK : wxID_CANCEL);
}

void CISOProperties::OnCloseClick(wxCommandEvent& WXUNUSED (event))
{
	Close();
}

void CISOProperties::RightClickOnBanner(wxMouseEvent& event)
{
	wxMenu* popupMenu = new wxMenu;
	popupMenu->Append(IDM_BNRSAVEAS, _("Save as..."));
	PopupMenu(popupMenu);

	event.Skip();
}

void CISOProperties::OnBannerImageSave(wxCommandEvent& WXUNUSED (event))
{
	wxString dirHome;

	wxFileDialog dialog(this, _("Save as..."), wxGetHomeDir(&dirHome), wxString::Format(wxT("%s.png"), m_GameID->GetLabel().c_str()),
		wxALL_FILES_PATTERN, wxFD_SAVE|wxFD_OVERWRITE_PROMPT, wxDefaultPosition, wxDefaultSize);
	if (dialog.ShowModal() == wxID_OK)
	{
		m_Banner->GetBitmap().ConvertToImage().SaveFile(dialog.GetPath());
	}
}

void CISOProperties::OnRightClickOnTree(wxTreeEvent& event)
{
	m_Treectrl->SelectItem(event.GetItem());

	wxMenu* popupMenu = new wxMenu;

	if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 0
		&& m_Treectrl->GetFirstVisibleItem() != m_Treectrl->GetSelection())
		popupMenu->Append(IDM_EXTRACTDIR, _("Extract Partition..."));
	else if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 1)
		popupMenu->Append(IDM_EXTRACTDIR, _("Extract Directory..."));
	else if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 2)
		popupMenu->Append(IDM_EXTRACTFILE, _("Extract File..."));

	popupMenu->Append(IDM_EXTRACTALL, _("Extract All Files..."));
	popupMenu->AppendSeparator();
	popupMenu->Append(IDM_EXTRACTAPPLOADER, _("Extract Apploader..."));
	popupMenu->Append(IDM_EXTRACTDOL, _("Extract DOL..."));

	if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 0
		&& m_Treectrl->GetFirstVisibleItem() != m_Treectrl->GetSelection())
	{
		popupMenu->AppendSeparator();
		popupMenu->Append(IDM_CHECKINTEGRITY, _("Check Partition Integrity"));
	}

	PopupMenu(popupMenu);

	event.Skip();
}

void CISOProperties::OnExtractFile(wxCommandEvent& WXUNUSED (event))
{
	wxString Path;
	wxString File;

	File = m_Treectrl->GetItemText(m_Treectrl->GetSelection());
	
	Path = wxFileSelector(
		_("Export File"),
		wxEmptyString, File, wxEmptyString,
		wxGetTranslation(wxALL_FILES),
		wxFD_SAVE,
		this);

	if (!Path || !File)
		return;

	while (m_Treectrl->GetItemParent(m_Treectrl->GetSelection()) != m_Treectrl->GetRootItem())
	{
		wxString temp;
		temp = m_Treectrl->GetItemText(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
		File = temp + wxT(DIR_SEP_CHR) + File;

		m_Treectrl->SelectItem(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
	}

	if (DiscIO::IsVolumeWiiDisc(OpenISO))
	{
		int partitionNum = wxAtoi(File.SubString(10, 11));
		File.Remove(0, 12); // Remove "Partition x/"
		WiiDisc.at(partitionNum).FileSystem->ExportFile(File.mb_str(), Path.mb_str());
	}
	else
		pFileSystem->ExportFile(File.mb_str(), Path.mb_str());
}

void CISOProperties::ExportDir(const char* _rFullPath, const char* _rExportFolder, const int partitionNum)
{
	char exportName[512];
	u32 index[2] = {0, 0};
	std::vector<const DiscIO::SFileInfo *> fst;
	DiscIO::IFileSystem *FS = 0;

	if (DiscIO::IsVolumeWiiDisc(OpenISO))
	{
		FS = WiiDisc.at(partitionNum).FileSystem;
	}
	else
		FS = pFileSystem;

	FS->GetFileList(fst);

	if (!_rFullPath) // Extract all
	{
		index[0] = 0;
		index[1] = (u32)fst.size();

		FS->ExportApploader(_rExportFolder);
		if (!DiscIO::IsVolumeWiiDisc(OpenISO))
			FS->ExportDOL(_rExportFolder);
	}
	else // Look for the dir we are going to extract
	{
		for(index[0] = 0; index[0] < fst.size(); index[0]++)
		{
			if (!strcmp(fst.at(index[0])->m_FullPath, _rFullPath))
			{
				DEBUG_LOG(DISCIO, "Found the Dir at %u", index[0]);
				index[1] = (u32)fst.at(index[0])->m_FileSize;
				break;
			}
		}

		DEBUG_LOG(DISCIO,"Dir found from %u to %u\nextracting to:\n%s",index[0],index[1],_rExportFolder);
	}

	wxString dialogTitle = index[0] ? _("Extracting Directory") : _("Extracting All Files");
	wxProgressDialog dialog(
		dialogTitle,
		_("Extracting..."),
		index[1] - 1,
		this,
		wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT |
		wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME |
		wxPD_SMOOTH
		);

	// Extraction
	for (u32 i = index[0]; i < index[1]; i++)
	{
		dialog.SetTitle(wxString::Format(wxT("%s : %d%%"), dialogTitle.c_str(),
			(u32)(((float)(i - index[0]) / (float)(index[1] - index[0])) * 100)));
		
		dialog.Update(i, wxString::Format(_("Extracting %s"),
			wxString(fst[i]->m_FullPath, *wxConvCurrent).c_str()));

		if (dialog.WasCancelled())
			break;

		if (fst[i]->IsDirectory())
		{
			snprintf(exportName, sizeof(exportName), "%s/%s/", _rExportFolder, fst[i]->m_FullPath);
			DEBUG_LOG(DISCIO, "%s", exportName);		

			if (!File::Exists(exportName) && !File::CreateFullPath(exportName))
			{
				ERROR_LOG(DISCIO, "Could not create the path %s", exportName);
			}
			else
			{
				if (!File::IsDirectory(exportName))
					ERROR_LOG(DISCIO, "%s already exists and is not a directory", exportName);

				DEBUG_LOG(DISCIO, "folder %s already exists", exportName);
			}
		}
		else
		{
			snprintf(exportName, sizeof(exportName), "%s/%s", _rExportFolder, fst[i]->m_FullPath);
			DEBUG_LOG(DISCIO, "%s", exportName);

			if (!File::Exists(exportName) && !FS->ExportFile(fst[i]->m_FullPath, exportName))
			{
				ERROR_LOG(DISCIO, "Could not export %s", exportName);
			}
			else
			{
				DEBUG_LOG(DISCIO, "%s already exists", exportName);
			}
		}
	}
}

void CISOProperties::OnExtractDir(wxCommandEvent& event)
{
	wxString Directory = m_Treectrl->GetItemText(m_Treectrl->GetSelection());
	wxString Path = wxDirSelector(_("Choose the folder to extract to"));

	if (!Path || !Directory)
		return;

	if (event.GetId() == IDM_EXTRACTALL)
	{
		if (DiscIO::IsVolumeWiiDisc(OpenISO))
			for (u32 i = 0; i < WiiDisc.size(); i++)
				ExportDir(NULL, Path.mb_str(), i);
		else
			ExportDir(NULL, Path.mb_str());

		return;
	}

	while (m_Treectrl->GetItemParent(m_Treectrl->GetSelection()) != m_Treectrl->GetRootItem())
	{
		wxString temp;
		temp = m_Treectrl->GetItemText(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
		Directory = temp + wxT(DIR_SEP_CHR) + Directory;

		m_Treectrl->SelectItem(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
	}

	if (DiscIO::IsVolumeWiiDisc(OpenISO))
	{
		int partitionNum = wxAtoi(Directory.SubString(10, 11));
		Directory.Remove(0, 12); // Remove "Partition x/"
		ExportDir(Directory.mb_str(), Path.mb_str(), partitionNum);
	}
	else
		ExportDir(Directory.mb_str(), Path.mb_str());
}

void CISOProperties::OnExtractDataFromHeader(wxCommandEvent& event)
{
	std::vector<const DiscIO::SFileInfo *> fst;
	DiscIO::IFileSystem *FS = 0;
	wxString Path = wxDirSelector(_("Choose the folder to extract to"));

	if (Path.empty())
		return;

	if (DiscIO::IsVolumeWiiDisc(OpenISO))
		FS = WiiDisc.at(1).FileSystem;
	else
		FS = pFileSystem;

	bool ret = false;
	if (event.GetId() == IDM_EXTRACTAPPLOADER)
	{
		ret = FS->ExportApploader(Path.mb_str());
	}
	else if (event.GetId() == IDM_EXTRACTDOL)
	{
		ret = FS->ExportDOL(Path.mb_str());
	}

	if (!ret)
		PanicAlertT("Failed to extract to %s!", (const char *)Path.mb_str());
}

class IntegrityCheckThread : public wxThread
{
public:
	IntegrityCheckThread(const WiiPartition& Partition)
		: wxThread(wxTHREAD_JOINABLE), m_Partition(Partition)
	{
		Create();
	}

	virtual ExitCode Entry()
	{
		return (ExitCode)m_Partition.Partition->CheckIntegrity();
	}

private:
	const WiiPartition& m_Partition;
};

void CISOProperties::CheckPartitionIntegrity(wxCommandEvent& event)
{
	// Normally we can't enter this function if we aren't analyzing a Wii disc
	// anyway, but let's still check to be sure.
	if (!DiscIO::IsVolumeWiiDisc(OpenISO))
		return;

	wxString PartitionName = m_Treectrl->GetItemText(m_Treectrl->GetSelection());
	if (!PartitionName)
		return;

	// Get the partition number from the item text ("Partition N")
	int PartitionNum = wxAtoi(PartitionName.SubString(10, 11));
	const WiiPartition& Partition = WiiDisc[PartitionNum];

	wxProgressDialog* dialog = new wxProgressDialog(
		_("Checking integrity..."), _("Working..."), 1000, this,
		wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_SMOOTH
	);

	IntegrityCheckThread thread(Partition);
	thread.Run();

	while (thread.IsAlive())
	{
		dialog->Pulse();
		wxThread::Sleep(50);
	}

	delete dialog;

	if (!thread.Wait())
	{
		wxMessageBox(
			wxString::Format(_("Integrity check for partition %d failed. "
							   "Your dump is most likely corrupted or has been "
							   "patched incorrectly."), PartitionNum),
			_("Integrity Check Error"), wxOK | wxICON_ERROR, this
		);
	}
	else
	{
		wxMessageBox(_("Integrity check completed. No errors have been found."),
					 _("Integrity check completed"), wxOK | wxICON_INFORMATION, this);
	}
}

void CISOProperties::SetRefresh(wxCommandEvent& event)
{
	bRefreshList = true;

	if (event.GetId() == ID_EMUSTATE)
		EmuIssues->Enable(event.GetSelection() != 0);
}

void CISOProperties::LoadGameConfig()
{
	bool bTemp;
	int iTemp;
	std::string sTemp;

	if (GameIni.Get("Core", "CPUThread", &bTemp))
		CPUThread->Set3StateValue((wxCheckBoxState)bTemp);
	else
		CPUThread->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "SkipIdle", &bTemp))
		SkipIdle->Set3StateValue((wxCheckBoxState)bTemp);
	else
		SkipIdle->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "MMU", &bTemp))
		MMU->Set3StateValue((wxCheckBoxState)bTemp);
	else
		MMU->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "BAT", &bTemp))
		MMUBAT->Set3StateValue((wxCheckBoxState)bTemp);
	else
		MMUBAT->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "TLBHack", &bTemp))
		TLBHack->Set3StateValue((wxCheckBoxState)bTemp);
	else
		TLBHack->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "VBeam", &bTemp))
		VBeam->Set3StateValue((wxCheckBoxState)bTemp);
	else
		VBeam->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "FastDiscSpeed", &bTemp))
		FastDiscSpeed->Set3StateValue((wxCheckBoxState)bTemp);
	else
		FastDiscSpeed->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "BlockMerging", &bTemp))
		BlockMerging->Set3StateValue((wxCheckBoxState)bTemp);
	else
		BlockMerging->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Core", "DSPHLE", &bTemp))
		DSPHLE->Set3StateValue((wxCheckBoxState)bTemp);
	else
		DSPHLE->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Display", "ProgressiveScan", &bTemp))
		EnableProgressiveScan->Set3StateValue((wxCheckBoxState)bTemp);
	else
		EnableProgressiveScan->Set3StateValue(wxCHK_UNDETERMINED);

	// ??
	if (GameIni.Get("Wii", "Widescreen", &bTemp))
		EnableWideScreen->Set3StateValue((wxCheckBoxState)bTemp);
	else
		EnableWideScreen->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Wii", "DisableWiimoteSpeaker", &bTemp))
		DisableWiimoteSpeaker->Set3StateValue((wxCheckBoxState)bTemp);
	else
		DisableWiimoteSpeaker->Set3StateValue(wxCHK_UNDETERMINED);

	if (GameIni.Get("Video", "UseBBox", &bTemp))
		UseBBox->Set3StateValue((wxCheckBoxState)bTemp);
	else
		UseBBox->Set3StateValue(wxCHK_UNDETERMINED);


	if (GameIni.Get("Video", "ZTPSpeedupHack", &bTemp))
		UseZTPSpeedupHack->Set3StateValue((wxCheckBoxState)bTemp);
	else
		UseZTPSpeedupHack->Set3StateValue(wxCHK_UNDETERMINED);

	GameIni.Get("Video", "ProjectionHack", &bTemp);
	PHackEnable->Set3StateValue((wxCheckBoxState)bTemp);
	
	GameIni.Get("Video", "PH_SZNear", &PHack_Data.PHackSZNear);
	GameIni.Get("Video", "PH_SZFar", &PHack_Data.PHackSZFar);
	GameIni.Get("Video", "PH_ExtraParam", &PHack_Data.PHackExP);

	GameIni.Get("Video", "PH_ZNear", &PHack_Data.PHZNear);
	GameIni.Get("Video", "PH_ZFar", &PHack_Data.PHZFar);

	GameIni.Get("EmuState", "EmulationStateId", &iTemp, 0/*Not Set*/);
	EmuState->SetSelection(iTemp);

	GameIni.Get("EmuState", "EmulationIssues", &sTemp);
	if (!sTemp.empty())
	{
		EmuIssues->SetValue(wxString(sTemp.c_str(), *wxConvCurrent));
		bRefreshList = true;
	}
	EmuIssues->Enable(EmuState->GetSelection() != 0);

	PatchList_Load();
	ActionReplayList_Load();
	m_geckocode_panel->LoadCodes(GameIni, OpenISO->GetUniqueID());
}

bool CISOProperties::SaveGameConfig()
{
	if (CPUThread->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "CPUThread");
	else
		GameIni.Set("Core", "CPUThread", CPUThread->Get3StateValue());

	if (SkipIdle->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "SkipIdle");
	else
		GameIni.Set("Core", "SkipIdle", SkipIdle->Get3StateValue());

	if (MMU->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "MMU");
	else
		GameIni.Set("Core", "MMU", MMU->Get3StateValue());

	if (MMUBAT->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "BAT");
	else
		GameIni.Set("Core", "BAT", MMUBAT->Get3StateValue());

	if (TLBHack->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "TLBHack");
	else
		GameIni.Set("Core", "TLBHack", TLBHack->Get3StateValue());

	if (VBeam->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "VBeam");
	else
		GameIni.Set("Core", "VBeam", VBeam->Get3StateValue());

	if (FastDiscSpeed->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "FastDiscSpeed");
	else
		GameIni.Set("Core", "FastDiscSpeed", FastDiscSpeed->Get3StateValue());

	if (BlockMerging->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "BlockMerging");
	else
		GameIni.Set("Core", "BlockMerging", BlockMerging->Get3StateValue());

	if (DSPHLE->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Core", "DSPHLE");
	else
		GameIni.Set("Core", "DSPHLE", DSPHLE->Get3StateValue());

	if (EnableProgressiveScan->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Display", "ProgressiveScan");
	else
		GameIni.Set("Display", "ProgressiveScan", EnableProgressiveScan->Get3StateValue());

	if (EnableWideScreen->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Wii", "Widescreen");
	else
		GameIni.Set("Wii", "Widescreen", EnableWideScreen->Get3StateValue());

	if (DisableWiimoteSpeaker->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Wii", "DisableWiimoteSpeaker");
	else
		GameIni.Set("Wii", "DisableWiimoteSpeaker", DisableWiimoteSpeaker->Get3StateValue());

	if (UseBBox->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Video", "UseBBox");
	else
		GameIni.Set("Video", "UseBBox", UseBBox->Get3StateValue());

	if (UseZTPSpeedupHack->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIni.DeleteKey("Video", "ZTPSpeedupHack");
	else
		GameIni.Set("Video", "ZTPSpeedupHack", UseZTPSpeedupHack->Get3StateValue());

	GameIni.Set("Video", "ProjectionHack", PHackEnable->Get3StateValue());

	GameIni.Set("Video", "PH_SZNear", PHack_Data.PHackSZNear ? 1 : 0);
	GameIni.Set("Video", "PH_SZFar", PHack_Data.PHackSZFar ? 1 : 0);
	GameIni.Set("Video", "PH_ExtraParam", PHack_Data.PHackExP ? 1 : 0);

	GameIni.Set("Video", "PH_ZNear", PHack_Data.PHZNear);
	GameIni.Set("Video", "PH_ZFar", PHack_Data.PHZFar);

	GameIni.Set("EmuState", "EmulationStateId", EmuState->GetSelection());
	GameIni.Set("EmuState", "EmulationIssues", (const char*)EmuIssues->GetValue().mb_str(*wxConvCurrent));

	PatchList_Save();
	ActionReplayList_Save();
	Gecko::SaveCodes(GameIni, m_geckocode_panel->GetCodes());

	return GameIni.Save(GameIniFile.c_str());
}

void CISOProperties::OnEditConfig(wxCommandEvent& WXUNUSED (event))
{
	if (wxFileExists(wxString::From8BitData(GameIniFile.c_str())))
	{
		SaveGameConfig();

#ifdef __APPLE__
		// wxTheMimeTypesManager is not yet implemented for wxCocoa
		[[NSWorkspace sharedWorkspace] openFile:
			[NSString stringWithUTF8String: GameIniFile.c_str()]
			withApplication: @"TextEdit"];
#else
		wxFileType* filetype = wxTheMimeTypesManager->GetFileTypeFromExtension(_T("ini"));
		if(filetype == NULL) // From extension failed, trying with MIME type now
		{
			filetype = wxTheMimeTypesManager->GetFileTypeFromMimeType(_T("text/plain"));
			if(filetype == NULL) // MIME type failed, aborting mission
			{
				PanicAlertT("Filetype 'ini' is unknown! Will not open!");
				return;
			}
		}
		wxString OpenCommand;
		OpenCommand = filetype->GetOpenCommand(wxString::From8BitData(GameIniFile.c_str()));
		if(OpenCommand.IsEmpty())
			PanicAlertT("Couldn't find open command for extension 'ini'!");
		else
			if(wxExecute(OpenCommand, wxEXEC_SYNC) == -1)
				PanicAlertT("wxExecute returned -1 on application run!");
#endif

		GameIni.Load(GameIniFile.c_str());
		LoadGameConfig();

		bRefreshList = true; // Just in case
	}

	// Once we're done with the ini edit, give the focus back to Dolphin 
	SetFocus();
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
		Patches->Append(wxString(p.name.c_str(), *wxConvCurrent));
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
			std::string temp = StringFromFormat("0x%08X:%s:0x%08X", iter2->address, PatchEngine::PatchTypeStrings[iter2->type], iter2->value);			
			lines.push_back(temp);
		}
		++index;
	}
	GameIni.SetLines("OnFrame", lines);
	lines.clear();
}

void CISOProperties::PHackButtonClicked(wxCommandEvent& event)
{
	if (event.GetId() == ID_PHSETTINGS)
	{
		::PHack_Data = PHack_Data;
		CPHackSettings dlg(this, 1);
		if (dlg.ShowModal() == wxID_OK)
			PHack_Data = ::PHack_Data;
	}
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
		CPatchAddEdit dlg(-1, this, 1, _("Add Patch"));
		if (dlg.ShowModal() == wxID_OK)
		{
			Patches->Append(wxString(onFrame.back().name.c_str(), *wxConvCurrent));
			Patches->Check((unsigned int)(onFrame.size() - 1), onFrame.back().active);
		}
		}
		break;
	case ID_REMOVEPATCH:
		onFrame.erase(onFrame.begin() + Patches->GetSelection());
		Patches->Delete(Cheats->GetSelection());
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
		Cheats->Append(wxString(arCode.name.c_str(), *wxConvCurrent));
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
			lines.push_back(std::string(wxString::Format(wxT("%08X %08X"), iter2->cmd_addr, iter2->value).mb_str()));
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
			CARCodeAddEdit dlg(-1, this, 1, _("Add ActionReplay Code"));
			if (dlg.ShowModal() == wxID_OK)
			{
				Cheats->Append(wxString::From8BitData(arCodes.back().name.c_str()));
				Cheats->Check((unsigned int)(arCodes.size() - 1), arCodes.back().active);
			}
		}
		break;
	case ID_REMOVECHEAT:
		arCodes.erase(arCodes.begin() + Cheats->GetSelection());
		Cheats->Delete(Cheats->GetSelection());
		break;
	}

	ActionReplayList_Save();
	Cheats->Clear();
	ActionReplayList_Load();

	EditCheat->Enable(false);
	RemoveCheat->Enable(false);
}

void CISOProperties::OnChangeBannerLang(wxCommandEvent& event)
{
	ChangeBannerDetails(event.GetSelection());
}

void CISOProperties::ChangeBannerDetails(int lang)
{
	std::wstring wname;
	wxString shortName,
			 comment,
			 maker;

#ifdef _WIN32
	wxCSConv SJISConv(*(wxCSConv*)wxConvCurrent);
	static bool validCP932 = ::IsValidCodePage(932) != 0;
	if (validCP932)
	{
		SJISConv = wxCSConv(wxFontMapper::GetEncodingName(wxFONTENCODING_SHIFT_JIS));
	}
	else
	{
		WARN_LOG(COMMON, "Cannot Convert from Charset Windows Japanese cp 932");
	}
#else
		// on linux the wrong string is returned from wxFontMapper::GetEncodingName(wxFONTENCODING_SHIFT_JIS)
		// it returns CP-932, in order to use iconv we need to use CP932
		wxCSConv SJISConv(wxT("CP932"));
#endif
	switch (OpenGameListItem->GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_TAIWAN:
	case DiscIO::IVolume::COUNTRY_JAPAN:

		if (OpenGameListItem->GetName(wname, -1))
			shortName = wname.c_str();
		else
			shortName = wxString(OpenGameListItem->GetName(0).c_str(), SJISConv);

		if ((comment = OpenGameListItem->GetDescription().c_str()).size() == 0)
			comment = wxString(OpenGameListItem->GetDescription(0).c_str(), SJISConv);
		maker = wxString(OpenGameListItem->GetCompany().c_str(), SJISConv);
		break;
	case DiscIO::IVolume::COUNTRY_USA:
		lang = 0;
	default:
		{
		wxCSConv WindowsCP1252(wxFontMapper::GetEncodingName(wxFONTENCODING_CP1252));
		if (OpenGameListItem->GetName(wname, lang))
			shortName = wname.c_str();
		else
			shortName = wxString(OpenGameListItem->GetName(lang).c_str(), WindowsCP1252);
		if ((comment = OpenGameListItem->GetDescription().c_str()).size() == 0)
			comment = wxString(OpenGameListItem->GetDescription(lang).c_str(), WindowsCP1252);
		maker = wxString(OpenGameListItem->GetCompany().c_str(), WindowsCP1252);
		}
		break;
	}
	// Updates the informations shown in the window
	m_ShortName->SetValue(shortName);
	m_Comment->SetValue(comment);
	m_Maker->SetValue(maker);//dev too

	std::string filename, extension;
	SplitPath(OpenGameListItem->GetFileName(), 0, &filename, &extension);
	// Also sets the window's title
	SetTitle(wxString(StringFromFormat("%s%s: %s - ", filename.c_str(), extension.c_str(), OpenGameListItem->GetUniqueID().c_str()).c_str(), *wxConvCurrent)+shortName);
}
