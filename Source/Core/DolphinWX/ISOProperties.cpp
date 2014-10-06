// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#endif

#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <type_traits>
#include <vector>
#include <polarssl/md5.h>
#include <wx/arrstr.h>
#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/chartype.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/dirdlg.h>
#include <wx/dynarray.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/gbsizer.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/imaglist.h>
#include <wx/itemid.h>
#include <wx/menu.h>
#include <wx/mimetype.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/progdlg.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/thread.h>
#include <wx/translation.h>
#include <wx/treebase.h>
#include <wx/treectrl.h>
#include <wx/utils.h>
#include <wx/validate.h>
#include <wx/windowid.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Common/SysConf.h"
#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "Core/CoreParameter.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/PatchEngine.h"
#include "Core/Boot/Boot.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DolphinWX/ARCodeAddEdit.h"
#include "DolphinWX/GeckoCodeDiag.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/ISOProperties.h"
#include "DolphinWX/PatchAddEdit.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/resources/isoprop_disc.xpm"
#include "DolphinWX/resources/isoprop_file.xpm"
#include "DolphinWX/resources/isoprop_folder.xpm"

class wxWindow;

struct WiiPartition
{
	DiscIO::IVolume *Partition;
	DiscIO::IFileSystem *FileSystem;
	std::vector<const DiscIO::SFileInfo *> Files;
};
static std::vector<WiiPartition> WiiDisc;

static DiscIO::IVolume *OpenISO = nullptr;
static DiscIO::IFileSystem *pFileSystem = nullptr;

std::vector<PatchEngine::Patch> onFrame;
std::vector<ActionReplay::ARCode> arCodes;
PHackData PHack_Data;


BEGIN_EVENT_TABLE(CISOProperties, wxDialog)
	EVT_CLOSE(CISOProperties::OnClose)
	EVT_BUTTON(wxID_OK, CISOProperties::OnCloseClick)
	EVT_BUTTON(ID_EDITCONFIG, CISOProperties::OnEditConfig)
	EVT_BUTTON(ID_MD5SUMCOMPUTE, CISOProperties::OnComputeMD5Sum)
	EVT_BUTTON(ID_SHOWDEFAULTCONFIG, CISOProperties::OnShowDefaultConfig)
	EVT_CHOICE(ID_EMUSTATE, CISOProperties::SetRefresh)
	EVT_CHOICE(ID_EMU_ISSUES, CISOProperties::SetRefresh)
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
	// Load ISO data
	OpenISO = DiscIO::CreateVolumeFromFilename(fileName);
	bool IsWad = DiscIO::IsVolumeWadFile(OpenISO);
	bool IsWiiDisc = DiscIO::IsVolumeWiiDisc(OpenISO);
	if (IsWiiDisc)
	{
		for (u32 i = 0; i < 0xFFFFFFFF; i++) // yes, technically there can be OVER NINE THOUSAND partitions...
		{
			WiiPartition temp;
			if ((temp.Partition = DiscIO::CreateVolumeFromFilename(fileName, 0, i)) != nullptr)
			{
				if ((temp.FileSystem = DiscIO::CreateFileSystem(temp.Partition)) != nullptr)
				{
					temp.FileSystem->GetFileList(temp.Files);
					WiiDisc.push_back(temp);
				}
			}
			else
			{
				break;
			}
		}
	}
	else
	{
		// TODO : Should we add a way to browse the wad file ?
		if (!IsWad)
		{
			GCFiles.clear();
			pFileSystem = DiscIO::CreateFileSystem(OpenISO);
			if (pFileSystem)
				pFileSystem->GetFileList(GCFiles);
		}
	}

	// Load game ini
	std::string _iniFilename = OpenISO->GetUniqueID();
	std::string _iniFilenameRevisionSpecific = OpenISO->GetRevisionSpecificUniqueID();

	if (!_iniFilename.length())
	{
		u8 title_id[8];
		if (OpenISO->GetTitleID(title_id))
		{
			_iniFilename = StringFromFormat("%016" PRIx64, Common::swap64(title_id));
		}
	}

	GameIniFileDefault = File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + _iniFilename + ".ini";
	std::string GameIniFileDefaultRevisionSpecific = File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + _iniFilenameRevisionSpecific + ".ini";
	GameIniFileLocal = File::GetUserPath(D_GAMESETTINGS_IDX) + _iniFilename + ".ini";

	GameIniDefault.Load(GameIniFileDefault);
	if (_iniFilenameRevisionSpecific != "")
		GameIniDefault.Load(GameIniFileDefaultRevisionSpecific, true);
	GameIniLocal.Load(GameIniFileLocal);

	// Setup GUI
	OpenGameListItem = new GameListItem(fileName);

	bRefreshList = false;

	CreateGUIControls(IsWad);

	LoadGameConfig();

	// Disk header and apploader

	m_Name->SetValue(StrToWxStr(OpenISO->GetName()));
	m_GameID->SetValue(StrToWxStr(OpenISO->GetUniqueID()));
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
		if (!IsWad) // For (non wad) NTSC Games, there's no multi lang
		{
			m_Lang->SetSelection(0);
			m_Lang->Disable();
		}

		break;
	case DiscIO::IVolume::COUNTRY_JAPAN:
		m_Country->SetValue(_("JAPAN"));
		if (!IsWad) // For (non wad) NTSC Games, there's no multi lang
		{
			m_Lang->Insert(_("Japanese"), 0);
			m_Lang->SetSelection(0);
			m_Lang->Disable();
		}
		break;
	case DiscIO::IVolume::COUNTRY_KOREA:
		m_Country->SetValue(_("KOREA"));
		break;
	case DiscIO::IVolume::COUNTRY_TAIWAN:
		m_Country->SetValue(_("TAIWAN"));
		if (!IsWad) // For (non wad) NTSC Games, there's no multi lang
		{
			m_Lang->Insert(_("TAIWAN"), 0);
			m_Lang->SetSelection(0);
			m_Lang->Disable();
		}
		break;
	case DiscIO::IVolume::COUNTRY_SDK:
		m_Country->SetValue(_("No Country (SDK)"));
		break;
	default:
		m_Country->SetValue(_("UNKNOWN"));
		break;
	}

	if (IsWiiDisc) // Only one language with wii banners
	{
		m_Lang->SetSelection(0);
		m_Lang->Disable();
	}

	wxString temp = "0x" + StrToWxStr(OpenISO->GetMakerID());
	m_MakerID->SetValue(temp);
	m_Revision->SetValue(wxString::Format("%u", OpenISO->GetRevision()));
	m_Date->SetValue(StrToWxStr(OpenISO->GetApploaderDate()));
	m_FST->SetValue(wxString::Format("%u", OpenISO->GetFSTSize()));

	// Here we set all the info to be shown (be it SJIS or Ascii) + we set the window title
	if (!IsWad)
	{
		ChangeBannerDetails(SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage);
	}
	else
	{
		ChangeBannerDetails(SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.LNG"));
	}

	m_Banner->SetBitmap(OpenGameListItem->GetBitmap());
	m_Banner->Bind(wxEVT_RIGHT_DOWN, &CISOProperties::RightClickOnBanner, this);

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
		{
			CreateDirectoryTree(RootId, GCFiles, 1, GCFiles.at(0)->m_FileSize);
		}

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
		const DiscIO::SFileInfo* rFileInfo = fileInfos[CurrentIndex];
		std::string filePath = rFileInfo->m_FullPath;

		// Trim the trailing '/' if it exists.
		if (filePath[filePath.length() - 1] == DIR_SEP_CHR)
		{
			filePath.pop_back();
		}

		// Cut off the path up to the actual filename or folder.
		// Say we have "/music/stream/stream1.strm", the result will be "stream1.strm".
		size_t dirSepIndex = filePath.find_last_of(DIR_SEP_CHR);
		if (dirSepIndex != std::string::npos)
		{
			filePath = filePath.substr(dirSepIndex + 1);
		}

		// check next index
		if (rFileInfo->IsDirectory())
		{
			wxTreeItemId item = m_Treectrl->AppendItem(parent, StrToWxStr(filePath), 1, 1);
			CurrentIndex = CreateDirectoryTree(item, fileInfos, CurrentIndex + 1, (size_t)rFileInfo->m_FileSize);
		}
		else
		{
			m_Treectrl->AppendItem(parent, StrToWxStr(filePath), 2, 2);
			CurrentIndex++;
		}
	}

	return CurrentIndex;
}

long CISOProperties::GetElementStyle(const char* section, const char* key)
{
	// Disable 3rd state if default gameini overrides the setting
	if (GameIniDefault.Exists(section, key))
		return 0;

	return wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER;
}

void CISOProperties::CreateGUIControls(bool IsWad)
{
	wxButton* const EditConfig = new wxButton(this, ID_EDITCONFIG, _("Edit Config"));
	EditConfig->SetToolTip(_("This will let you manually edit the INI config file."));

	wxButton* const EditConfigDefault = new wxButton(this, ID_SHOWDEFAULTCONFIG, _("Show Defaults"));
	EditConfigDefault->SetToolTip(_("Opens the default (read-only) configuration for this game in an external text editor."));

	// Notebook
	wxNotebook* const m_Notebook = new wxNotebook(this, ID_NOTEBOOK);
	wxPanel* const m_GameConfig = new wxPanel(m_Notebook, ID_GAMECONFIG);
	m_Notebook->AddPage(m_GameConfig, _("GameConfig"));
	wxPanel* const m_PatchPage = new wxPanel(m_Notebook, ID_PATCH_PAGE);
	m_Notebook->AddPage(m_PatchPage, _("Patches"));
	wxPanel* const m_CheatPage = new wxPanel(m_Notebook, ID_ARCODE_PAGE);
	m_Notebook->AddPage(m_CheatPage, _("AR Codes"));
	m_geckocode_panel = new Gecko::CodeConfigPanel(m_Notebook);
	m_Notebook->AddPage(m_geckocode_panel, _("Gecko Codes"));
	wxPanel* const m_Information = new wxPanel(m_Notebook, ID_INFORMATION);
	m_Notebook->AddPage(m_Information, _("Info"));

	// GameConfig editing - Overrides and emulation state
	wxStaticText* const OverrideText = new wxStaticText(m_GameConfig, wxID_ANY, _("These settings override core Dolphin settings.\nUndetermined means the game uses Dolphin's setting."));

	// Core
	CPUThread = new wxCheckBox(m_GameConfig, ID_USEDUALCORE, _("Enable Dual Core"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "CPUThread"));
	SkipIdle = new wxCheckBox(m_GameConfig, ID_IDLESKIP, _("Enable Idle Skipping"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "SkipIdle"));
	MMU = new wxCheckBox(m_GameConfig, ID_MMU, _("Enable MMU"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "MMU"));
	MMU->SetToolTip(_("Enables the Memory Management Unit, needed for some games. (ON = Compatible, OFF = Fast)"));
	TLBHack = new wxCheckBox(m_GameConfig, ID_TLBHACK, _("MMU Speed Hack"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "TLBHack"));
	TLBHack->SetToolTip(_("Fast version of the MMU. Does not work for every game."));
	DCBZOFF = new wxCheckBox(m_GameConfig, ID_DCBZOFF, _("Skip DCBZ clearing"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "DCBZ"));
	DCBZOFF->SetToolTip(_("Bypass the clearing of the data cache by the DCBZ instruction. Usually leave this option disabled."));
	VBeam = new wxCheckBox(m_GameConfig, ID_VBEAM, _("VBeam Speed Hack"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "VBeam"));
	VBeam->SetToolTip(_("Doubles the emulated GPU clock rate. May speed up some games (ON = Fast, OFF = Compatible)"));
	SyncGPU = new wxCheckBox(m_GameConfig, ID_SYNCGPU, _("Synchronize GPU thread"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "SyncGPU"));
	SyncGPU->SetToolTip(_("Synchronizes the GPU and CPU threads to help prevent random freezes in Dual Core mode. (ON = Compatible, OFF = Fast)"));
	FastDiscSpeed = new wxCheckBox(m_GameConfig, ID_DISCSPEED, _("Speed up Disc Transfer Rate"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "FastDiscSpeed"));
	FastDiscSpeed->SetToolTip(_("Enable fast disc access. Needed for a few games. (ON = Fast, OFF = Compatible)"));
	BlockMerging = new wxCheckBox(m_GameConfig, ID_MERGEBLOCKS, _("Enable Block Merging"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "BlockMerging"));
	DSPHLE = new wxCheckBox(m_GameConfig, ID_AUDIO_DSP_HLE, _("DSP HLE emulation (fast)"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "DSPHLE"));

	// Wii Console
	EnableWideScreen = new wxCheckBox(m_GameConfig, ID_ENABLEWIDESCREEN, _("Enable WideScreen"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Wii", "Widescreen"));

	// Video
	UseBBox = new wxCheckBox(m_GameConfig, ID_USE_BBOX, _("Enable Bounding Box Calculation"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Video", "UseBBox"));
	UseBBox->SetToolTip(_("If checked, the bounding box registers will be updated. Used by the Paper Mario games."));

	wxBoxSizer* const sEmuState = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText* const EmuStateText = new wxStaticText(m_GameConfig, wxID_ANY, _("Emulation State: "));
	arrayStringFor_EmuState.Add(_("Not Set"));
	arrayStringFor_EmuState.Add(_("Broken"));
	arrayStringFor_EmuState.Add(_("Intro"));
	arrayStringFor_EmuState.Add(_("In Game"));
	arrayStringFor_EmuState.Add(_("Playable"));
	arrayStringFor_EmuState.Add(_("Perfect"));
	EmuState = new wxChoice(m_GameConfig, ID_EMUSTATE, wxDefaultPosition, wxDefaultSize, arrayStringFor_EmuState);
	EmuIssues = new wxTextCtrl(m_GameConfig, ID_EMU_ISSUES, wxEmptyString);

	wxBoxSizer* const sConfigPage = new wxBoxSizer(wxVERTICAL);
	wxStaticBoxSizer* const sbCoreOverrides =
		new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Core"));
	sbCoreOverrides->Add(CPUThread, 0, wxLEFT, 5);
	sbCoreOverrides->Add(SkipIdle, 0, wxLEFT, 5);
	sbCoreOverrides->Add(MMU, 0, wxLEFT, 5);
	sbCoreOverrides->Add(TLBHack, 0, wxLEFT, 5);
	sbCoreOverrides->Add(DCBZOFF, 0, wxLEFT, 5);
	sbCoreOverrides->Add(VBeam, 0, wxLEFT, 5);
	sbCoreOverrides->Add(SyncGPU, 0, wxLEFT, 5);
	sbCoreOverrides->Add(FastDiscSpeed, 0, wxLEFT, 5);
	sbCoreOverrides->Add(BlockMerging, 0, wxLEFT, 5);
	sbCoreOverrides->Add(DSPHLE, 0, wxLEFT, 5);

	wxStaticBoxSizer * const sbWiiOverrides = new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Wii Console"));
	if (!DiscIO::IsVolumeWiiDisc(OpenISO) && !DiscIO::IsVolumeWadFile(OpenISO))
	{
		sbWiiOverrides->ShowItems(false);
		EnableWideScreen->Hide();
	}
	sbWiiOverrides->Add(EnableWideScreen, 0, wxLEFT, 5);

	wxStaticBoxSizer * const sbVideoOverrides = new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Video"));
	sbVideoOverrides->Add(UseBBox, 0, wxLEFT, 5);

	wxStaticBoxSizer * const sbGameConfig = new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Game-Specific Settings"));
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
	wxBoxSizer* const sPatches = new wxBoxSizer(wxVERTICAL);
	Patches = new wxCheckListBox(m_PatchPage, ID_PATCHES_LIST, wxDefaultPosition, wxDefaultSize, arrayStringFor_Patches, wxLB_HSCROLL);
	wxBoxSizer* const sPatchButtons = new wxBoxSizer(wxHORIZONTAL);
	EditPatch = new wxButton(m_PatchPage, ID_EDITPATCH, _("Edit..."));
	wxButton* const AddPatch = new wxButton(m_PatchPage, ID_ADDPATCH, _("Add..."));
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
	Cheats = new wxCheckListBox(m_CheatPage, ID_CHEATS_LIST, wxDefaultPosition, wxDefaultSize, arrayStringFor_Cheats, wxLB_HSCROLL);
	wxBoxSizer * const sCheatButtons = new wxBoxSizer(wxHORIZONTAL);
	EditCheat = new wxButton(m_CheatPage, ID_EDITCHEAT, _("Edit..."));
	wxButton * const AddCheat = new wxButton(m_CheatPage, ID_ADDCHEAT, _("Add..."));
	RemoveCheat = new wxButton(m_CheatPage, ID_REMOVECHEAT, _("Remove"));
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


	wxStaticText* const m_NameText = new wxStaticText(m_Information, wxID_ANY, _("Name:"));
	m_Name = new wxTextCtrl(m_Information, ID_NAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_GameIDText = new wxStaticText(m_Information, wxID_ANY, _("Game ID:"));
	m_GameID = new wxTextCtrl(m_Information, ID_GAMEID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_CountryText = new wxStaticText(m_Information, wxID_ANY, _("Country:"));
	m_Country = new wxTextCtrl(m_Information, ID_COUNTRY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_MakerIDText = new wxStaticText(m_Information, wxID_ANY, _("Maker ID:"));
	m_MakerID = new wxTextCtrl(m_Information, ID_MAKERID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_RevisionText = new wxStaticText(m_Information, wxID_ANY, _("Revision:"));
	m_Revision = new wxTextCtrl(m_Information, ID_REVISION, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_DateText = new wxStaticText(m_Information, wxID_ANY, _("Date:"));
	m_Date = new wxTextCtrl(m_Information, ID_DATE, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_FSTText = new wxStaticText(m_Information, wxID_ANY, _("FST Size:"));
	m_FST = new wxTextCtrl(m_Information, ID_FST, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_MD5SumText = new wxStaticText(m_Information, wxID_ANY, _("MD5 Checksum:"));
	m_MD5Sum = new wxTextCtrl(m_Information, ID_MD5SUM, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_MD5SumCompute = new wxButton(m_Information, ID_MD5SUMCOMPUTE, _("Compute"));

	wxStaticText* const m_LangText = new wxStaticText(m_Information, wxID_ANY, _("Show Language:"));
	arrayStringFor_Lang.Add(_("English"));
	arrayStringFor_Lang.Add(_("German"));
	arrayStringFor_Lang.Add(_("French"));
	arrayStringFor_Lang.Add(_("Spanish"));
	arrayStringFor_Lang.Add(_("Italian"));
	arrayStringFor_Lang.Add(_("Dutch"));
	int language = SConfig::GetInstance().m_LocalCoreStartupParameter.SelectedLanguage;
	if (IsWad)
	{
		arrayStringFor_Lang.Insert(_("Japanese"), 0);
		arrayStringFor_Lang.Add(_("Simplified Chinese"));
		arrayStringFor_Lang.Add(_("Traditional Chinese"));
		arrayStringFor_Lang.Add(_("Korean"));

		language = SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.LNG");
	}
	m_Lang = new wxChoice(m_Information, ID_LANG, wxDefaultPosition, wxDefaultSize, arrayStringFor_Lang);
	m_Lang->SetSelection(language);

	wxStaticText* const m_ShortText = new wxStaticText(m_Information, wxID_ANY, _("Short Name:"));
	m_ShortName = new wxTextCtrl(m_Information, ID_SHORTNAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_MakerText = new wxStaticText(m_Information, wxID_ANY, _("Maker:"));
	m_Maker = new wxTextCtrl(m_Information, ID_MAKER, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_CommentText = new wxStaticText(m_Information, wxID_ANY, _("Comment:"));
	m_Comment = new wxTextCtrl(m_Information, ID_COMMENT, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY);
	wxStaticText* const m_BannerText = new wxStaticText(m_Information, wxID_ANY, _("Banner:"));
	m_Banner = new wxStaticBitmap(m_Information, ID_BANNER, wxNullBitmap, wxDefaultPosition, wxSize(96, 32));

	// ISO Details
	wxGridBagSizer* const sISODetails = new wxGridBagSizer(0, 0);
	sISODetails->Add(m_NameText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Name, wxGBPosition(0, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_GameIDText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_GameID, wxGBPosition(1, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_CountryText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Country, wxGBPosition(2, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_MakerIDText, wxGBPosition(3, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_MakerID, wxGBPosition(3, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_RevisionText, wxGBPosition(4, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Revision, wxGBPosition(4, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_DateText, wxGBPosition(5, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_Date, wxGBPosition(5, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_FSTText, wxGBPosition(6, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_FST, wxGBPosition(6, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sISODetails->Add(m_MD5SumText, wxGBPosition(7, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_MD5Sum, wxGBPosition(7, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	wxSizer* sMD5SumButtonSizer = CreateButtonSizer(wxNO_DEFAULT);
	sMD5SumButtonSizer->Add(m_MD5SumCompute);
	sISODetails->Add(sMD5SumButtonSizer, wxGBPosition(7, 2), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);

	sISODetails->AddGrowableCol(1);
	wxStaticBoxSizer* const sbISODetails =
		new wxStaticBoxSizer(wxVERTICAL, m_Information, _("ISO Details"));
	sbISODetails->Add(sISODetails, 0, wxEXPAND, 5);

	// Banner Details
	wxGridBagSizer* const sBannerDetails = new wxGridBagSizer(0, 0);
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
	wxStaticBoxSizer* const sbBannerDetails =
		new wxStaticBoxSizer(wxVERTICAL, m_Information, _("Banner Details"));
	sbBannerDetails->Add(sBannerDetails, 0, wxEXPAND, 5);

	wxBoxSizer* const sInfoPage = new wxBoxSizer(wxVERTICAL);
	sInfoPage->Add(sbISODetails, 0, wxEXPAND|wxALL, 5);
	sInfoPage->Add(sbBannerDetails, 0, wxEXPAND|wxALL, 5);
	m_Information->SetSizer(sInfoPage);

	if (!IsWad)
	{
		wxPanel* const m_Filesystem = new wxPanel(m_Notebook, ID_FILESYSTEM);
		m_Notebook->AddPage(m_Filesystem, _("Filesystem"));

		// Filesystem icons
		wxImageList* const m_iconList = new wxImageList(16, 16);
		m_iconList->Add(wxBitmap(disc_xpm), wxNullBitmap);   // 0
		m_iconList->Add(wxBitmap(folder_xpm), wxNullBitmap); // 1
		m_iconList->Add(wxBitmap(file_xpm), wxNullBitmap);   // 2

		// Filesystem tree
		m_Treectrl = new wxTreeCtrl(m_Filesystem, ID_TREECTRL);
		m_Treectrl->AssignImageList(m_iconList);
		RootId = m_Treectrl->AddRoot(_("Disc"), 0, 0, nullptr);

		wxBoxSizer* sTreePage = new wxBoxSizer(wxVERTICAL);
		sTreePage->Add(m_Treectrl, 1, wxEXPAND|wxALL, 5);
		m_Filesystem->SetSizer(sTreePage);
	}

	wxSizer* sButtons = CreateButtonSizer(wxNO_DEFAULT);
	sButtons->Prepend(EditConfigDefault);
	sButtons->Prepend(EditConfig);
	sButtons->Add(new wxButton(this, wxID_OK, _("Close")));

	// If there is no default gameini, disable the button.
	if (!File::Exists(GameIniFileDefault))
		EditConfigDefault->Disable();

	// Add notebook and buttons to the dialog
	wxBoxSizer* sMain = new wxBoxSizer(wxVERTICAL);
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
		WxUtils::ShowErrorDialog(wxString::Format(_("Could not save %s."), GameIniFileLocal.c_str()));

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

	wxFileDialog dialog(this, _("Save as..."), wxGetHomeDir(&dirHome), wxString::Format("%s.png", m_GameID->GetLabel().c_str()),
		wxALL_FILES_PATTERN, wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
	if (dialog.ShowModal() == wxID_OK)
	{
		m_Banner->GetBitmap().ConvertToImage().SaveFile(dialog.GetPath());
	}
}

void CISOProperties::OnRightClickOnTree(wxTreeEvent& event)
{
	m_Treectrl->SelectItem(event.GetItem());

	wxMenu* popupMenu = new wxMenu;

	if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 0 &&
	    m_Treectrl->GetFirstVisibleItem() != m_Treectrl->GetSelection())
	{
		popupMenu->Append(IDM_EXTRACTDIR, _("Extract Partition..."));
	}
	else if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 1)
	{
		popupMenu->Append(IDM_EXTRACTDIR, _("Extract Directory..."));
	}
	else if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 2)
	{
		popupMenu->Append(IDM_EXTRACTFILE, _("Extract File..."));
	}

	popupMenu->Append(IDM_EXTRACTALL, _("Extract All Files..."));

	if (!DiscIO::IsVolumeWiiDisc(OpenISO) ||
		(m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 0 &&
		m_Treectrl->GetFirstVisibleItem() != m_Treectrl->GetSelection()))
	{
		popupMenu->AppendSeparator();
		popupMenu->Append(IDM_EXTRACTAPPLOADER, _("Extract Apploader..."));
		popupMenu->Append(IDM_EXTRACTDOL, _("Extract DOL..."));
	}

	if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 0 &&
		m_Treectrl->GetFirstVisibleItem() != m_Treectrl->GetSelection())
	{
		popupMenu->AppendSeparator();
		popupMenu->Append(IDM_CHECKINTEGRITY, _("Check Partition Integrity"));
	}

	PopupMenu(popupMenu);

	event.Skip();
}

void CISOProperties::OnExtractFile(wxCommandEvent& WXUNUSED (event))
{
	wxString File = m_Treectrl->GetItemText(m_Treectrl->GetSelection());

	wxString Path = wxFileSelector(
		_("Export File"),
		wxEmptyString, File, wxEmptyString,
		wxGetTranslation(wxALL_FILES),
		wxFD_SAVE,
		this);

	if (!Path || !File)
		return;

	while (m_Treectrl->GetItemParent(m_Treectrl->GetSelection()) != m_Treectrl->GetRootItem())
	{
		wxString temp = m_Treectrl->GetItemText(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
		File = temp + DIR_SEP_CHR + File;

		m_Treectrl->SelectItem(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
	}

	if (DiscIO::IsVolumeWiiDisc(OpenISO))
	{
		int partitionNum = wxAtoi(File.Mid(File.find_first_of("/") - 1, 1));
		File.erase(0, File.find_first_of("/") + 1); // Remove "Partition x/"
		WiiDisc.at(partitionNum).FileSystem->ExportFile(WxStrToStr(File), WxStrToStr(Path));
	}
	else
	{
		pFileSystem->ExportFile(WxStrToStr(File), WxStrToStr(Path));
	}
}

void CISOProperties::ExportDir(const std::string& _rFullPath, const std::string& _rExportFolder, const int partitionNum)
{
	DiscIO::IFileSystem* const fs = DiscIO::IsVolumeWiiDisc(OpenISO) ? WiiDisc[partitionNum].FileSystem : pFileSystem;

	std::vector<const DiscIO::SFileInfo*> fst;
	fs->GetFileList(fst);

	u32 index = 0;
	u32 size = 0;

	// Extract all
	if (_rFullPath.empty())
	{
		index = 0;
		size = (u32)fst.size();

		fs->ExportApploader(_rExportFolder);
		if (!DiscIO::IsVolumeWiiDisc(OpenISO))
			fs->ExportDOL(_rExportFolder);
	}
	else
	{
		// Look for the dir we are going to extract
		for (index = 0; index != fst.size(); ++index)
		{
			if (fst[index]->m_FullPath == _rFullPath)
			{
				DEBUG_LOG(DISCIO, "Found the directory at %u", index);
				size = (u32)fst[index]->m_FileSize;
				break;
			}
		}

		DEBUG_LOG(DISCIO,"Directory found from %u to %u\nextracting to:\n%s", index , size, _rExportFolder.c_str());
	}

	wxString dialogTitle = (index != 0) ? _("Extracting Directory") : _("Extracting All Files");
	wxProgressDialog dialog(
		dialogTitle,
		_("Extracting..."),
		size - 1,
		this,
		wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT |
		wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME |
		wxPD_SMOOTH
		);

	// Extraction
	for (u32 i = index; i < size; i++)
	{
		dialog.SetTitle(wxString::Format("%s : %d%%", dialogTitle.c_str(),
			(u32)(((float)(i - index) / (float)(size - index)) * 100)));

		dialog.Update(i, wxString::Format(_("Extracting %s"), StrToWxStr(fst[i]->m_FullPath)));

		if (dialog.WasCancelled())
			break;

		if (fst[i]->IsDirectory())
		{
			const std::string exportName = StringFromFormat("%s/%s/", _rExportFolder.c_str(), fst[i]->m_FullPath.c_str());
			DEBUG_LOG(DISCIO, "%s", exportName.c_str());

			if (!File::Exists(exportName) && !File::CreateFullPath(exportName))
			{
				ERROR_LOG(DISCIO, "Could not create the path %s", exportName.c_str());
			}
			else
			{
				if (!File::IsDirectory(exportName))
					ERROR_LOG(DISCIO, "%s already exists and is not a directory", exportName.c_str());

				DEBUG_LOG(DISCIO, "Folder %s already exists", exportName.c_str());
			}
		}
		else
		{
			const std::string exportName = StringFromFormat("%s/%s", _rExportFolder.c_str(), fst[i]->m_FullPath.c_str());
			DEBUG_LOG(DISCIO, "%s", exportName.c_str());

			if (!File::Exists(exportName) && !fs->ExportFile(fst[i]->m_FullPath, exportName))
			{
				ERROR_LOG(DISCIO, "Could not export %s", exportName.c_str());
			}
			else
			{
				DEBUG_LOG(DISCIO, "%s already exists", exportName.c_str());
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
				ExportDir("", WxStrToStr(Path), i);
		else
			ExportDir("", WxStrToStr(Path));

		return;
	}

	while (m_Treectrl->GetItemParent(m_Treectrl->GetSelection()) != m_Treectrl->GetRootItem())
	{
		wxString temp = m_Treectrl->GetItemText(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
		Directory = temp + DIR_SEP_CHR + Directory;

		m_Treectrl->SelectItem(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
	}

	Directory += DIR_SEP_CHR;

	if (DiscIO::IsVolumeWiiDisc(OpenISO))
	{
		int partitionNum = wxAtoi(Directory.Mid(Directory.find_first_of("/") - 1, 1));
		Directory.erase(0, Directory.find_first_of("/") + 1); // Remove "Partition x/"
		ExportDir(WxStrToStr(Directory), WxStrToStr(Path), partitionNum);
	}
	else
	{
		ExportDir(WxStrToStr(Directory), WxStrToStr(Path));
	}
}

void CISOProperties::OnExtractDataFromHeader(wxCommandEvent& event)
{
	DiscIO::IFileSystem *FS = nullptr;
	wxString Path = wxDirSelector(_("Choose the folder to extract to"));

	if (Path.empty())
		return;

	if (DiscIO::IsVolumeWiiDisc(OpenISO))
	{
		wxString Directory = m_Treectrl->GetItemText(m_Treectrl->GetSelection());
		unsigned int partitionNum = wxAtoi(Directory.Mid(Directory.find_first_of("0123456789"), 2));

		if (WiiDisc.size() > partitionNum)
		{
			// Get the filesystem of the LAST partition
			FS = WiiDisc.at(partitionNum).FileSystem;
		}
		else
		{
			WxUtils::ShowErrorDialog(wxString::Format(_("Partition doesn't exist: %d"), partitionNum));
			return;
		}
	}
	else
	{
		FS = pFileSystem;
	}

	bool ret = false;
	if (event.GetId() == IDM_EXTRACTAPPLOADER)
	{
		ret = FS->ExportApploader(WxStrToStr(Path));
	}
	else if (event.GetId() == IDM_EXTRACTDOL)
	{
		ret = FS->ExportDOL(WxStrToStr(Path));
	}

	if (!ret)
		WxUtils::ShowErrorDialog(wxString::Format(_("Failed to extract to %s!"), WxStrToStr(Path).c_str()));
}

class IntegrityCheckThread : public wxThread
{
public:
	IntegrityCheckThread(const WiiPartition& Partition)
		: wxThread(wxTHREAD_JOINABLE), m_Partition(Partition)
	{
		Create();
	}

	virtual ExitCode Entry() override
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
	int PartitionNum = wxAtoi(PartitionName.Mid(PartitionName.find_first_of("0123456789"), 1));
	const WiiPartition& Partition = WiiDisc[PartitionNum];

	wxProgressDialog dialog(_("Checking integrity..."), _("Working..."), 1000, this,
		wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_SMOOTH
	);

	IntegrityCheckThread thread(Partition);
	thread.Run();

	while (thread.IsAlive())
	{
		dialog.Pulse();
		wxThread::Sleep(50);
	}

	dialog.Destroy();

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

void CISOProperties::SetCheckboxValueFromGameini(const char* section, const char* key, wxCheckBox* checkbox)
{
	// Prefer local gameini value over default gameini value.
	bool value;
	if (GameIniLocal.GetOrCreateSection(section)->Get(key, &value))
		checkbox->Set3StateValue((wxCheckBoxState)value);
	else if (GameIniDefault.GetOrCreateSection(section)->Get(key, &value))
		checkbox->Set3StateValue((wxCheckBoxState)value);
	else
		checkbox->Set3StateValue(wxCHK_UNDETERMINED);
}

void CISOProperties::LoadGameConfig()
{
	SetCheckboxValueFromGameini("Core", "CPUThread", CPUThread);
	SetCheckboxValueFromGameini("Core", "SkipIdle", SkipIdle);
	SetCheckboxValueFromGameini("Core", "MMU", MMU);
	SetCheckboxValueFromGameini("Core", "TLBHack", TLBHack);
	SetCheckboxValueFromGameini("Core", "DCBZ", DCBZOFF);
	SetCheckboxValueFromGameini("Core", "VBeam", VBeam);
	SetCheckboxValueFromGameini("Core", "SyncGPU", SyncGPU);
	SetCheckboxValueFromGameini("Core", "FastDiscSpeed", FastDiscSpeed);
	SetCheckboxValueFromGameini("Core", "BlockMerging", BlockMerging);
	SetCheckboxValueFromGameini("Core", "DSPHLE", DSPHLE);
	SetCheckboxValueFromGameini("Wii", "Widescreen", EnableWideScreen);
	SetCheckboxValueFromGameini("Video", "UseBBox", UseBBox);

	IniFile::Section* default_video = GameIniDefault.GetOrCreateSection("Video");

	int iTemp;
	default_video->Get("ProjectionHack", &iTemp);
	default_video->Get("PH_SZNear", &PHack_Data.PHackSZNear);
	if (GameIniLocal.GetIfExists("Video", "PH_SZNear", &iTemp))
		PHack_Data.PHackSZNear = !!iTemp;
	default_video->Get("PH_SZFar", &PHack_Data.PHackSZFar);
	if (GameIniLocal.GetIfExists("Video", "PH_SZFar", &iTemp))
		PHack_Data.PHackSZFar = !!iTemp;

	std::string sTemp;
	default_video->Get("PH_ZNear", &PHack_Data.PHZNear);
	if (GameIniLocal.GetIfExists("Video", "PH_ZNear", &sTemp))
		PHack_Data.PHZNear = sTemp;
	default_video->Get("PH_ZFar", &PHack_Data.PHZFar);
	if (GameIniLocal.GetIfExists("Video", "PH_ZFar", &sTemp))
		PHack_Data.PHZFar = sTemp;

	IniFile::Section* default_emustate = GameIniDefault.GetOrCreateSection("EmuState");
	default_emustate->Get("EmulationStateId", &iTemp, 0/*Not Set*/);
	EmuState->SetSelection(iTemp);
	if (GameIniLocal.GetIfExists("EmuState", "EmulationStateId", &iTemp))
		EmuState->SetSelection(iTemp);

	default_emustate->Get("EmulationIssues", &sTemp);
	if (!sTemp.empty())
		EmuIssues->SetValue(StrToWxStr(sTemp));
	if (GameIniLocal.GetIfExists("EmuState", "EmulationIssues", &sTemp))
		EmuIssues->SetValue(StrToWxStr(sTemp));

	EmuIssues->Enable(EmuState->GetSelection() != 0);

	PatchList_Load();
	ActionReplayList_Load();
	m_geckocode_panel->LoadCodes(GameIniDefault, GameIniLocal, OpenISO->GetUniqueID());
}

void CISOProperties::SaveGameIniValueFrom3StateCheckbox(const char* section, const char* key, wxCheckBox* checkbox)
{
	// Delete any existing entries from the local gameini if checkbox is undetermined.
	// Otherwise, write the current value to the local gameini if the value differs from the default gameini values.
	// Delete any existing entry from the local gameini if the value does not differ from the default gameini value.
	bool checkbox_val = (checkbox->Get3StateValue() == wxCHK_CHECKED);

	if (checkbox->Get3StateValue() == wxCHK_UNDETERMINED)
		GameIniLocal.DeleteKey(section, key);
	else if (!GameIniDefault.Exists(section, key))
		GameIniLocal.GetOrCreateSection(section)->Set(key, checkbox_val);
	else
	{
		bool default_value;
		GameIniDefault.GetOrCreateSection(section)->Get(key, &default_value);
		if (default_value != checkbox_val)
			GameIniLocal.GetOrCreateSection(section)->Set(key, checkbox_val);
		else
			GameIniLocal.DeleteKey(section, key);
	}
}

bool CISOProperties::SaveGameConfig()
{
	SaveGameIniValueFrom3StateCheckbox("Core", "CPUThread", CPUThread);
	SaveGameIniValueFrom3StateCheckbox("Core", "SkipIdle", SkipIdle);
	SaveGameIniValueFrom3StateCheckbox("Core", "MMU", MMU);
	SaveGameIniValueFrom3StateCheckbox("Core", "TLBHack", TLBHack);
	SaveGameIniValueFrom3StateCheckbox("Core", "DCBZ", DCBZOFF);
	SaveGameIniValueFrom3StateCheckbox("Core", "VBeam", VBeam);
	SaveGameIniValueFrom3StateCheckbox("Core", "SyncGPU", SyncGPU);
	SaveGameIniValueFrom3StateCheckbox("Core", "FastDiscSpeed", FastDiscSpeed);
	SaveGameIniValueFrom3StateCheckbox("Core", "BlockMerging", BlockMerging);
	SaveGameIniValueFrom3StateCheckbox("Core", "DSPHLE", DSPHLE);
	SaveGameIniValueFrom3StateCheckbox("Wii", "Widescreen", EnableWideScreen);
	SaveGameIniValueFrom3StateCheckbox("Video", "UseBBox", UseBBox);

	#define SAVE_IF_NOT_DEFAULT(section, key, val, def) do { \
		if (GameIniDefault.Exists((section), (key))) { \
			std::remove_reference<decltype((val))>::type tmp__; \
			GameIniDefault.GetOrCreateSection((section))->Get((key), &tmp__); \
			if ((val) != tmp__) \
				GameIniLocal.GetOrCreateSection((section))->Set((key), (val)); \
			else \
				GameIniLocal.DeleteKey((section), (key)); \
		} else if ((val) != (def)) \
			GameIniLocal.GetOrCreateSection((section))->Set((key), (val)); \
		else \
			GameIniLocal.DeleteKey((section), (key)); \
	} while (0)

	SAVE_IF_NOT_DEFAULT("Video", "PH_SZNear", (PHack_Data.PHackSZNear ? 1 : 0), 0);
	SAVE_IF_NOT_DEFAULT("Video", "PH_SZFar", (PHack_Data.PHackSZFar ? 1 : 0), 0);
	SAVE_IF_NOT_DEFAULT("Video", "PH_ZNear", PHack_Data.PHZNear, "");
	SAVE_IF_NOT_DEFAULT("Video", "PH_ZFar", PHack_Data.PHZFar, "");
	SAVE_IF_NOT_DEFAULT("EmuState", "EmulationStateId", EmuState->GetSelection(), 0);

	std::string emu_issues = EmuIssues->GetValue().ToStdString();
	SAVE_IF_NOT_DEFAULT("EmuState", "EmulationIssues", emu_issues, "");

	PatchList_Save();
	ActionReplayList_Save();
	Gecko::SaveCodes(GameIniLocal, m_geckocode_panel->GetCodes());

	bool success = GameIniLocal.Save(GameIniFileLocal);

	// If the resulting file is empty, delete it. Kind of a hack, but meh.
	if (success && File::GetSize(GameIniFileLocal) == 0)
		File::Delete(GameIniFileLocal);

	return success;
}

void CISOProperties::LaunchExternalEditor(const std::string& filename)
{
#ifdef __APPLE__
	// wxTheMimeTypesManager is not yet implemented for wxCocoa
	[[NSWorkspace sharedWorkspace] openFile:
		[NSString stringWithUTF8String: filename.c_str()]
		withApplication: @"TextEdit"];
#else
	wxFileType* filetype = wxTheMimeTypesManager->GetFileTypeFromExtension("ini");
	if (filetype == nullptr) // From extension failed, trying with MIME type now
	{
		filetype = wxTheMimeTypesManager->GetFileTypeFromMimeType("text/plain");
		if (filetype == nullptr) // MIME type failed, aborting mission
		{
			WxUtils::ShowErrorDialog(_("Filetype 'ini' is unknown! Will not open!"));
			return;
		}
	}

	wxString OpenCommand = filetype->GetOpenCommand(StrToWxStr(filename));
	if (OpenCommand.IsEmpty())
	{
		WxUtils::ShowErrorDialog(_("Couldn't find open command for extension 'ini'!"));
	}
	else
	{
		if (wxExecute(OpenCommand, wxEXEC_SYNC) == -1)
			WxUtils::ShowErrorDialog(_("wxExecute returned -1 on application run!"));
	}
#endif

	bRefreshList = true; // Just in case

	// Once we're done with the ini edit, give the focus back to Dolphin
	SetFocus();
}

void CISOProperties::OnEditConfig(wxCommandEvent& WXUNUSED (event))
{
	SaveGameConfig();
	// Create blank file to prevent editor from prompting to create it.
	if (!File::Exists(GameIniFileLocal))
	{
		std::fstream blankFile(GameIniFileLocal, std::ios::out);
		blankFile.close();
	}
	LaunchExternalEditor(GameIniFileLocal);
	GameIniLocal.Load(GameIniFileLocal);
	LoadGameConfig();
}

void CISOProperties::OnComputeMD5Sum(wxCommandEvent& WXUNUSED (event))
{
	u8 output[16];
	std::string output_string;
	std::vector<u8> data(8 * 1024 * 1024);
	u64 read_offset = 0;
	md5_context ctx;

	File::IOFile file(OpenGameListItem->GetFileName(), "rb");
	u64 game_size = file.GetSize();

	wxProgressDialog progressDialog(
		_("Computing MD5 checksum"),
		_("Working..."),
		1000,
		this,
		wxPD_APP_MODAL | wxPD_CAN_ABORT |
		wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_REMAINING_TIME |
		wxPD_SMOOTH
		);

	md5_starts(&ctx);

	while(read_offset < game_size)
	{
		if (!progressDialog.Update((int)((double)read_offset / (double)game_size * 1000), _("Computing MD5 checksum")))
			return;

		size_t read_size;
		file.ReadArray(&data[0], data.size(), &read_size);
		md5_update(&ctx, &data[0], read_size);

		read_offset += read_size;
	}

	md5_finish(&ctx, output);

	// Convert to hex
	for (int a = 0; a < 16; ++a)
		output_string += StringFromFormat("%02x", output[a]);

	m_MD5Sum->SetValue(output_string);
}

void CISOProperties::OnShowDefaultConfig(wxCommandEvent& WXUNUSED (event))
{
	LaunchExternalEditor(GameIniFileDefault);
}

void CISOProperties::ListSelectionChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_PATCHES_LIST:
		if (Patches->GetSelection() == wxNOT_FOUND ||
		    DefaultPatches.find(Patches->GetString(Patches->GetSelection()).ToStdString()) != DefaultPatches.end())
		{
			EditPatch->Disable();
			RemovePatch->Disable();
		}
		else
		{
			EditPatch->Enable();
			RemovePatch->Enable();
		}
		break;
	case ID_CHEATS_LIST:
		if (Cheats->GetSelection() == wxNOT_FOUND ||
		    DefaultCheats.find(Cheats->GetString(Cheats->GetSelection()).ToStdString()) != DefaultCheats.end())
		{
			EditCheat->Disable();
			RemoveCheat->Disable();
		}
		else
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

	PatchEngine::LoadPatchSection("OnFrame", onFrame, GameIniDefault, GameIniLocal);

	u32 index = 0;
	for (PatchEngine::Patch& p : onFrame)
	{
		Patches->Append(StrToWxStr(p.name));
		Patches->Check(index, p.active);
		if (!p.user_defined)
			DefaultPatches.insert(p.name);
		++index;
	}
}

void CISOProperties::PatchList_Save()
{
	std::vector<std::string> lines;
	std::vector<std::string> enabledLines;
	u32 index = 0;
	for (PatchEngine::Patch& p : onFrame)
	{
		if (Patches->IsChecked(index))
			enabledLines.push_back("$" + p.name);

		// Do not save default patches.
		if (DefaultPatches.find(p.name) == DefaultPatches.end())
		{
			lines.push_back("$" + p.name);
			for (const PatchEngine::PatchEntry& entry : p.entries)
			{
				std::string temp = StringFromFormat("0x%08X:%s:0x%08X", entry.address, PatchEngine::PatchTypeStrings[entry.type], entry.value);
				lines.push_back(temp);
			}
		}
		++index;
	}
	GameIniLocal.SetLines("OnFrame_Enabled", enabledLines);
	GameIniLocal.SetLines("OnFrame", lines);
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
			Patches->Append(StrToWxStr(onFrame.back().name));
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
	ActionReplay::LoadCodes(arCodes, GameIniDefault, GameIniLocal);

	u32 index = 0;
	for (const ActionReplay::ARCode& arCode : arCodes)
	{
		Cheats->Append(StrToWxStr(arCode.name));
		Cheats->Check(index, arCode.active);
		if (!arCode.user_defined)
			DefaultCheats.insert(arCode.name);
		++index;
	}
}

void CISOProperties::ActionReplayList_Save()
{
	std::vector<std::string> lines;
	std::vector<std::string> enabledLines;
	u32 index = 0;
	u32 cheats_chkbox_count = Cheats->GetCount();
	for (const ActionReplay::ARCode& code : arCodes)
	{
		// Check the index against the count because of the hacky way codes are added from the "Cheat Search" dialog
		if ((index < cheats_chkbox_count) && Cheats->IsChecked(index))
			enabledLines.push_back("$" + code.name);

		// Do not save default cheats.
		if (DefaultCheats.find(code.name) == DefaultCheats.end())
		{
			lines.push_back("$" + code.name);
			for (const ActionReplay::AREntry& op : code.ops)
			{
				lines.push_back(WxStrToStr(wxString::Format("%08X %08X", op.cmd_addr, op.value)));
			}
		}
		++index;
	}
	GameIniLocal.SetLines("ActionReplay_Enabled", enabledLines);
	GameIniLocal.SetLines("ActionReplay", lines);
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
				Cheats->Append(StrToWxStr(arCodes.back().name));
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
	wxString const shortName = StrToWxStr(OpenGameListItem->GetName(lang));
	wxString const comment = StrToWxStr(OpenGameListItem->GetDescription(lang));
	wxString const maker = StrToWxStr(OpenGameListItem->GetCompany());

	// Updates the information shown in the window
	m_ShortName->SetValue(shortName);
	m_Comment->SetValue(comment);
	m_Maker->SetValue(maker);//dev too

	std::string filename, extension;
	SplitPath(OpenGameListItem->GetFileName(), nullptr, &filename, &extension);
	// Also sets the window's title
	SetTitle(StrToWxStr(StringFromFormat("%s%s: %s - ", filename.c_str(),
		extension.c_str(), OpenGameListItem->GetUniqueID().c_str())) + shortName);
}
