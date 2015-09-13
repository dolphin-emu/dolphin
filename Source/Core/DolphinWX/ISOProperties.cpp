// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
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
#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/gbsizer.h>
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
#include <wx/slider.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/thread.h>
#include <wx/treebase.h>
#include <wx/treectrl.h>
#include <wx/utils.h>
#include <wx/validate.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Common/SysConf.h"
#include "Core/ActionReplay.h"
#include "Core/ConfigManager.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/PatchEngine.h"
#include "Core/HideObjectEngine.h"
#include "Core/Boot/Boot.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DolphinWX/ARCodeAddEdit.h"
#include "DolphinWX/GameListCtrl.h"
//#include "DolphinWX/Frame.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/ISOProperties.h"
#include "DolphinWX/PatchAddEdit.h"
#include "DolphinWX/HideObjectAddEdit.h"
#include "DolphinWX/VideoConfigDiag.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Cheats/GeckoCodeDiag.h"
#include "DolphinWX/resources/isoprop_disc.xpm"
#include "DolphinWX/resources/isoprop_file.xpm"
#include "DolphinWX/resources/isoprop_folder.xpm"
#include "VideoCommon/VR.h"

std::vector<HideObjectEngine::HideObject> HideObjectCodes;

BEGIN_EVENT_TABLE(CISOProperties, wxDialog)
	EVT_CLOSE(CISOProperties::OnClose)
	EVT_BUTTON(wxID_OK, CISOProperties::OnCloseClick)
	EVT_BUTTON(ID_EDITCONFIG, CISOProperties::OnEditConfig)
	EVT_BUTTON(ID_MD5SUMCOMPUTE, CISOProperties::OnComputeMD5Sum)
	EVT_BUTTON(ID_SHOWDEFAULTCONFIG, CISOProperties::OnShowDefaultConfig)
	EVT_CHOICE(ID_EMUSTATE, CISOProperties::SetRefresh)
	EVT_CHOICE(ID_EMU_ISSUES, CISOProperties::SetRefresh)
	EVT_LISTBOX(ID_HIDEOBJECTS_LIST, CISOProperties::ListSelectionChanged)
	EVT_CHECKLISTBOX(ID_HIDEOBJECTS_LIST, CISOProperties::CheckboxSelectionChanged)
	EVT_BUTTON(ID_EDITHIDEOBJECT, CISOProperties::HideObjectButtonClicked)
	EVT_BUTTON(ID_ADDHideObject, CISOProperties::HideObjectButtonClicked)
	EVT_BUTTON(ID_REMOVEHIDEOBJECT, CISOProperties::HideObjectButtonClicked)
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

CISOProperties::CISOProperties(const std::string& fileName, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	// Load ISO data
	OpenISO = DiscIO::CreateVolumeFromFilename(fileName);

	// Is it really necessary to use GetTitleID if GetUniqueID fails?
	game_id = OpenISO->GetUniqueID();
	if (game_id.empty())
	{
		u8 game_id_bytes[8];
		if (OpenISO->GetTitleID(game_id_bytes))
		{
			game_id = StringFromFormat("%016" PRIx64, Common::swap64(game_id_bytes));
		}
	}

	// Load game INIs
	GameIniFileLocal = File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini";
	GameIniDefault = SConfig::LoadDefaultGameIni(game_id, OpenISO->GetRevision());
	GameIniLocal = SConfig::LoadLocalGameIni(game_id, OpenISO->GetRevision());

	// Setup GUI
	OpenGameListItem = new GameListItem(fileName);

	bRefreshList = false;

	CreateGUIControls();

	LoadGameConfig();

	// Disk header and apploader

	m_InternalName->SetValue(StrToWxStr(OpenISO->GetInternalName()));
	m_GameID->SetValue(StrToWxStr(OpenISO->GetUniqueID()));
	switch (OpenISO->GetCountry())
	{
	case DiscIO::IVolume::COUNTRY_AUSTRALIA:
		m_Country->SetValue(_("Australia"));
		break;
	case DiscIO::IVolume::COUNTRY_EUROPE:
		m_Country->SetValue(_("Europe"));
		break;
	case DiscIO::IVolume::COUNTRY_FRANCE:
		m_Country->SetValue(_("France"));
		break;
	case DiscIO::IVolume::COUNTRY_ITALY:
		m_Country->SetValue(_("Italy"));
		break;
	case DiscIO::IVolume::COUNTRY_GERMANY:
		m_Country->SetValue(_("Germany"));
		break;
	case DiscIO::IVolume::COUNTRY_NETHERLANDS:
		m_Country->SetValue(_("Netherlands"));
		break;
	case DiscIO::IVolume::COUNTRY_RUSSIA:
		m_Country->SetValue(_("Russia"));
		break;
	case DiscIO::IVolume::COUNTRY_SPAIN:
		m_Country->SetValue(_("Spain"));
		break;
	case DiscIO::IVolume::COUNTRY_USA:
		m_Country->SetValue(_("USA"));
		break;
	case DiscIO::IVolume::COUNTRY_JAPAN:
		m_Country->SetValue(_("Japan"));
		break;
	case DiscIO::IVolume::COUNTRY_KOREA:
		m_Country->SetValue(_("Korea"));
		break;
	case DiscIO::IVolume::COUNTRY_TAIWAN:
		m_Country->SetValue(_("Taiwan"));
		break;
	case DiscIO::IVolume::COUNTRY_WORLD:
		m_Country->SetValue(_("World"));
		break;
	case DiscIO::IVolume::COUNTRY_UNKNOWN:
	default:
		m_Country->SetValue(_("Unknown"));
		break;
	}

	wxString temp = "0x" + StrToWxStr(OpenISO->GetMakerID());
	m_MakerID->SetValue(temp);
	m_Revision->SetValue(StrToWxStr(std::to_string(OpenISO->GetRevision())));
	m_Date->SetValue(StrToWxStr(OpenISO->GetApploaderDate()));
	m_FST->SetValue(StrToWxStr(std::to_string(OpenISO->GetFSTSize())));

	// Here we set all the info to be shown + we set the window title
	bool wii = OpenISO->GetVolumeType() != DiscIO::IVolume::GAMECUBE_DISC;
	ChangeBannerDetails(SConfig::GetInstance().GetCurrentLanguage(wii));

	m_Banner->SetBitmap(OpenGameListItem->GetBitmap());
	m_Banner->Bind(wxEVT_RIGHT_DOWN, &CISOProperties::RightClickOnBanner, this);

	// Filesystem browser/dumper
	// TODO : Should we add a way to browse the wad file ?
	if (OpenISO->GetVolumeType() != DiscIO::IVolume::WII_WAD)
	{
		if (OpenISO->GetVolumeType() == DiscIO::IVolume::WII_DISC)
		{
			int partition_count = 0;
			for (int group = 0; group < 4; group++)
			{
				for (u32 i = 0; i < 0xFFFFFFFF; i++) // yes, technically there can be OVER NINE THOUSAND partitions...
				{
					std::unique_ptr<DiscIO::IVolume> volume(DiscIO::CreateVolumeFromFilename(fileName, group, i));
					if (volume != nullptr)
					{
						std::unique_ptr<DiscIO::IFileSystem> file_system(DiscIO::CreateFileSystem(volume.get()));
						if (file_system != nullptr)
						{
							WiiPartition* const partition = new WiiPartition(std::move(volume), std::move(file_system));

							wxTreeItemId PartitionRoot =
								m_Treectrl->AppendItem(RootId, wxString::Format(_("Partition %i"), partition_count), 0, 0);

							m_Treectrl->SetItemData(PartitionRoot, partition);
							CreateDirectoryTree(PartitionRoot, partition->FileSystem->GetFileList());

							if (partition_count == 1)
								m_Treectrl->Expand(PartitionRoot);

							partition_count++;
						}
					}
					else
					{
						break;
					}
				}
			}
		}
		else
		{
			pFileSystem = DiscIO::CreateFileSystem(OpenISO);
			if (pFileSystem)
				CreateDirectoryTree(RootId, pFileSystem->GetFileList());
		}

		m_Treectrl->Expand(RootId);
	}
}

CISOProperties::~CISOProperties()
{
	if (OpenISO->GetVolumeType() == DiscIO::IVolume::GAMECUBE_DISC && pFileSystem)
		delete pFileSystem;
	delete OpenGameListItem;
	delete OpenISO;
}

size_t CISOProperties::CreateDirectoryTree(wxTreeItemId& parent, const std::vector<DiscIO::SFileInfo>& fileInfos)
{
	if (fileInfos.empty())
		return 0;
	else
		return CreateDirectoryTree(parent, fileInfos, 1, fileInfos.at(0).m_FileSize);
}

size_t CISOProperties::CreateDirectoryTree(wxTreeItemId& parent,
		const std::vector<DiscIO::SFileInfo>& fileInfos,
		const size_t _FirstIndex,
		const size_t _LastIndex)
{
	size_t CurrentIndex = _FirstIndex;

	while (CurrentIndex < _LastIndex)
	{
		const DiscIO::SFileInfo rFileInfo = fileInfos[CurrentIndex];
		std::string filePath = rFileInfo.m_FullPath;

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
		if (rFileInfo.IsDirectory())
		{
			wxTreeItemId item = m_Treectrl->AppendItem(parent, StrToWxStr(filePath), 1, 1);
			CurrentIndex = CreateDirectoryTree(item, fileInfos, CurrentIndex + 1, (size_t)rFileInfo.m_FileSize);
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

void CISOProperties::CreateGUIControls()
{
	wxButton* const EditConfig = new wxButton(this, ID_EDITCONFIG, _("Edit Config"));
	EditConfig->SetToolTip(_("This will let you manually edit the INI config file."));

	wxButton* const EditConfigDefault = new wxButton(this, ID_SHOWDEFAULTCONFIG, _("Show Defaults"));
	EditConfigDefault->SetToolTip(_("Opens the default (read-only) configuration for this game in an external text editor."));

	// Notebook
	wxNotebook* const m_Notebook = new wxNotebook(this, ID_NOTEBOOK);
	wxPanel* const m_GameConfig = new wxPanel(m_Notebook, ID_GAMECONFIG);
	m_Notebook->AddPage(m_GameConfig, _("GameConfig"));
	wxPanel* const m_VR = new wxPanel(m_Notebook, ID_VR);
	m_Notebook->AddPage(m_VR, _("VR"));
	wxPanel* const m_HideObjectPage = new wxPanel(m_Notebook, ID_HIDEOBJECT_PAGE);
	m_Notebook->AddPage(m_HideObjectPage, _("Hide Objects"));
	wxPanel* const m_PatchPage = new wxPanel(m_Notebook, ID_PATCH_PAGE);
	m_Notebook->AddPage(m_PatchPage, _("Patches"));
	wxPanel* const m_CheatPage = new wxPanel(m_Notebook, ID_ARCODE_PAGE);
	m_Notebook->AddPage(m_CheatPage, _("AR Codes"));
	m_geckocode_panel = new Gecko::CodeConfigPanel(m_Notebook);
	m_Notebook->AddPage(m_geckocode_panel, _("Gecko Codes"));
	wxPanel* const m_Information = new wxPanel(m_Notebook, ID_INFORMATION);
	m_Notebook->AddPage(m_Information, _("Info"));

	// VR
	wxBoxSizer* const sbVR = new wxBoxSizer(wxVERTICAL);
	m_VR->SetSizer(sbVR);
	//wxStaticBoxSizer * const sbVR = new wxStaticBoxSizer(wxVERTICAL, m_VR, _("Game-Specific VR Settings"));
	//sVRPage->Add(sbVR, 0, wxEXPAND | wxALL, 5);
	//wxStaticText* const OverrideTextVR = new wxStaticText(m_VR, wxID_ANY, _("These settings override core Dolphin settings.\nUndetermined means the game uses Dolphin's setting."));
	//sbVR->Add(OverrideTextVR, 0, wxEXPAND | wxALL, 5);
	wxStaticBoxSizer * const sb3D = new wxStaticBoxSizer(wxVERTICAL, m_VR, _("3D World"));
	sbVR->Add(sb3D, 0, wxEXPAND | wxALL, 5);
	// Disable3D = new wxCheckBox(m_VR, ID_DISABLE_3D, _("Disable 3D"), wxDefaultPosition, wxDefaultSize, GetElementStyle("VR", "Disable3D"));
	HudFullscreen = new wxCheckBox(m_VR, ID_HUD_FULLSCREEN, _("HUD Fullscreen"), wxDefaultPosition, wxDefaultSize, GetElementStyle("VR", "HudFullscreen"));
	HudOnTop = new wxCheckBox(m_VR, ID_HUD_ON_TOP, _("HUD On Top"), wxDefaultPosition, wxDefaultSize, GetElementStyle("VR", "HudOnTop"));
	//sb3D->Add(Disable3D, 0, wxLEFT, 5);
	sb3D->Add(HudOnTop, 0, wxLEFT, 5);
	sb3D->Add(HudFullscreen, 0, wxLEFT, 5);
	wxGridBagSizer *s3DGrid = new wxGridBagSizer();
	sb3D->Add(s3DGrid, 0, wxEXPAND);

	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Units Per Metre:")), wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	UnitsPerMetre = new wxSpinCtrlDouble(m_VR, ID_UNITS_PER_METRE, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0.0000001, 10000000, DEFAULT_VR_UNITS_PER_METRE, 0.5);
	s3DGrid->Add(UnitsPerMetre, wxGBPosition(0, 1), wxDefaultSpan, wxALL, 5);
	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("game units")), wxGBPosition(0, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("HUD Distance:")), wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	HudDistance = new wxSpinCtrlDouble(m_VR, ID_HUD_DISTANCE, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 10000, DEFAULT_VR_HUD_DISTANCE, 0.1);
	s3DGrid->Add(HudDistance, wxGBPosition(1, 1), wxDefaultSpan, wxALL, 5);
	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(1, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("HUD Thickness:")), wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	HudThickness = new wxSpinCtrlDouble(m_VR, ID_HUD_THICKNESS, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000, DEFAULT_VR_HUD_THICKNESS, 0.1);
	s3DGrid->Add(HudThickness, wxGBPosition(2, 1), wxDefaultSpan, wxALL, 5);
	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(2, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("HUD 3D Closer:")), wxGBPosition(3, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	Hud3DCloser = new wxSpinCtrlDouble(m_VR, ID_HUD_3D_CLOSER, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 1, DEFAULT_VR_HUD_3D_CLOSER, 0.1);
	s3DGrid->Add(Hud3DCloser, wxGBPosition(3, 1), wxDefaultSpan, wxALL, 5);
	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("0=Back, 1=Front")), wxGBPosition(3, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Camera Forward:")), wxGBPosition(4, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	CameraForward = new wxSpinCtrlDouble(m_VR, ID_CAMERA_FORWARD, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -10000, 10000, DEFAULT_VR_CAMERA_FORWARD, 0.1);
	s3DGrid->Add(CameraForward, wxGBPosition(4, 1), wxDefaultSpan, wxALL, 5);
	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(4, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Camera Pitch:")), wxGBPosition(5, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	CameraPitch = new wxSpinCtrlDouble(m_VR, ID_CAMERA_PITCH, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -180, 360, DEFAULT_VR_CAMERA_PITCH, 1);
	s3DGrid->Add(CameraPitch, wxGBPosition(5, 1), wxDefaultSpan, wxALL, 5);
	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Degrees Up")), wxGBPosition(5, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Aim Distance:")), wxGBPosition(6, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	AimDistance = new wxSpinCtrlDouble(m_VR, ID_AIM_DISTANCE, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 10000, DEFAULT_VR_AIM_DISTANCE, 0.1);
	s3DGrid->Add(AimDistance, wxGBPosition(6, 1), wxDefaultSpan, wxALL, 5);
	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(6, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Min HFOV:")), wxGBPosition(7, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	MinFOV = new wxSpinCtrlDouble(m_VR, ID_MIN_FOV, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 179, DEFAULT_VR_MIN_FOV, 1);
	s3DGrid->Add(MinFOV, wxGBPosition(7, 1), wxDefaultSpan, wxALL, 5);
	s3DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("degrees")), wxGBPosition(7, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	wxStaticBoxSizer * const sb2D = new wxStaticBoxSizer(wxVERTICAL, m_VR, _("2D Screens"));
	sbVR->Add(sb2D, 0, wxEXPAND | wxALL, 5);
	wxGridBagSizer *s2DGrid = new wxGridBagSizer();
	sb2D->Add(s2DGrid, 0, wxEXPAND);

	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Height:")), wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	ScreenHeight = new wxSpinCtrlDouble(m_VR, ID_SCREEN_HEIGHT, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 10000, DEFAULT_VR_SCREEN_HEIGHT, 0.1);
	s2DGrid->Add(ScreenHeight, wxGBPosition(0, 1), wxDefaultSpan, wxALL, 5);
	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(0, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Distance:")), wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	ScreenDistance = new wxSpinCtrlDouble(m_VR, ID_SCREEN_DISTANCE, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 10000, DEFAULT_VR_SCREEN_DISTANCE, 0.1);
	s2DGrid->Add(ScreenDistance, wxGBPosition(1, 1), wxDefaultSpan, wxALL, 5);
	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(1, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Thickness:")), wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	ScreenThickness = new wxSpinCtrlDouble(m_VR, ID_SCREEN_THICKNESS, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000, DEFAULT_VR_SCREEN_THICKNESS, 0.1);
	s2DGrid->Add(ScreenThickness, wxGBPosition(2, 1), wxDefaultSpan, wxALL, 5);
	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(2, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Up:")), wxGBPosition(3, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	ScreenUp = new wxSpinCtrlDouble(m_VR, ID_SCREEN_UP, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -10000, 10000, DEFAULT_VR_SCREEN_UP, 0.1);
	s2DGrid->Add(ScreenUp, wxGBPosition(3, 1), wxDefaultSpan, wxALL, 5);
	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(3, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Right:")), wxGBPosition(4, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	ScreenRight = new wxSpinCtrlDouble(m_VR, ID_SCREEN_RIGHT, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -10000, 10000, DEFAULT_VR_SCREEN_RIGHT, 0.1);
	s2DGrid->Add(ScreenRight, wxGBPosition(4, 1), wxDefaultSpan, wxALL, 5);
	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("metres")), wxGBPosition(4, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("Screen Pitch:")), wxGBPosition(5, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	ScreenPitch = new wxSpinCtrlDouble(m_VR, ID_SCREEN_PITCH, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, -180, 360, DEFAULT_VR_SCREEN_PITCH, 1);
	s2DGrid->Add(ScreenPitch, wxGBPosition(5, 1), wxDefaultSpan, wxALL, 5);
	s2DGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("degrees")), wxGBPosition(5, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	wxGridBagSizer *sVRGrid = new wxGridBagSizer();
	sbVR->Add(sVRGrid, 0, wxEXPAND);

	arrayStringFor_VRState.Add(_("Not Set"));
	arrayStringFor_VRState.Add(_("Unplayable"));
	arrayStringFor_VRState.Add(_("Bad"));
	arrayStringFor_VRState.Add(_("Playable"));
	arrayStringFor_VRState.Add(_("Good"));
	arrayStringFor_VRState.Add(_("Perfect"));
	VRState = new wxChoice(m_VR, ID_EMUSTATE, wxDefaultPosition, wxDefaultSize, arrayStringFor_VRState);
	sVRGrid->Add(new wxStaticText(m_VR, wxID_ANY, _("VR state:")), wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sVRGrid->Add(VRState, wxGBPosition(0, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	VRIssues = new wxTextCtrl(m_VR, ID_EMU_ISSUES, wxEmptyString);
	sbVR->Add(VRIssues, 0, wxEXPAND);

	// GameConfig editing - Overrides and emulation state
	wxStaticText* const OverrideText = new wxStaticText(m_GameConfig, wxID_ANY, _("These settings override core Dolphin settings.\nUndetermined means the game uses Dolphin's setting."));

	// Core
	CPUThread = new wxCheckBox(m_GameConfig, ID_USEDUALCORE, _("Enable Dual Core"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "CPUThread"));
	SkipIdle = new wxCheckBox(m_GameConfig, ID_IDLESKIP, _("Enable Idle Skipping"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "SkipIdle"));
	MMU = new wxCheckBox(m_GameConfig, ID_MMU, _("Enable MMU"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "MMU"));
	MMU->SetToolTip(_("Enables the Memory Management Unit, needed for some games. (ON = Compatible, OFF = Fast)"));
	DCBZOFF = new wxCheckBox(m_GameConfig, ID_DCBZOFF, _("Skip DCBZ clearing"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "DCBZ"));
	DCBZOFF->SetToolTip(_("Bypass the clearing of the data cache by the DCBZ instruction. Usually leave this option disabled."));
	FPRF = new wxCheckBox(m_GameConfig, ID_FPRF, _("Enable FPRF"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "FPRF"));
	FPRF->SetToolTip(_("Enables Floating Point Result Flag calculation, needed for a few games. (ON = Compatible, OFF = Fast)"));
	SyncGPU = new wxCheckBox(m_GameConfig, ID_SYNCGPU, _("Synchronize GPU thread"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "SyncGPU"));
	SyncGPU->SetToolTip(_("Synchronizes the GPU and CPU threads to help prevent random freezes in Dual Core mode. (ON = Compatible, OFF = Fast)"));
	FastDiscSpeed = new wxCheckBox(m_GameConfig, ID_DISCSPEED, _("Speed up Disc Transfer Rate"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "FastDiscSpeed"));
	FastDiscSpeed->SetToolTip(_("Enable fast disc access. This can cause crashes and other problems in some games. (ON = Fast, OFF = Compatible)"));
	DSPHLE = new wxCheckBox(m_GameConfig, ID_AUDIO_DSP_HLE, _("DSP HLE emulation (fast)"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Core", "DSPHLE"));

	wxBoxSizer* const sAudioSlowDown = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText* const AudioSlowDownText = new wxStaticText(m_GameConfig, wxID_ANY, _("Frame Rate Hack Audio Synchronization: "));
	AudioSlowDown = new wxSpinCtrlDouble(m_GameConfig, ID_AUDIOSLOWDOWN, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0.01, 10, 1, 0.01);
	AudioSlowDown->SetToolTip(_("Leave at 1 unless using a frame rate hack. Input a multiplier equivilent to the amount the frame rate has been altered. e.g. If a 30fps game is hacked to 75fps, input 2.5"));
	sAudioSlowDown->Add(AudioSlowDownText);
	sAudioSlowDown->Add(AudioSlowDown);

	wxBoxSizer* const sGPUDeterminism = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText* const GPUDeterminismText = new wxStaticText(m_GameConfig, wxID_ANY, _("Deterministic dual core: "));
	arrayStringFor_GPUDeterminism.Add(_("Not Set"));
	arrayStringFor_GPUDeterminism.Add(_("auto"));
	arrayStringFor_GPUDeterminism.Add(_("none"));
	arrayStringFor_GPUDeterminism.Add(_("fake-completion"));
	GPUDeterminism = new wxChoice(m_GameConfig, ID_GPUDETERMINISM, wxDefaultPosition, wxDefaultSize, arrayStringFor_GPUDeterminism);
	sGPUDeterminism->Add(GPUDeterminismText);
	sGPUDeterminism->Add(GPUDeterminism);

	// Wii Console
	EnableWideScreen = new wxCheckBox(m_GameConfig, ID_ENABLEWIDESCREEN, _("Enable WideScreen"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Wii", "Widescreen"));

	// Stereoscopy
	wxBoxSizer* const sDepthPercentage = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText* const DepthPercentageText = new wxStaticText(m_GameConfig, wxID_ANY, _("Depth Percentage: "));
	DepthPercentage = new wxSlider(m_GameConfig, ID_DEPTHPERCENTAGE, 100, 0, 200);
	DepthPercentage->SetToolTip(_("This value is multiplied with the depth set in the graphics configuration."));
	sDepthPercentage->Add(DepthPercentageText);
	sDepthPercentage->Add(DepthPercentage);

	wxBoxSizer* const sConvergenceMinimum = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText* const ConvergenceMinimumText = new wxStaticText(m_GameConfig, wxID_ANY, _("Convergence Minimum: "));
	ConvergenceMinimum = new wxSpinCtrl(m_GameConfig, ID_CONVERGENCEMINIMUM);
	ConvergenceMinimum->SetRange(0, INT32_MAX);
	ConvergenceMinimum->SetToolTip(_("This value is added to the convergence value set in the graphics configuration."));
	sConvergenceMinimum->Add(ConvergenceMinimumText);
	sConvergenceMinimum->Add(ConvergenceMinimum);

	MonoDepth = new wxCheckBox(m_GameConfig, ID_MONODEPTH, _("Monoscopic Shadows"), wxDefaultPosition, wxDefaultSize, GetElementStyle("Video_Stereoscopy", "StereoEFBMonoDepth"));
	MonoDepth->SetToolTip(_("Use a single depth buffer for both eyes. Needed for a few games."));

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
	sbCoreOverrides->Add(DCBZOFF, 0, wxLEFT, 5);
	sbCoreOverrides->Add(FPRF, 0, wxLEFT, 5);
	sbCoreOverrides->Add(SyncGPU, 0, wxLEFT, 5);
	sbCoreOverrides->Add(FastDiscSpeed, 0, wxLEFT, 5);
	sbCoreOverrides->Add(DSPHLE, 0, wxLEFT, 5);
	sbCoreOverrides->Add(sGPUDeterminism, 0, wxEXPAND|wxALL, 5);
	sbCoreOverrides->Add(sAudioSlowDown, 0, wxEXPAND | wxALL, 5);

	wxStaticBoxSizer * const sbWiiOverrides = new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Wii Console"));
	if (OpenISO->GetVolumeType() == DiscIO::IVolume::GAMECUBE_DISC)
	{
		sbWiiOverrides->ShowItems(false);
		EnableWideScreen->Hide();
	}
	sbWiiOverrides->Add(EnableWideScreen, 0, wxLEFT, 5);

	wxStaticBoxSizer* const sbStereoOverrides =
		new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Stereoscopy"));
	sbStereoOverrides->Add(sDepthPercentage);
	sbStereoOverrides->Add(sConvergenceMinimum);
	sbStereoOverrides->Add(MonoDepth);

	wxStaticBoxSizer * const sbGameConfig = new wxStaticBoxSizer(wxVERTICAL, m_GameConfig, _("Game-Specific Settings"));
	sbGameConfig->Add(OverrideText, 0, wxEXPAND|wxALL, 5);
	sbGameConfig->Add(sbCoreOverrides, 0, wxEXPAND);
	sbGameConfig->Add(sbWiiOverrides, 0, wxEXPAND);
	sbGameConfig->Add(sbStereoOverrides, 0, wxEXPAND);
	sConfigPage->Add(sbGameConfig, 0, wxEXPAND|wxALL, 5);
	sEmuState->Add(EmuStateText, 0, wxALIGN_CENTER_VERTICAL);
	sEmuState->Add(EmuState, 0, wxEXPAND);
	sEmuState->Add(EmuIssues, 1, wxEXPAND);
	sConfigPage->Add(sEmuState, 0, wxEXPAND|wxALL, 5);
	m_GameConfig->SetSizer(sConfigPage);

	// Hide Object Range
	wxBoxSizer* const sHideObjects = new wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer * const sbHideObjectRange = new wxStaticBoxSizer(wxVERTICAL, m_HideObjectPage, _("Hide Object Range"));
	sHideObjects->Add(sbHideObjectRange, 0, wxEXPAND | wxALL, 5);
	wxGridBagSizer *sbHideObjectRangeGrid = new wxGridBagSizer();
	sbHideObjectRange->Add(sbHideObjectRangeGrid, 0, wxEXPAND);

	sbHideObjectRangeGrid->Add(new wxStaticText(m_HideObjectPage, wxID_ANY, _("From:")), wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sbHideObjectRangeGrid->Add(new wxStaticText(m_HideObjectPage, wxID_ANY, _("To:")), wxGBPosition(0, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	U32Setting* HideObjectsStart = new U32Setting(m_HideObjectPage, _("Hide Start"), SConfig::GetInstance().skip_objects_start, 0, 100000);
	U32Setting* HideObjectsEnd = new U32Setting(m_HideObjectPage, _("Hide End"), SConfig::GetInstance().skip_objects_end, 0, 100000);
	sbHideObjectRangeGrid->Add(HideObjectsStart, wxGBPosition(0, 1), wxDefaultSpan, wxALL, 5);
	sbHideObjectRangeGrid->Add(HideObjectsEnd, wxGBPosition(0, 3), wxDefaultSpan, wxALL, 5);

#ifdef DEBUG_OBJECTS

	sbHideObjectRangeGrid->Add(new wxStaticText(m_HideObjectPage, wxID_ANY, _("From:")), wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	sbHideObjectRangeGrid->Add(new wxStaticText(m_HideObjectPage, wxID_ANY, _("To:")), wxGBPosition(1, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);

	U32Setting* HideObjectsStartTwo = new U32Setting(m_HideObjectPage, _("Hide Start Two"), SConfig::GetInstance().skip_objects_start_two, 0, 100000);
	U32Setting* HideObjectsEndTwo = new U32Setting(m_HideObjectPage, _("Hide End Two"), SConfig::GetInstance().skip_objects_end_two, 0, 100000);
	sbHideObjectRangeGrid->Add(HideObjectsStartTwo, wxGBPosition(1, 1), wxDefaultSpan, wxALL, 5);
	sbHideObjectRangeGrid->Add(HideObjectsEndTwo, wxGBPosition(1, 3), wxDefaultSpan, wxALL, 5);

#endif
	// Hide Object Code
	HideObjects = new wxCheckListBox(m_HideObjectPage, ID_HIDEOBJECTS_LIST, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_HSCROLL);
	wxBoxSizer* const sHideObjectsButtons = new wxBoxSizer(wxHORIZONTAL);
	EditHideObject = new wxButton(m_HideObjectPage, ID_EDITHIDEOBJECT, _("Edit..."));
	wxButton* const AddHideObject = new wxButton(m_HideObjectPage, ID_ADDHideObject, _("Add..."));
	RemoveHideObject = new wxButton(m_HideObjectPage, ID_REMOVEHIDEOBJECT, _("Remove"));
	EditHideObject->Enable(false);
	RemoveHideObject->Enable(false);

	wxBoxSizer* sHideObjectPage = new wxBoxSizer(wxVERTICAL);
	sHideObjects->Add(HideObjects, 1, wxEXPAND | wxALL);
	sHideObjectsButtons->Add(EditHideObject, 0, wxEXPAND | wxALL);
	sHideObjectsButtons->AddStretchSpacer();
	sHideObjectsButtons->Add(AddHideObject, 0, wxEXPAND | wxALL);
	sHideObjectsButtons->Add(RemoveHideObject, 0, wxEXPAND | wxALL);
	sHideObjects->Add(sHideObjectsButtons, 0, wxEXPAND | wxALL);
	sHideObjectPage->Add(sHideObjects, 1, wxEXPAND | wxALL, 5);
	m_HideObjectPage->SetSizer(sHideObjectPage);

	// Patches
	wxBoxSizer* const sPatches = new wxBoxSizer(wxVERTICAL);
	Patches = new wxCheckListBox(m_PatchPage, ID_PATCHES_LIST, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_HSCROLL);
	wxBoxSizer* const sPatchButtons = new wxBoxSizer(wxHORIZONTAL);
	EditPatch = new wxButton(m_PatchPage, ID_EDITPATCH, _("Edit..."));
	wxButton* const AddPatch = new wxButton(m_PatchPage, ID_ADDPATCH, _("Add..."));
	RemovePatch = new wxButton(m_PatchPage, ID_REMOVEPATCH, _("Remove"));
	EditPatch->Disable();
	RemovePatch->Disable();

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
	Cheats = new wxCheckListBox(m_CheatPage, ID_CHEATS_LIST, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_HSCROLL);
	wxBoxSizer * const sCheatButtons = new wxBoxSizer(wxHORIZONTAL);
	EditCheat = new wxButton(m_CheatPage, ID_EDITCHEAT, _("Edit..."));
	wxButton * const AddCheat = new wxButton(m_CheatPage, ID_ADDCHEAT, _("Add..."));
	RemoveCheat = new wxButton(m_CheatPage, ID_REMOVECHEAT, _("Remove"));
	EditCheat->Disable();
	RemoveCheat->Disable();

	wxBoxSizer* sCheatPage = new wxBoxSizer(wxVERTICAL);
	sCheats->Add(Cheats, 1, wxEXPAND|wxALL, 0);
	sCheatButtons->Add(EditCheat,  0, wxEXPAND|wxALL, 0);
	sCheatButtons->AddStretchSpacer();
	sCheatButtons->Add(AddCheat,  0, wxEXPAND|wxALL, 0);
	sCheatButtons->Add(RemoveCheat,  0, wxEXPAND|wxALL, 0);
	sCheats->Add(sCheatButtons, 0, wxEXPAND|wxALL, 0);
	sCheatPage->Add(sCheats, 1, wxEXPAND|wxALL, 5);
	m_CheatPage->SetSizer(sCheatPage);


	wxStaticText* const m_InternalNameText = new wxStaticText(m_Information, wxID_ANY, _("Internal Name:"));
	m_InternalName = new wxTextCtrl(m_Information, ID_NAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_GameIDText = new wxStaticText(m_Information, wxID_ANY, _("Game ID:"));
	m_GameID = new wxTextCtrl(m_Information, ID_GAMEID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_CountryText = new wxStaticText(m_Information, wxID_ANY, _("Country:"));
	m_Country = new wxTextCtrl(m_Information, ID_COUNTRY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_MakerIDText = new wxStaticText(m_Information, wxID_ANY, _("Maker ID:"));
	m_MakerID = new wxTextCtrl(m_Information, ID_MAKERID, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_RevisionText = new wxStaticText(m_Information, wxID_ANY, _("Revision:"));
	m_Revision = new wxTextCtrl(m_Information, ID_REVISION, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_DateText = new wxStaticText(m_Information, wxID_ANY, _("Apploader Date:"));
	m_Date = new wxTextCtrl(m_Information, ID_DATE, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_FSTText = new wxStaticText(m_Information, wxID_ANY, _("FST Size:"));
	m_FST = new wxTextCtrl(m_Information, ID_FST, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_MD5SumText = new wxStaticText(m_Information, wxID_ANY, _("MD5 Checksum:"));
	m_MD5Sum = new wxTextCtrl(m_Information, ID_MD5SUM, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	m_MD5SumCompute = new wxButton(m_Information, ID_MD5SUMCOMPUTE, _("Compute"));

	wxStaticText* const m_LangText = new wxStaticText(m_Information, wxID_ANY, _("Show Language:"));

	bool wii = OpenISO->GetVolumeType() != DiscIO::IVolume::GAMECUBE_DISC;
	DiscIO::IVolume::ELanguage preferred_language = SConfig::GetInstance().GetCurrentLanguage(wii);

	std::vector<DiscIO::IVolume::ELanguage> languages = OpenGameListItem->GetLanguages();
	int preferred_language_index = 0;
	for (size_t i = 0; i < languages.size(); ++i)
	{
		if (languages[i] == preferred_language)
			preferred_language_index = i;

		switch (languages[i])
		{
		case DiscIO::IVolume::LANGUAGE_JAPANESE:
			arrayStringFor_Lang.Add(_("Japanese"));
			break;
		case DiscIO::IVolume::LANGUAGE_ENGLISH:
			arrayStringFor_Lang.Add(_("English"));
			break;
		case DiscIO::IVolume::LANGUAGE_GERMAN:
			arrayStringFor_Lang.Add(_("German"));
			break;
		case DiscIO::IVolume::LANGUAGE_FRENCH:
			arrayStringFor_Lang.Add(_("French"));
			break;
		case DiscIO::IVolume::LANGUAGE_SPANISH:
			arrayStringFor_Lang.Add(_("Spanish"));
			break;
		case DiscIO::IVolume::LANGUAGE_ITALIAN:
			arrayStringFor_Lang.Add(_("Italian"));
			break;
		case DiscIO::IVolume::LANGUAGE_DUTCH:
			arrayStringFor_Lang.Add(_("Dutch"));
			break;
		case DiscIO::IVolume::LANGUAGE_SIMPLIFIED_CHINESE:
			arrayStringFor_Lang.Add(_("Simplified Chinese"));
			break;
		case DiscIO::IVolume::LANGUAGE_TRADITIONAL_CHINESE:
			arrayStringFor_Lang.Add(_("Traditional Chinese"));
			break;
		case DiscIO::IVolume::LANGUAGE_KOREAN:
			arrayStringFor_Lang.Add(_("Korean"));
			break;
		case DiscIO::IVolume::LANGUAGE_UNKNOWN:
		default:
			arrayStringFor_Lang.Add(_("Unknown"));
			break;
		}
	}
	m_Lang = new wxChoice(m_Information, ID_LANG, wxDefaultPosition, wxDefaultSize, arrayStringFor_Lang);
	m_Lang->SetSelection(preferred_language_index);
	if (arrayStringFor_Lang.size() <= 1)
		m_Lang->Disable();

	wxStaticText* const m_NameText = new wxStaticText(m_Information, wxID_ANY, _("Name:"));
	m_Name = new wxTextCtrl(m_Information, ID_SHORTNAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_MakerText = new wxStaticText(m_Information, wxID_ANY, _("Maker:"));
	m_Maker = new wxTextCtrl(m_Information, ID_MAKER, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	wxStaticText* const m_CommentText = new wxStaticText(m_Information, wxID_ANY, _("Description:"));
	m_Comment = new wxTextCtrl(m_Information, ID_COMMENT, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY);
	wxStaticText* const m_BannerText = new wxStaticText(m_Information, wxID_ANY, _("Banner:"));
	m_Banner = new wxStaticBitmap(m_Information, ID_BANNER, wxNullBitmap, wxDefaultPosition, wxSize(96, 32));

	// ISO Details
	wxGridBagSizer* const sISODetails = new wxGridBagSizer(0, 0);
	sISODetails->Add(m_InternalNameText, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sISODetails->Add(m_InternalName, wxGBPosition(0, 1), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
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
	sBannerDetails->Add(m_NameText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sBannerDetails->Add(m_Name, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
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

	if (OpenISO->GetVolumeType() != DiscIO::IVolume::WII_WAD)
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
	bool game_ini_exists = false;
	for (const std::string& ini_filename : SConfig::GetGameIniFilenames(game_id, OpenISO->GetRevision()))
	{
		if (File::Exists(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + ini_filename))
		{
			game_ini_exists = true;
			break;
		}
	}
	if (!game_ini_exists)
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
	if (bRefreshList)
		((CGameListCtrl*)GetParent())->Update();
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

	wxFileDialog dialog(this, _("Save as..."), wxGetHomeDir(&dirHome), wxString::Format("%s.png", m_GameID->GetValue().c_str()),
		wxALL_FILES_PATTERN, wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
	if (dialog.ShowModal() == wxID_OK)
	{
		m_Banner->GetBitmap().ConvertToImage().SaveFile(dialog.GetPath());
	}
	Raise();
}

void CISOProperties::OnRightClickOnTree(wxTreeEvent& event)
{
	m_Treectrl->SelectItem(event.GetItem());

	wxMenu popupMenu;

	if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 0 &&
	    m_Treectrl->GetFirstVisibleItem() != m_Treectrl->GetSelection())
	{
		popupMenu.Append(IDM_EXTRACTDIR, _("Extract Partition..."));
	}
	else if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 1)
	{
		popupMenu.Append(IDM_EXTRACTDIR, _("Extract Directory..."));
	}
	else if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 2)
	{
		popupMenu.Append(IDM_EXTRACTFILE, _("Extract File..."));
	}

	popupMenu.Append(IDM_EXTRACTALL, _("Extract All Files..."));

	if (OpenISO->GetVolumeType() != DiscIO::IVolume::WII_DISC ||
		(m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 0 &&
		m_Treectrl->GetFirstVisibleItem() != m_Treectrl->GetSelection()))
	{
		popupMenu.AppendSeparator();
		popupMenu.Append(IDM_EXTRACTAPPLOADER, _("Extract Apploader..."));
		popupMenu.Append(IDM_EXTRACTDOL, _("Extract DOL..."));
	}

	if (m_Treectrl->GetItemImage(m_Treectrl->GetSelection()) == 0 &&
		m_Treectrl->GetFirstVisibleItem() != m_Treectrl->GetSelection())
	{
		popupMenu.AppendSeparator();
		popupMenu.Append(IDM_CHECKINTEGRITY, _("Check Partition Integrity"));
	}

	PopupMenu(&popupMenu);

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

	if (OpenISO->GetVolumeType() == DiscIO::IVolume::WII_DISC)
	{
		const wxTreeItemId tree_selection = m_Treectrl->GetSelection();
		WiiPartition* partition = reinterpret_cast<WiiPartition*>(m_Treectrl->GetItemData(tree_selection));
		File.erase(0, m_Treectrl->GetItemText(tree_selection).length() + 1); // Remove "Partition x/"

		partition->FileSystem->ExportFile(WxStrToStr(File), WxStrToStr(Path));
	}
	else
	{
		pFileSystem->ExportFile(WxStrToStr(File), WxStrToStr(Path));
	}
}

void CISOProperties::ExportDir(const std::string& _rFullPath, const std::string& _rExportFolder, const WiiPartition* partition)
{
	DiscIO::IFileSystem* const fs = OpenISO->GetVolumeType() == DiscIO::IVolume::WII_DISC ? partition->FileSystem.get() : pFileSystem;

	const std::vector<DiscIO::SFileInfo>& fst = fs->GetFileList();

	u32 index = 0;
	u32 size = 0;

	// Extract all
	if (_rFullPath.empty())
	{
		index = 0;
		size = (u32)fst.size();

		fs->ExportApploader(_rExportFolder);
		if (OpenISO->GetVolumeType() != DiscIO::IVolume::WII_DISC)
			fs->ExportDOL(_rExportFolder);
	}
	else
	{
		// Look for the dir we are going to extract
		for (index = 0; index != fst.size(); ++index)
		{
			if (fst[index].m_FullPath == _rFullPath)
			{
				DEBUG_LOG(DISCIO, "Found the directory at %u", index);
				size = (u32)fst[index].m_FileSize;
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

		dialog.Update(i, wxString::Format(_("Extracting %s"), StrToWxStr(fst[i].m_FullPath)));

		if (dialog.WasCancelled())
			break;

		if (fst[i].IsDirectory())
		{
			const std::string exportName = StringFromFormat("%s/%s/", _rExportFolder.c_str(), fst[i].m_FullPath.c_str());
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
			const std::string exportName = StringFromFormat("%s/%s", _rExportFolder.c_str(), fst[i].m_FullPath.c_str());
			DEBUG_LOG(DISCIO, "%s", exportName.c_str());

			if (!File::Exists(exportName) && !fs->ExportFile(fst[i].m_FullPath, exportName))
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
		if (OpenISO->GetVolumeType() == DiscIO::IVolume::WII_DISC)
		{
			wxTreeItemIdValue cookie;
			wxTreeItemId root = m_Treectrl->GetRootItem();
			wxTreeItemId item = m_Treectrl->GetFirstChild(root, cookie);
			while (item.IsOk())
			{
				ExportDir("", WxStrToStr(Path), reinterpret_cast<WiiPartition*>(m_Treectrl->GetItemData(item)));
				item = m_Treectrl->GetNextChild(root, cookie);
			}
		}
		else
		{
			ExportDir("", WxStrToStr(Path));
		}

		return;
	}

	while (m_Treectrl->GetItemParent(m_Treectrl->GetSelection()) != m_Treectrl->GetRootItem())
	{
		wxString temp = m_Treectrl->GetItemText(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
		Directory = temp + DIR_SEP_CHR + Directory;

		m_Treectrl->SelectItem(m_Treectrl->GetItemParent(m_Treectrl->GetSelection()));
	}

	Directory += DIR_SEP_CHR;

	if (OpenISO->GetVolumeType() == DiscIO::IVolume::WII_DISC)
	{
		const wxTreeItemId tree_selection = m_Treectrl->GetSelection();
		WiiPartition* partition = reinterpret_cast<WiiPartition*>(m_Treectrl->GetItemData(tree_selection));
		Directory.erase(0, m_Treectrl->GetItemText(tree_selection).length() + 1); // Remove "Partition x/"

		ExportDir(WxStrToStr(Directory), WxStrToStr(Path), partition);
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

	if (OpenISO->GetVolumeType() == DiscIO::IVolume::WII_DISC)
	{
		WiiPartition* partition = reinterpret_cast<WiiPartition*>(m_Treectrl->GetItemData(m_Treectrl->GetSelection()));
		FS = partition->FileSystem.get();
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

	ExitCode Entry() override
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
	if (OpenISO->GetVolumeType() != DiscIO::IVolume::WII_DISC)
		return;

	wxProgressDialog dialog(_("Checking integrity..."), _("Working..."), 1000, this,
		wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_SMOOTH
	);

	WiiPartition* partition = reinterpret_cast<WiiPartition*>(m_Treectrl->GetItemData(m_Treectrl->GetSelection()));
	IntegrityCheckThread thread(*partition);
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
			wxString::Format(_("Integrity check for %s failed. The disc image is most "
			                   "likely corrupted or has been patched incorrectly."),
			                 m_Treectrl->GetItemText(m_Treectrl->GetSelection())),
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
	SetCheckboxValueFromGameini("Core", "DCBZ", DCBZOFF);
	SetCheckboxValueFromGameini("Core", "FPRF", FPRF);
	SetCheckboxValueFromGameini("Core", "SyncGPU", SyncGPU);
	SetCheckboxValueFromGameini("Core", "FastDiscSpeed", FastDiscSpeed);
	SetCheckboxValueFromGameini("Core", "DSPHLE", DSPHLE);
	SetCheckboxValueFromGameini("Wii", "Widescreen", EnableWideScreen);
	SetCheckboxValueFromGameini("Video_Stereoscopy", "StereoEFBMonoDepth", MonoDepth);

	IniFile::Section* default_video = GameIniDefault.GetOrCreateSection("Video");

	int iTemp;
	default_video->Get("ProjectionHack", &iTemp);
	default_video->Get("PH_SZNear", &m_PHack_Data.PHackSZNear);
	if (GameIniLocal.GetIfExists("Video", "PH_SZNear", &iTemp))
		m_PHack_Data.PHackSZNear = !!iTemp;
	default_video->Get("PH_SZFar", &m_PHack_Data.PHackSZFar);
	if (GameIniLocal.GetIfExists("Video", "PH_SZFar", &iTemp))
		m_PHack_Data.PHackSZFar = !!iTemp;

	std::string sTemp;
	default_video->Get("PH_ZNear", &m_PHack_Data.PHZNear);
	if (GameIniLocal.GetIfExists("Video", "PH_ZNear", &sTemp))
		m_PHack_Data.PHZNear = sTemp;
	default_video->Get("PH_ZFar", &m_PHack_Data.PHZFar);
	if (GameIniLocal.GetIfExists("Video", "PH_ZFar", &sTemp))
		m_PHack_Data.PHZFar = sTemp;

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

	sTemp = "";
	if (!GameIniLocal.GetIfExists("Core", "GPUDeterminismMode", &sTemp))
		GameIniDefault.GetIfExists("Core", "GPUDeterminismMode", &sTemp);

	if (sTemp == "")
		GPUDeterminism->SetSelection(0);
	else if (sTemp == "auto")
		GPUDeterminism->SetSelection(1);
	else if (sTemp == "none")
		GPUDeterminism->SetSelection(2);
	else if (sTemp == "fake-completion")
		GPUDeterminism->SetSelection(3);

	float fTemp;

	fTemp = 1;
	GameIniDefault.GetIfExists("Core", "AudioSlowDown", &fTemp);
	GameIniLocal.GetIfExists("Core", "AudioSlowDown", &fTemp);
	AudioSlowDown->SetValue(fTemp);

	IniFile::Section* default_stereoscopy = GameIniDefault.GetOrCreateSection("Video_Stereoscopy");
	default_stereoscopy->Get("StereoDepthPercentage", &iTemp, 100);
	GameIniLocal.GetIfExists("Video_Stereoscopy", "StereoDepthPercentage", &iTemp);
	DepthPercentage->SetValue(iTemp);
	default_stereoscopy->Get("StereoConvergenceMinimum", &iTemp, 0);
	GameIniLocal.GetIfExists("Video_Stereoscopy", "StereoConvergenceMinimum", &iTemp);
	ConvergenceMinimum->SetValue(iTemp);
	//SetCheckboxValueFromGameini("VR", "Disable3D", Disable3D);
	SetCheckboxValueFromGameini("VR", "HudFullscreen", HudFullscreen);
	SetCheckboxValueFromGameini("VR", "HudOnTop", HudOnTop);

	fTemp = DEFAULT_VR_UNITS_PER_METRE;
	if (GameIniDefault.GetIfExists("VR", "UnitsPerMetre", &fTemp))
		UnitsPerMetre->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "UnitsPerMetre", &fTemp))
		UnitsPerMetre->SetValue(fTemp);

	fTemp = DEFAULT_VR_HUD_DISTANCE;
	if (GameIniDefault.GetIfExists("VR", "HudDistance", &fTemp))
		HudDistance->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "HudDistance", &fTemp))
		HudDistance->SetValue(fTemp);

	fTemp = DEFAULT_VR_HUD_THICKNESS;
	if (GameIniDefault.GetIfExists("VR", "HudThickness", &fTemp))
		HudThickness->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "HudThickness", &fTemp))
		HudThickness->SetValue(fTemp);

	fTemp = DEFAULT_VR_HUD_3D_CLOSER;
	if (GameIniDefault.GetIfExists("VR", "Hud3DCloser", &fTemp))
		Hud3DCloser->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "Hud3DCloser", &fTemp))
		Hud3DCloser->SetValue(fTemp);

	fTemp = DEFAULT_VR_CAMERA_FORWARD;
	if (GameIniDefault.GetIfExists("VR", "CameraForward", &fTemp))
		CameraForward->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "CameraForward", &fTemp))
		CameraForward->SetValue(fTemp);

	fTemp = DEFAULT_VR_CAMERA_PITCH;
	if (GameIniDefault.GetIfExists("VR", "CameraPitch", &fTemp))
		CameraPitch->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "CameraPitch", &fTemp))
		CameraPitch->SetValue(fTemp);

	fTemp = DEFAULT_VR_AIM_DISTANCE;
	if (GameIniDefault.GetIfExists("VR", "AimDistance", &fTemp))
		AimDistance->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "AimDistance", &fTemp))
		AimDistance->SetValue(fTemp);

	fTemp = DEFAULT_VR_MIN_FOV;
	if (GameIniDefault.GetIfExists("VR", "MinFOV", &fTemp))
		MinFOV->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "MinFOV", &fTemp))
		MinFOV->SetValue(fTemp);

	fTemp = DEFAULT_VR_SCREEN_HEIGHT;
	if (GameIniDefault.GetIfExists("VR", "ScreenHeight", &fTemp))
		ScreenHeight->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "ScreenHeight", &fTemp))
		ScreenHeight->SetValue(fTemp);

	fTemp = DEFAULT_VR_SCREEN_DISTANCE;
	if (GameIniDefault.GetIfExists("VR", "ScreenDistance", &fTemp))
		ScreenDistance->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "ScreenDistance", &fTemp))
		ScreenDistance->SetValue(fTemp);

	fTemp = DEFAULT_VR_SCREEN_THICKNESS;
	if (GameIniDefault.GetIfExists("VR", "ScreenThickness", &fTemp))
		ScreenThickness->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "ScreenThickness", &fTemp))
		ScreenThickness->SetValue(fTemp);

	fTemp = DEFAULT_VR_SCREEN_UP;
	if (GameIniDefault.GetIfExists("VR", "ScreenUp", &fTemp))
		ScreenUp->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "ScreenUp", &fTemp))
		ScreenUp->SetValue(fTemp);

	fTemp = DEFAULT_VR_SCREEN_RIGHT;
	if (GameIniDefault.GetIfExists("VR", "ScreenRight", &fTemp))
		ScreenRight->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "ScreenRight", &fTemp))
		ScreenRight->SetValue(fTemp);

	fTemp = DEFAULT_VR_SCREEN_PITCH;
	if (GameIniDefault.GetIfExists("VR", "ScreenPitch", &fTemp))
		ScreenPitch->SetValue(fTemp);
	if (GameIniLocal.GetIfExists("VR", "ScreenPitch", &fTemp))
		ScreenPitch->SetValue(fTemp);

	iTemp = 0;
	GameIniDefault.GetIfExists("VR", "VRStateId", &iTemp);
	VRState->SetSelection(iTemp);
	if (GameIniLocal.GetIfExists("VR", "VRStateId", &iTemp))
		VRState->SetSelection(iTemp);

	sTemp = "";
	GameIniDefault.GetIfExists("VR", "VRIssues", &sTemp);
	if (!sTemp.empty())
		VRIssues->SetValue(StrToWxStr(sTemp));
	if (GameIniLocal.GetIfExists("VR", "VRIssues", &sTemp))
		VRIssues->SetValue(StrToWxStr(sTemp));

	HideObjectList_Load();
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
	SaveGameIniValueFrom3StateCheckbox("Core", "DCBZ", DCBZOFF);
	SaveGameIniValueFrom3StateCheckbox("Core", "FPRF", FPRF);
	SaveGameIniValueFrom3StateCheckbox("Core", "SyncGPU", SyncGPU);
	SaveGameIniValueFrom3StateCheckbox("Core", "FastDiscSpeed", FastDiscSpeed);
	SaveGameIniValueFrom3StateCheckbox("Core", "DSPHLE", DSPHLE);
	SaveGameIniValueFrom3StateCheckbox("Wii", "Widescreen", EnableWideScreen);
	SaveGameIniValueFrom3StateCheckbox("Video_Stereoscopy", "StereoEFBMonoDepth", MonoDepth);

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

	SAVE_IF_NOT_DEFAULT("Video", "PH_SZNear", (m_PHack_Data.PHackSZNear ? 1 : 0), 0);
	SAVE_IF_NOT_DEFAULT("Video", "PH_SZFar", (m_PHack_Data.PHackSZFar ? 1 : 0), 0);
	SAVE_IF_NOT_DEFAULT("Video", "PH_ZNear", m_PHack_Data.PHZNear, "");
	SAVE_IF_NOT_DEFAULT("Video", "PH_ZFar", m_PHack_Data.PHZFar, "");
	SAVE_IF_NOT_DEFAULT("EmuState", "EmulationStateId", EmuState->GetSelection(), 0);

	std::string emu_issues = EmuIssues->GetValue().ToStdString();
	SAVE_IF_NOT_DEFAULT("EmuState", "EmulationIssues", emu_issues, "");

	std::string tmp;
	if (GPUDeterminism->GetSelection() == 0)
		tmp = "Not Set";
	else if (GPUDeterminism->GetSelection() == 1)
		tmp = "auto";
	else if (GPUDeterminism->GetSelection() == 2)
		tmp = "none";
	else if (GPUDeterminism->GetSelection() == 3)
		tmp = "fake-completion";

	SAVE_IF_NOT_DEFAULT("Core", "GPUDeterminismMode", tmp, "Not Set");
	SAVE_IF_NOT_DEFAULT("Core", "AudioSlowDown", AudioSlowDown->GetValue(), 1.00f);

	int depth = DepthPercentage->GetValue() > 0 ? DepthPercentage->GetValue() : 100;
	SAVE_IF_NOT_DEFAULT("Video_Stereoscopy", "StereoDepthPercentage", depth, 100);
	SAVE_IF_NOT_DEFAULT("Video_Stereoscopy", "StereoConvergenceMinimum", ConvergenceMinimum->GetValue(), 0);
	//SaveGameIniValueFrom3StateCheckbox("VR", "Disable3D", Disable3D);
	SaveGameIniValueFrom3StateCheckbox("VR", "HudFullscreen", HudFullscreen);
	SaveGameIniValueFrom3StateCheckbox("VR", "HudOnTop", HudOnTop);
	SAVE_IF_NOT_DEFAULT("VR", "UnitsPerMetre", (float)UnitsPerMetre->GetValue(), DEFAULT_VR_UNITS_PER_METRE);
	SAVE_IF_NOT_DEFAULT("VR", "HudDistance", (float)HudDistance->GetValue(), DEFAULT_VR_HUD_DISTANCE);
	SAVE_IF_NOT_DEFAULT("VR", "HudThickness", (float)HudThickness->GetValue(), DEFAULT_VR_HUD_THICKNESS);
	SAVE_IF_NOT_DEFAULT("VR", "Hud3DCloser", (float)Hud3DCloser->GetValue(), DEFAULT_VR_HUD_3D_CLOSER);
	SAVE_IF_NOT_DEFAULT("VR", "CameraForward", (float)CameraForward->GetValue(), DEFAULT_VR_CAMERA_FORWARD);
	SAVE_IF_NOT_DEFAULT("VR", "CameraPitch", (float)CameraPitch->GetValue(), DEFAULT_VR_CAMERA_PITCH);
	SAVE_IF_NOT_DEFAULT("VR", "AimDistance", (float)AimDistance->GetValue(), DEFAULT_VR_AIM_DISTANCE);
	SAVE_IF_NOT_DEFAULT("VR", "MinFOV", (float)MinFOV->GetValue(), DEFAULT_VR_MIN_FOV);
	SAVE_IF_NOT_DEFAULT("VR", "ScreenHeight", (float)ScreenHeight->GetValue(), DEFAULT_VR_SCREEN_HEIGHT);
	SAVE_IF_NOT_DEFAULT("VR", "ScreenDistance", (float)ScreenDistance->GetValue(), DEFAULT_VR_SCREEN_DISTANCE);
	SAVE_IF_NOT_DEFAULT("VR", "ScreenThickness", (float)ScreenThickness->GetValue(), DEFAULT_VR_SCREEN_THICKNESS);
	SAVE_IF_NOT_DEFAULT("VR", "ScreenUp", (float)ScreenUp->GetValue(), DEFAULT_VR_SCREEN_UP);
	SAVE_IF_NOT_DEFAULT("VR", "ScreenRight", (float)ScreenRight->GetValue(), DEFAULT_VR_SCREEN_RIGHT);
	SAVE_IF_NOT_DEFAULT("VR", "ScreenPitch", (float)ScreenPitch->GetValue(), DEFAULT_VR_SCREEN_PITCH);
	SAVE_IF_NOT_DEFAULT("VR", "VRStateId", VRState->GetSelection(), 0);
	emu_issues = VRIssues->GetValue().ToStdString();
	SAVE_IF_NOT_DEFAULT("VR", "VRIssues", emu_issues, "");

	PatchList_Save();
	ActionReplayList_Save();
	Gecko::SaveCodes(GameIniLocal, m_geckocode_panel->GetCodes());

	bool success = GameIniLocal.Save(GameIniFileLocal);

	// If the resulting file is empty, delete it. Kind of a hack, but meh.
	if (success && File::GetSize(GameIniFileLocal) == 0)
		File::Delete(GameIniFileLocal);

	return success;
}

void CISOProperties::LaunchExternalEditor(const std::string& filename, bool wait_until_closed)
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
		return;
	}

	long result;

	if (wait_until_closed)
		result = wxExecute(OpenCommand, wxEXEC_SYNC);
	else
		result = wxExecute(OpenCommand);

	if (result == -1)
	{
		WxUtils::ShowErrorDialog(_("wxExecute returned -1 on application run!"));
		return;
	}

	if (wait_until_closed)
		bRefreshList = true; // Just in case
#endif
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
	LaunchExternalEditor(GameIniFileLocal, true);
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

// Opens all pre-defined INIs for the game. If there are multiple ones,
// they will all be opened, but there is usually only one
void CISOProperties::OnShowDefaultConfig(wxCommandEvent& WXUNUSED (event))
{
	for (const std::string& filename : SConfig::GetGameIniFilenames(game_id, OpenISO->GetRevision()))
	{
		std::string path = File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename;
		if (File::Exists(path))
			LaunchExternalEditor(path, false);
	}
}

void CISOProperties::ListSelectionChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_HIDEOBJECTS_LIST:
		if (HideObjects->GetSelection() == wxNOT_FOUND ||
			DefaultHideObjects.find(HideObjects->GetString(HideObjects->GetSelection()).ToStdString()) != DefaultHideObjects.end())
		{
			EditHideObject->Disable();
			RemoveHideObject->Disable();
		}
		else
		{
			EditHideObject->Enable();
			RemoveHideObject->Enable();
		}
		break;
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

void CISOProperties::CheckboxSelectionChanged(wxCommandEvent& event)
{
	HideObjectList_Save();
	HideObjectList_Load();
}

void CISOProperties::HideObjectList_Load()
{
	HideObjectCodes.clear();
	HideObjects->Clear();

	HideObjectEngine::LoadHideObjectSection("HideObjectCodes", HideObjectCodes, GameIniDefault, GameIniLocal);

	u32 index = 0;
	for (HideObjectEngine::HideObject& p : HideObjectCodes)
	{
		HideObjects->Append(StrToWxStr(p.name));
		HideObjects->Check(index, p.active);
		if (!p.user_defined)
			DefaultHideObjects.insert(p.name);
		++index;
	}

	HideObjectEngine::ApplyHideObjects(HideObjectCodes);

}

void CISOProperties::HideObjectList_Save()
{
	std::vector<std::string> lines;
	std::vector<std::string> enabledLines;
	u32 index = 0;
	for (HideObjectEngine::HideObject& p : HideObjectCodes)
	{
		if (HideObjects->IsChecked(index))
			enabledLines.push_back("$" + p.name);

		// Do not save default removed objects.
		if (DefaultHideObjects.find(p.name) == DefaultHideObjects.end())
		{
			lines.push_back("$" + p.name);
			for (const HideObjectEngine::HideObjectEntry& entry : p.entries)
			{
				std::string temp = StringFromFormat("%s:0x%08X%08X:0x%08X%08X", HideObjectEngine::HideObjectTypeStrings[entry.type].c_str(), (entry.value_upper & 0xffffffff00000000) >> 32, (entry.value_upper & 0xffffffff), (entry.value_lower & 0xffffffff00000000) >> 32, (entry.value_lower & 0xffffffff));
				lines.push_back(temp);
			}
		}
		++index;
	}
	GameIniLocal.SetLines("HideObjectCodes_Enabled", enabledLines);
	GameIniLocal.SetLines("HideObjectCodes", lines);
}

void CISOProperties::HideObjectButtonClicked(wxCommandEvent& event)
{
	int selection = HideObjects->GetSelection();

	switch (event.GetId())
	{
	case ID_EDITHIDEOBJECT:
	{
		CHideObjectAddEdit dlg(selection, this);
		dlg.ShowModal();
	}
	break;
	case ID_ADDHideObject:
	{
		CHideObjectAddEdit dlg(-1, this, 1, _("Add Hide Object Code"));
		if (dlg.ShowModal() == wxID_OK)
		{
			HideObjects->Append(StrToWxStr(HideObjectCodes.back().name));
			HideObjects->Check((unsigned int)(HideObjectCodes.size() - 1), HideObjectCodes.back().active);
		}
	}
	break;
	case ID_REMOVEHIDEOBJECT:
		HideObjectCodes.erase(HideObjectCodes.begin() + HideObjects->GetSelection());
		HideObjects->Delete(HideObjects->GetSelection());
		break;
	}

	HideObjectList_Save();
	HideObjects->Clear();
	HideObjectList_Load();

	HideObjectEngine::ApplyHideObjects(HideObjectCodes);

	EditHideObject->Disable();
	RemoveHideObject->Disable();
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
		CPatchAddEdit dlg(selection, &onFrame, this);
		dlg.ShowModal();
		Raise();
		}
		break;
	case ID_ADDPATCH:
		{
		CPatchAddEdit dlg(-1, &onFrame, this, 1, _("Add Patch"));
		int res = dlg.ShowModal();
		Raise();
		if (res == wxID_OK)
		{
			Patches->Append(StrToWxStr(onFrame.back().name));
			Patches->Check((unsigned int)(onFrame.size() - 1), onFrame.back().active);
		}
		}
		break;
	case ID_REMOVEPATCH:
		onFrame.erase(onFrame.begin() + Patches->GetSelection());
		Patches->Delete(Patches->GetSelection());
		break;
	}

	PatchList_Save();
	Patches->Clear();
	PatchList_Load();

	EditPatch->Disable();
	RemovePatch->Disable();
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
		CARCodeAddEdit dlg(selection, &arCodes, this);
		dlg.ShowModal();
		Raise();
		}
		break;
	case ID_ADDCHEAT:
		{
			CARCodeAddEdit dlg(-1, &arCodes, this, 1, _("Add ActionReplay Code"));
			int res = dlg.ShowModal();
			Raise();
			if (res == wxID_OK)
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

	EditCheat->Disable();
	RemoveCheat->Disable();
}

void CISOProperties::OnChangeBannerLang(wxCommandEvent& event)
{
	ChangeBannerDetails(OpenGameListItem->GetLanguages()[event.GetSelection()]);
}

void CISOProperties::ChangeBannerDetails(DiscIO::IVolume::ELanguage language)
{
	wxString const name = StrToWxStr(OpenGameListItem->GetName(language));
	wxString const comment = StrToWxStr(OpenGameListItem->GetDescription(language));
	wxString const maker = StrToWxStr(OpenGameListItem->GetCompany());

	// Updates the information shown in the window
	m_Name->SetValue(name);
	m_Comment->SetValue(comment);
	m_Maker->SetValue(maker);//dev too

	std::string filename, extension;
	SplitPath(OpenGameListItem->GetFileName(), nullptr, &filename, &extension);
	// Also sets the window's title
	SetTitle(StrToWxStr(StringFromFormat("%s%s: %s - ", filename.c_str(),
		extension.c_str(), OpenGameListItem->GetUniqueID().c_str())) + name);
}
